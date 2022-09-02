/*
 * This file is part of the GTK+ DMD 5620 Emultor.
 *
 * Copyright 2018, Seth Morabito <web@loomcom.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#if defined __APPLE__
#include <util.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#else
#include <pty.h>
#endif
#include <stdlib.h>
#include <utmp.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <termios.h>

#include "version.h"
#include "dmd_5620.h"

#define PCHAR(p)   (((p) >= 0x20 && (p) < 0x7f) ? (p) : '.')
#define TX_BUF_LEN    64

static char VERSION_STRING[64];
static GtkWidget *main_window;
static cairo_surface_t *surface = NULL;
static GdkPixbuf *pixbuf = NULL;
static pthread_t dmd_thread;
static int pty_master, pty_slave;
static char *nvram = NULL;
static uint8_t previous_vram[VIDRAM_SIZE];
static struct pollfd fds[2];
static pid_t shell_pid;
static int erase_is_delete = FALSE;
static int verbose = FALSE;
static volatile int window_beep = FALSE;
static volatile int dmd_thread_run = TRUE;
static int sigint_count = 0;
static const char *trace_file = NULL;
static int trace_on = FALSE;
static int tty_fd = -1;

static void
int_handler(int signal)
{
    if (sigint_count) {
        fprintf(stderr, "Shutting down immediately. Good bye.\n");
        exit(1);
    }

    if (main_window != NULL) {
        gtk_window_close(GTK_WINDOW(main_window));
    }

    sigint_count++;
}

static void
close_window()
{
    uint8_t buf[NVRAM_SIZE];
    FILE *fp;

    if (nvram != NULL && dmd_get_nvram(buf) == 0) {
        fp = fopen(nvram, "w+");
        if (fp == NULL) {
            fprintf(stderr, "Could not open %s for writing. Skipping\n", nvram);
        } else {
            if (fwrite(buf, NVRAM_SIZE, 1, fp) != 1) {
                fprintf(stderr, "Could not write full NVRAM file %s\n", nvram);
            }
        }
    }

    if (surface) {
        cairo_surface_destroy(surface);
    }

    dmd_thread_run = 0;
    gtk_main_quit();
}

static gboolean
configure_handler(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    if (surface) {
        cairo_surface_destroy(surface);
    }

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR,
                                                gtk_widget_get_allocated_width(widget),
                                                gtk_widget_get_allocated_height(widget));

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, WIDTH, HEIGHT);

    return TRUE;
}

static gboolean
draw_handler(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}


static gboolean
refresh_display(gpointer data)
{
    uint8_t oport;
    GtkWidget *widget = (GtkWidget *)data;
    GdkWindow *window = gtk_widget_get_window(widget);
    const struct color *fg_color;
    const struct color *bg_color;

    if (widget == NULL || !GTK_IS_WIDGET(widget)) {
        return FALSE;
    }

    if (window_beep) {
        gdk_window_beep(window);
        window_beep = FALSE;
    }

    uint8_t *vram = dmd_video_ram();

    if (memcmp(previous_vram, vram, VIDRAM_SIZE) == 0) {
        return TRUE;
    }

    memcpy(previous_vram, vram, VIDRAM_SIZE);

    guchar *p = gdk_pixbuf_get_pixels(pixbuf);
    uint32_t p_index = 0;
    int byte_width = WIDTH / 8;

    if (vram == NULL) {
        fprintf(stderr, "ERROR: Unable to access video ram!\n");
        exit(-1);
    } else {
        /* Bit 2 of the DUART output port controls whether the
         * screen is Dark-on-Light or Light-on-Dark
         */
        dmd_get_duart_output_port(&oport);

        if (oport & 0x2) {
            fg_color = &COLOR_DARK;
            bg_color = &COLOR_LIGHT;
        } else {
            fg_color = &COLOR_LIGHT;
            bg_color = &COLOR_DARK;
        }

        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < byte_width; x++) {
                uint8_t b = vram[y*byte_width + x];
                for (int i = 0; i < 8; i++) {
                    int bit = (b >> (7 - i)) & 1;
                    if (bit) {
                        p[p_index++] = fg_color->r;
                        p[p_index++] = fg_color->g;
                        p[p_index++] = fg_color->b;
                        p[p_index++] = fg_color->a;
                    } else {
                        p[p_index++] = bg_color->r;
                        p[p_index++] = bg_color->g;
                        p[p_index++] = bg_color->b;
                        p[p_index++] = bg_color->a;
                    }
                }
            }
        }
    }

    cairo_t *cr;
    cr = cairo_create(surface);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);
    cairo_fill(cr);
    cairo_destroy(cr);

    gtk_widget_queue_draw(widget);

    return TRUE;
}

static gboolean
mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
    dmd_mouse_move((uint16_t) event->x, (uint16_t) (1024 - event->y));

    return TRUE;
}

static gboolean
mouse_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    /* GDK mouse buttons are numbered 1,2,3, whereas the DMD
       expects buttons to be numbered 0,1,2, so we remap
       here. */
    uint8_t button = event->button - 1;

    switch(event->type) {
    case GDK_BUTTON_PRESS:
        dmd_mouse_down(button);
        break;
    case GDK_BUTTON_RELEASE:
        dmd_mouse_up(button);
        break;
    default:
        break;
    }

    return TRUE;
}

static void
pty_init(const char *shell)
{
    char pty_name[64];

    /* Set up our PTY */
    if (openpty(&pty_master, &pty_slave, pty_name, NULL, NULL) < 0) {
        perror("Could not open terminal pty: ");
        exit(-1);
    }

    /* Fork the shell process */

    fds[0].fd = pty_master;
    fds[0].events = POLLIN;
    fds[1].fd = pty_slave;
    fds[1].events = POLLOUT;

    shell_pid = fork();

    if (shell_pid < 0) {
        perror("Could not fork child shell: ");
        exit(-1);
    } else if (shell_pid == 0) {
        /* Child */
        char *const env[] = {"TERM=dmd", NULL};
        int retval;
        close(pty_master);

        setsid();

        if (ioctl(pty_slave, TIOCSCTTY, NULL) == -1) {
            perror("Ioctl erorr: ");
            exit(-1);
        }

        dup2(pty_slave, 0);
        dup2(pty_slave, 1);
        dup2(pty_slave, 2);
        close(pty_slave);

        if (shell) {
            retval = execle(shell, "-", NULL, env);
        } else {
            retval = execle("/bin/sh", "-", NULL, env);
        }

        /* Child process is now replaced, nothing beyond this point
           will ever be reached unless there's an error. */
        if (retval < 0) {
            perror("Could not start shell process: ");
            exit(-1);
        }
    }

    close(pty_slave);
}

/*
 * PTY implemntation of read and write polling
 */
static void
pty_io_poll()
{
    uint8_t txc;
    char tx_buf[TX_BUF_LEN];
    int b_read, i;

    if (poll(fds, 2, 100) > 0) {
        if (fds[0].revents & POLLIN) {
            b_read = read(pty_master, tx_buf, TX_BUF_LEN);

            if (b_read <= 0) {
                perror("Nothing to read from child: ");
                exit(-1);
            }

            for (i = 0; i < b_read; i++) {
                dmd_rx_char(tx_buf[i] & 0xff);
                if (verbose) {
                    printf("<--- PTY rx_char[%02d]: %02x %c\n", i, tx_buf[i] & 0xff, PCHAR(tx_buf[i]));
                }
            }
        }
    }

    i = 0;
    while (dmd_rs232_tx_poll(&txc) == 0) {
        if (verbose) {
            printf("---> PTY tx_char[%02d]: %02x %c\n", i++, txc & 0xff, PCHAR(txc));
        }
        if (write(pty_master, &txc, 1) < 0) {
            fprintf(stderr, "Error %d from write: %s\n", errno, strerror(errno));
        }
    }

}

/*
 * Open and initialize a TTY device (e.g. "/dev/ttyS0", "/dev/pts/1", etc.)
 */
static int
tty_init(int fd)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "error %d from tcgetattr", errno);
        return -1;
    }

    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = fd;
    fds[1].events = POLLOUT;

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  /* 8-bit characters */
    tty.c_iflag &= ~IGNBRK;                      /* No break */
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);

    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "error %d from tcsetattr", errno);
        return -1;
    }

    return 0;
}

static void
tty_io_poll()
{
    uint8_t txc;
    char tx_buf[TX_BUF_LEN];
    int b_read, i;

    if (poll(fds, 2, 100) > 0) {
        if (fds[0].revents & POLLIN) {

            b_read = read(tty_fd, tx_buf, TX_BUF_LEN);

            for (i = 0; i < b_read; i++) {
                dmd_rx_char(tx_buf[i] & 0xff);
                if (verbose) {
                    printf("<--- TTY rx_char[%02d]: %02x %c\n", i, tx_buf[i] & 0xff, PCHAR(tx_buf[i]));
                }
            }
        }
    }

    i = 0;
    while (dmd_rs232_tx_poll(&txc) == 0) {
        if (verbose) {
            printf("---> TTY tx_char[%02d]: %02x %c\n", i++, txc & 0xff, PCHAR(txc));
        }
        if (write(tty_fd, &txc, 1) < 0) {
            fprintf(stderr, "error %d during write: %s\n", errno, strerror(errno));
        }
    }
}

static void
tty_set_blocking(int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)  {
        fprintf(stderr, "error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "error %d setting term attributes", errno);
    }
}

#ifdef __APPLE__

#endif

/*
 * This is the main thread for stepping the DMD emulator.
 */
static void *
dmd_cpu_thread(void *thread_id)
{
    struct timespec sleep_time_req, sleep_time_rem;
    int size;
    uint8_t kbc;
    uint8_t nvram_buf[NVRAM_SIZE];
    FILE *fp;

    sleep_time_req.tv_sec = 0;
    sleep_time_req.tv_nsec = 500000;

    dmd_reset();

    /* Load NVRAM, if any */
    if (nvram != NULL) {
        fp = fopen(nvram, "r");

        /* If there's no file yet, don't load anything. */
        if (fp != NULL) {
            /* Validate the file size */
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            rewind(fp);

            if (size != NVRAM_SIZE) {
                fprintf(stderr,
                        "NVRAM file %s does not seem to be valid. Skipping.\n",
                        nvram);
            } else {
                if (fread(nvram_buf, NVRAM_SIZE, 1, fp) != 1) {
                    fprintf(stderr,
                            "Unable to read NVRAM file %s. Skipping.\n",
                            nvram);
                } else {
                    dmd_set_nvram(nvram_buf);
                }
            }
        }
    }

    while (dmd_thread_run) {
        dmd_step_loop(5000);

        if (tty_fd < 0) {
            pty_io_poll();
        } else {
            tty_io_poll();
        }

        /* Poll for output for the keyboard */
        if (dmd_kb_tx_poll(&kbc) == 0) {
            if (kbc & 0x08) {
                /* Beep! For thread safety reasons, we don't
                 * actually interct with GDK in this
                 * thread. Instead, we set a flag telling
                 * refresh_display to beep for us. */
                window_beep = TRUE;
            }
        }

        if (nanosleep(&sleep_time_req, &sleep_time_rem)) {
            perror("Unable to idle: ");
            break;
        }
    }

    pthread_exit(NULL);
}

static gboolean
keydown(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    gboolean is_ctrl = event->state & GDK_CONTROL_MASK;
    gboolean is_shift = event->state & GDK_SHIFT_MASK;
    int result;

    uint8_t c = 0;

    switch(event->keyval) {
    case GDK_KEY_VoidSymbol:
        return TRUE;
    case GDK_KEY_F1:
        c = 0xe8;
        break;
    case GDK_KEY_F2:
        c = 0xe9;
        break;
    case GDK_KEY_F3:
        c = 0xea;
        break;
    case GDK_KEY_F4:
        c = 0xeb;
        break;
    case GDK_KEY_F5:
        c = 0xec;
        break;
    case GDK_KEY_F6:
        c = 0xed;
        break;
    case GDK_KEY_F7:
        c = 0xee;
        break;
    case GDK_KEY_F8:
        c = 0xef;
        break;
    case GDK_KEY_F9:
        if (is_shift) {
            c = 0x8e;
        } else {
            c = 0xae;
        }
        break;
    case GDK_KEY_F10:
        if (trace_file != NULL) {
            if (trace_on) {
                dmd_trace_off();
                trace_on = FALSE;
            } else {
                if ((result = dmd_trace_on(trace_file))) {
                    fprintf(stderr, "Error: Cannot open trace file: %d\n", result);
                }
                trace_on = TRUE;
            }
        }
        break;
    case GDK_KEY_Escape:
        c = 0x1b;
        break;
    case GDK_KEY_Delete:
        c = 0xfe;
        break;
    case GDK_KEY_Down:
        c = 0x90;
        break;
    case GDK_KEY_Up:
        c = 0x92;
    case GDK_KEY_Right:
        c = 0xc3;
        break;
    case GDK_KEY_Left:
        c = 0xc4;
        break;
    case GDK_KEY_BackSpace:
        if (erase_is_delete) {
            c = 0x7f;
            break;
        }
    case GDK_KEY_Return:
    case GDK_KEY_Tab:
    case GDK_KEY_space:
    case GDK_KEY_exclam:
    case GDK_KEY_quotedbl:
    case GDK_KEY_numbersign:
    case GDK_KEY_dollar:
    case GDK_KEY_percent:
    case GDK_KEY_ampersand:
    case GDK_KEY_apostrophe:
    case GDK_KEY_parenleft:
    case GDK_KEY_parenright:
    case GDK_KEY_asterisk:
    case GDK_KEY_plus:
    case GDK_KEY_comma:
    case GDK_KEY_minus:
    case GDK_KEY_period:
    case GDK_KEY_slash:
    case GDK_KEY_0:
    case GDK_KEY_1:
    case GDK_KEY_2:
    case GDK_KEY_3:
    case GDK_KEY_4:
    case GDK_KEY_5:
    case GDK_KEY_6:
    case GDK_KEY_7:
    case GDK_KEY_8:
    case GDK_KEY_9:
    case GDK_KEY_colon:
    case GDK_KEY_semicolon:
    case GDK_KEY_less:
    case GDK_KEY_equal:
    case GDK_KEY_greater:
    case GDK_KEY_question:
    case GDK_KEY_quoteleft:
    case GDK_KEY_braceleft:
    case GDK_KEY_bar:
    case GDK_KEY_braceright:
    case GDK_KEY_asciitilde:
        c = (uint8_t) (event->keyval & 0xff);
        break;
    case GDK_KEY_at:
    case GDK_KEY_A:
    case GDK_KEY_B:
    case GDK_KEY_C:
    case GDK_KEY_D:
    case GDK_KEY_E:
    case GDK_KEY_F:
    case GDK_KEY_G:
    case GDK_KEY_H:
    case GDK_KEY_I:
    case GDK_KEY_J:
    case GDK_KEY_K:
    case GDK_KEY_L:
    case GDK_KEY_M:
    case GDK_KEY_N:
    case GDK_KEY_O:
    case GDK_KEY_P:
    case GDK_KEY_Q:
    case GDK_KEY_R:
    case GDK_KEY_S:
    case GDK_KEY_T:
    case GDK_KEY_U:
    case GDK_KEY_V:
    case GDK_KEY_W:
    case GDK_KEY_X:
    case GDK_KEY_Y:
    case GDK_KEY_Z:
    case GDK_KEY_bracketleft:
    case GDK_KEY_backslash:
    case GDK_KEY_bracketright:
    case GDK_KEY_asciicircum:
    case GDK_KEY_underscore:
        if (is_ctrl) {
            c = (uint8_t) ((event->keyval & 0xff) - 0x40);
        } else {
            c = (uint8_t) (event->keyval & 0xff);
        }
        break;
    case GDK_KEY_a:
    case GDK_KEY_b:
    case GDK_KEY_c:
    case GDK_KEY_d:
    case GDK_KEY_e:
    case GDK_KEY_f:
    case GDK_KEY_g:
    case GDK_KEY_h:
    case GDK_KEY_i:
    case GDK_KEY_j:
    case GDK_KEY_k:
    case GDK_KEY_l:
    case GDK_KEY_m:
    case GDK_KEY_n:
    case GDK_KEY_o:
    case GDK_KEY_p:
    case GDK_KEY_q:
    case GDK_KEY_r:
    case GDK_KEY_s:
    case GDK_KEY_t:
    case GDK_KEY_u:
    case GDK_KEY_v:
    case GDK_KEY_w:
    case GDK_KEY_x:
    case GDK_KEY_y:
    case GDK_KEY_z:
        if (is_ctrl) {
            c = (uint8_t) ((event->keyval & 0xff) - 0x60);
        } else {
            c = (uint8_t) (event->keyval & 0xff);
        }
        break;
    default:
        return TRUE;
    }

    dmd_rx_keyboard(c);

    return TRUE;
}

/* Called on startup as a callback */
static void
gtk_setup(int *argc, char ***argv)
{
    GtkWidget *drawing_area;

    gtk_init(argc, argv);

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_icon_name(GTK_WINDOW(main_window), "dmd5620");
    gtk_window_set_title(GTK_WINDOW(main_window), "AT&T DMD 5620");
    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);

    g_signal_connect(main_window, "destroy", G_CALLBACK(close_window), NULL);

    gtk_container_set_border_width(GTK_CONTAINER(main_window), 0);

    drawing_area = gtk_drawing_area_new();

    gtk_widget_set_size_request(drawing_area, 800, 1024);

    gtk_container_add(GTK_CONTAINER(main_window), drawing_area);

    /* Try for 30 fps */
    g_timeout_add(33, refresh_display, drawing_area);

    /* Signals used to handle the backing surface */
    g_signal_connect(drawing_area, "draw",
                     G_CALLBACK(draw_handler), NULL);
    g_signal_connect(drawing_area, "configure-event",
                     G_CALLBACK(configure_handler), NULL);

    /* UI signals */
    g_signal_connect(drawing_area, "button-press-event",
                     G_CALLBACK(mouse_button), NULL);
    g_signal_connect(drawing_area, "button-release-event",
                     G_CALLBACK(mouse_button), NULL);
    g_signal_connect(G_OBJECT(main_window), "key-press-event",
                     G_CALLBACK(keydown), NULL);
    g_signal_connect(drawing_area, "motion-notify-event",
                     G_CALLBACK(mouse_moved), NULL);

    gtk_widget_set_events(drawing_area,
                          gtk_widget_get_events(drawing_area)
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_KEY_PRESS_MASK
                          | GDK_POINTER_MOTION_MASK);

    gtk_widget_show_all(main_window);
}

static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"verbose", no_argument, 0, 'V'},
    {"delete", no_argument, 0, 'D'},
    {"shell", required_argument, 0, 's'},
    {"trace", required_argument, 0, 't'},
    {"device", required_argument, 0, 'd'},
    {"nvram", required_argument, 0, 'n'},
    {0, 0, 0, 0}};

void usage()
{
    printf("Usage: dmd5620 [-h] [-v] [-V] [-D] [-d DEV|-s SHELL]\\\n"
           "               [-t FILE] [-n FILE] [-- <gtk_options> ...]\n");
    printf("AT&T DMD 5620 Terminal emulator.\n\n");
    printf("-h, --help                display help and exit\n");
    printf("-v, --version             display version and exit\n");
    printf("-V, --verbose             display verbose output\n");
    printf("-D, --delete              backspace sends ^? (DEL) instead of ^H\n");
    printf("-t, --trace FILE          trace to FILE\n");
    printf("-d, --device DEV          serial port name\n");
    printf("-s, --shell SHELL         execute SHELL instead of default user shell\n");
    printf("-n, --nvram FILE          store nvram state in FILE\n");
}

int
main(int argc, char *argv[])
{
    int c, errflg = 0;
    pthread_t thread_id = 0;
    int rs;
    char *shell = NULL;
    char *device = NULL;
    struct stat sb;

    snprintf(VERSION_STRING, 64, "%d.%d.%d",
             VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    signal(SIGINT, int_handler);
    signal(SIGCHLD, int_handler);

    extern char *optarg;
    extern int optind, optopt;

    int option_index = 0;

    while ((c = getopt_long(argc, argv, "vVdh:n:t:p:s:",
                            long_options, &option_index)) != -1) {
        switch(c) {
        case 0:
            break;
        case 'h':
            usage();
            exit(0);
        case 'd':
            erase_is_delete = TRUE;
            break;
        case 'v':
            printf("Version: %s\n", VERSION_STRING);
            exit(0);
        case 'V':
            verbose = TRUE;
            break;
        case 'n':
            nvram = optarg;
            break;
        case 's':
            shell = optarg;
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'p':
            device = optarg;
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: -%c\n", optopt);
            errflg++;
            break;
        }
    }

    if (errflg) {
        usage();
        exit(1);
    }

    if (shell == NULL && device == NULL) {
        fprintf(stderr, "Either --shell or --device is required.\n");
        exit(1);
    }

    if (shell != NULL &&  device != NULL) {
        fprintf(stderr, "Cannot specify both --shell or --device. Only one is allowed.\n");
        exit(1);
    }

    if (device == NULL) {
        if (stat(shell, &sb) != 0 || (sb.st_mode & S_IXUSR) == 0) {
            fprintf(stderr, "Cannot open %s as shell, or file is not executable.\n", shell);
            exit(1);
        }
        pty_init(shell);
    } else {
        if (stat(device, &sb) != 0) {
            fprintf(stderr, "Cannot open device %s.\n", device);
            exit(1);
        }
        tty_fd = open(device, O_RDWR|O_NOCTTY|O_SYNC);
        if (tty_fd < 0) {
            fprintf(stderr, "error %d opening %s: %s\n", errno, device, strerror(errno));
            return -1;
        }
        tty_init(tty_fd);
        tty_set_blocking(tty_fd, 0); /* No blocking */
    }

    /* Set up the GTK app */
    gtk_setup(&argc, &argv);

    if ((rs = pthread_create(&dmd_thread, NULL, dmd_cpu_thread, (void *)thread_id)) != 0) {
        fprintf(stderr, "Could not create DMD cpu thread. Status=%d\n", rs);
        exit(-1);
    }

    gtk_main();

    void *join_status;
    if ((rs = pthread_join(dmd_thread, &join_status)) != 0) {
        fprintf(stderr, "Could not join DMD cpu thread. Status=%d\n", rs);
        exit(-1);
    }

    return 0;
}

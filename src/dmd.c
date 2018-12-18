#include <gtk/gtk.h>
#include <gmodule.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

#include "libtelnet.h"

#define WIDTH 800
#define HEIGHT 1024

static char telnet_buf[90];
static cairo_surface_t *surface = NULL;
static GdkPixbuf *pixbuf = NULL;
static pthread_t dmd_thread;
static int sock = -1;
static telnet_t *telnet;
static volatile int dmd_thread_run = 1;
static GQueue *telnet_rx_queue;

/* Implement a very dumb protocol. */
static const telnet_telopt_t dmd_telopts[] = {
    { TELNET_TELOPT_ECHO,      TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_TTYPE,     TELNET_WILL, TELNET_DONT },
    { TELNET_TELOPT_COMPRESS,  TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_ZMP,       TELNET_WONT, TELNET_DONT },
    { TELNET_TELOPT_MSSP,      TELNET_WONT, TELNET_DONT },
    { -1, 0, 0}
};

/* DMD functions */
extern int dmd_reset();
extern uint8_t *dmd_video_ram();
extern int dmd_step();
extern uint32_t dmd_get_pc();
extern uint8_t dmd_get_duart_output_port();
extern int dmd_rx_char(uint8_t c);
extern int dmd_rx_keyboard(uint8_t c);
extern int dmd_tx_poll(uint8_t *c);
extern int dmd_mouse_move(uint16_t x, uint16_t y);

static void
_send(int sock, const char *buffer, size_t size)
{
    int rs;

    /* send data */
    while (size > 0) {
        if ((rs = send(sock, buffer, size, 0)) == -1) {
            fprintf(stderr, "send() failed: %s\n", strerror(errno));
            exit(1);
        } else if (rs == 0) {
            fprintf(stderr, "send() unexpectedly returned 0\n");
            exit(1);
        }

        /* update pointer and size to see if we've got more to send */
        buffer += rs;
        size -= rs;
    }
}

static void
telnet_event_handler(telnet_t *telnet, telnet_event_t *ev, void *user_data)
{
    switch (ev->type) {
    case TELNET_EV_DATA:
        if (ev->data.size) {
            for (int i = 0; i < ev->data.size; i++) {
                g_queue_push_head(telnet_rx_queue,
                                  GUINT_TO_POINTER(ev->data.buffer[i]));
            }
        }
        break;
    case TELNET_EV_SEND:
        _send(sock, ev->data.buffer, ev->data.size);
        break;
    case TELNET_EV_TTYPE:
        if (ev->ttype.cmd == TELNET_TTYPE_SEND) {
            telnet_ttype_is(telnet, "dmd");
        }
        break;
    default:
        break;
    }
}

static int
dmd_telnet_connect(char *host, char *port)
{
    struct addrinfo *ai;
    struct addrinfo hints;
    struct sockaddr_in addr;
    int flags;
    int rs;

    /* Look up the host */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rs = getaddrinfo(host, port, &hints, &ai)) != 0) {
        fprintf(stderr, "Unable to resolve host name %s: %s\n",
                host, port);
        return -1;
    }


    /* Create the socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        return -1;
    }

    /* Bind the server socket */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "bind() failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Connect */
    if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
        fprintf(stderr, "connect() failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Free up the looup info */
    freeaddrinfo(ai);

    /* Set non-blocking IO */
    flags = fcntl(sock, F_GETFL, 0);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK)) {
        fprintf(stderr, "set non-blocking failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    telnet = telnet_init(dmd_telopts, telnet_event_handler, 0, &sock);

    return 0;
}

int
dmd_telnet_disconnect()
{
    if (sock < 0) {
        return -1;
    }

    telnet_free(telnet);
    shutdown(sock, 2);
    close(sock);
    sock = -1;

    return 0;
}

static void
clear_surface()
{
    cairo_t *cr;

    cr = cairo_create(surface);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    cairo_destroy(cr);
}

static gboolean
configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    printf("[configure_event_cb]\n");

    if (surface) {
        cairo_surface_destroy(surface);
    }

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR,
                                                gtk_widget_get_allocated_width(widget),
                                                gtk_widget_get_allocated_height(widget));

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, WIDTH, HEIGHT);

    clear_surface();

    return TRUE;
}


static void
close_window(void)
{
    if (surface) {
        cairo_surface_destroy(surface);
    }

    dmd_telnet_disconnect();
    dmd_thread_run = 0;
}

static gboolean
draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

static long
current_time_ms()
{
    long us;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    us = spec.tv_sec * 1e6; /* 1 million microseconds per second */
    us += round(spec.tv_nsec / 1000);

    return us;
}

static long refresh_count = 0;
static long total_time = 0;

static gboolean
refresh_display(gpointer data)
{
    refresh_count++;

    long start_time = current_time_ms();

    GtkWidget *widget = (GtkWidget *)data;

    if (widget == NULL || !GTK_IS_WIDGET(widget)) {
        return FALSE;
    }

    guchar *p = gdk_pixbuf_get_pixels(pixbuf);
    uint32_t p_index = 0;
    uint8_t *vram = dmd_video_ram();
    int byte_width = WIDTH / 8;

    if (vram == NULL) {
        printf("[WARNING] Video Ram is NULL!");
    } else {
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < byte_width; x++) {
                uint8_t b = vram[y*byte_width + x];
                for (int i = 0; i < 8; i++) {
                    int bit = (b >> (7 - i)) & 1;
                    if (bit) {
                        p[p_index++] = 0;
                        p[p_index++] = 0xff;
                        p[p_index++] = 0;
                        p[p_index++] = 0xff;
                    } else {
                        p[p_index++] = 0;
                        p[p_index++] = 0;
                        p[p_index++] = 0;
                        p[p_index++] = 0xff;
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

    long end_time = current_time_ms();

    total_time += (end_time - start_time);

    if (refresh_count % 120 == 0) {
        printf("Average refresh: %ld us (total_time=%ld us)\n",
               (total_time / refresh_count), total_time);
        refresh_count = 0;
        total_time = 0;
    }

    return TRUE;
}

static gboolean
mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
    dmd_mouse_move((uint16_t) event->x, (uint16_t) (1024 - event->y));

    return TRUE;
}

static gboolean
button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    return TRUE;
}

void *dmd_run(void *threadid)
{
    struct timespec sleep_time_req, sleep_time_rem;
    long double steps = 0;
    ssize_t read_count;
    uint8_t rx_char;
    uint8_t tx_char;
    char tx_buf[1];

    sleep_time_req.tv_sec = 0;
    sleep_time_req.tv_nsec = 1000000;

    printf("[DMD thread starting]\n");
    dmd_reset();

    while (dmd_thread_run) {
        dmd_step();

        // Stop every once in a while to poll for I/O and idle.
        if (steps++ == 25000) {
            steps = 0;

            if (!g_queue_is_empty(telnet_rx_queue)) {
                rx_char = (uint8_t)(GPOINTER_TO_UINT(g_queue_pop_tail(telnet_rx_queue)));
                dmd_rx_char(rx_char);
            }

            if (dmd_tx_poll(&tx_char) == 0) {
                tx_buf[0] = tx_char;
                telnet_send(telnet, tx_buf, 1);
            }

            /* Try getting some love from Telnet */
            if (sock >= 0) {
                read_count = recv(sock, telnet_buf, sizeof(telnet_buf), 0);

                if (read_count < 0) {
                    if (errno == EAGAIN) {
                        // No worries, try again.
                    } else {
                        printf("[DMD thread] Error reading from telnet socket. Closing connection. rc=%ld err=%s\n", read_count, strerror(errno));
                        dmd_telnet_disconnect();
                        break;
                    }
                } else {
                    telnet_recv(telnet, telnet_buf, read_count);
                }
            }

            if (nanosleep(&sleep_time_req, &sleep_time_rem)) {
                printf("[DMD thread] SLEEP FAILED.\n");
                break;
            }
        }
    }

    printf("[DMD thread exiting]\n");

    g_queue_free(telnet_rx_queue);

    pthread_exit(NULL);
}

static gboolean
keydown_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    gboolean is_ctrl = event->state & GDK_CONTROL_MASK;
    gboolean is_shift = event->state & GDK_SHIFT_MASK;

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
    case GDK_KEY_Escape:
        c = 0x1b;
        break;
    case GDK_KEY_Delete:
        c = 0xfe;
        break;
    case GDK_KEY_BackSpace:
    case GDK_KEY_Return:
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
activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window;
    GtkWidget *frame;
    GtkWidget *drawing_area;

    printf("[activate]\n");

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "AT&T DMD 5620");
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(window, "destroy", G_CALLBACK(close_window), NULL);

    gtk_container_set_border_width(GTK_CONTAINER(window), 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(window), frame);

    drawing_area = gtk_drawing_area_new();

    gtk_widget_set_size_request(drawing_area, 800, 1024);

    gtk_container_add(GTK_CONTAINER(frame), drawing_area);

    g_timeout_add(40, refresh_display, drawing_area);

    /* Signals used to handle the backing surface */
    g_signal_connect(drawing_area, "draw",
                     G_CALLBACK(draw_cb), NULL);
    g_signal_connect(drawing_area, "configure-event",
                     G_CALLBACK(configure_event_cb), NULL);

    /* UI signals */
    g_signal_connect(drawing_area, "button-press-event",
                     G_CALLBACK(button_press_event_cb), NULL);
    g_signal_connect(G_OBJECT(window), "key_press_event",
                     G_CALLBACK(keydown_event), NULL);
    g_signal_connect(drawing_area, "motion-notify-event",
                     G_CALLBACK(mouse_moved), NULL);

    gtk_widget_set_events(drawing_area,
                          gtk_widget_get_events(drawing_area)
                          | GDK_BUTTON_PRESS_MASK
                          | GDK_KEY_PRESS_MASK
                          | GDK_POINTER_MOTION_MASK);

    gtk_widget_show_all(window);
}

int
main(int argc, char *argv[])
{

    GtkApplication *app;
    int status;
    long thread_id = 0;
    char *host, *port;
    int portno;
    int rs;

    if (argc != 3) {
        fprintf(stderr, "Usage: dmd <host> <port>\n");
        exit(-1);
    }

    host = argv[1];
    port = argv[2];

    portno = atoi(port);

    if (portno < 1 || portno > 65535) {
        fprintf(stderr, "Port %d out of range (1..65535)\n", portno);
        exit(-1);
    }

    /* Initialize the telnet receive queue. */
    telnet_rx_queue = g_queue_new();

    rs = dmd_telnet_connect(host, port);
    if (rs) {
        fprintf(stderr, "Unable to connect to %s:%s.\n", host, port);
        exit(-1);
    }

    if ((rs = pthread_create(&dmd_thread, NULL, dmd_run, (void *)thread_id)) != 0) {
        fprintf(stderr, "Could not create DMD cpu thread. Status=%d\n", rs);
        exit(-1);
    }

    app = gtk_application_new("com.loomcom.dmd", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc - 2, argv + 2);

    void *join_status;
    if ((rs = pthread_join(dmd_thread, &join_status)) != 0) {
        fprintf(stderr, "Could not join DMD cpu thread. Status=%d\n", rs);
        exit(-1);
    }

    g_object_unref(app);

    return status;
}

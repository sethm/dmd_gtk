#include <gtk/gtk.h>
#include <gmodule.h>

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

#include "telnet.h"

#define WIDTH 800
#define HEIGHT 1024

static cairo_surface_t *surface = NULL;
static GdkPixbuf *pixbuf = NULL;
static pthread_t dmd_thread;

static uint8_t last_kb_char;
static uint8_t kb_pending = 0;
static volatile int dmd_thread_run = 1;

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
    printf("[close_window]\n");

    if (surface) {
        cairo_surface_destroy(surface);
    }

    printf("[close_window] Closing Telnet socket...\n");
    telnet_close();

    printf("[close_window] Terminating thread...\n");
    dmd_thread_run = 0;
}

static gboolean
draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

static gboolean
refresh_display(gpointer data)
{
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
    uint8_t buf[512];
    ssize_t read_count;
    GQueue *telnet_rx_queue = g_queue_new();
    GQueue *telnet_tx_queue = g_queue_new();
    uint8_t rx_char;
    uint8_t tx_char;
    uint8_t tx_buf[1];

    if (telnet_open()) {
        printf("[DMD thread] Could not connect to localhost:9000!\n");
    }

    sleep_time_req.tv_sec = 0;
    sleep_time_req.tv_nsec = 1000000;

    printf("[DMD thread starting]\n");
    dmd_reset();

    while (dmd_thread_run) {
        dmd_step();

        // Stop every once in a while to poll for I/O and idle.
        if (steps++ == 25000) {
            steps = 0;

            if (kb_pending && dmd_rx_keyboard(last_kb_char) == 0) {
                printf("[DMD thread] Sent char 0x%02x.\n", last_kb_char);
                kb_pending = 0;
            }

            if (!g_queue_is_empty(telnet_rx_queue)) {
                rx_char = (uint8_t)(GPOINTER_TO_UINT(g_queue_peek_tail(telnet_rx_queue)));
                if (dmd_rx_char(rx_char) == 0) {
                    printf("[rx_char] c = %02x (%c)\n", GPOINTER_TO_UINT(rx_char), GPOINTER_TO_UINT(rx_char));
                    g_queue_pop_tail(telnet_rx_queue);
                }
            }

            if (dmd_tx_poll(&tx_char) == 0) {
                g_queue_push_head(telnet_tx_queue, GUINT_TO_POINTER(tx_char));
            }

            if ((read_count = telnet_read(buf, 512)) < 0) {
                printf("[DMD thread] Error reading from telnet socket. Closing connection.\n");
                telnet_close();
                break;
            } else {
                for (int i = 0; i < read_count; i++) {
                    printf("[DMD_thread]    telnet receive: 0x%02x (%c)\n",
                           buf[i], buf[i]);
                    g_queue_push_head(telnet_rx_queue, GUINT_TO_POINTER(buf[i]));
                }
            }

            if (!g_queue_is_empty(telnet_tx_queue)) {
                tx_buf[0] = GPOINTER_TO_UINT(g_queue_pop_tail(telnet_tx_queue));
                telnet_send(tx_buf, 1);
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
    uint8_t c = 0;

    if (event->keyval & 0xff00) {
        switch(event->keyval) {
        case 0xffbe: /* F1 */
            c = 0xe8;
            break;
        case 0xffbf: /* F2 */
            c = 0xe9;
            break;
        case 0xffc0: /* F3 */
            c = 0xea;
            break;
        case 0xffc1: /* F4 */
            c = 0xeb;
            break;
        case 0xffc2: /* F5 */
            c = 0xec;
            break;
        case 0xffc3: /* F6 */
            c = 0xed;
            break;
        case 0xffc4: /* F7 */
            c = 0xee;
            break;
        case 0xffc5: /* F8 */
            c = 0xef;
            break;
        case 0xffc6: /* F9 - Setup */
            c = 0xae;
            break;
        case 0xff1b: /* ESC */
            c = 0x1b;
            break;
        case 0xffff: /* DEL */
            c = 0xfe;
            break;
        case 0xff08: /* Backspace */
            c = 0x08;
            break;
        }
    } else {
        c = (uint8_t)(event->keyval & 0xff);
    }

    if (c) {
        last_kb_char = c;
        kb_pending = 1;
    }

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

    g_timeout_add(50, refresh_display, drawing_area);

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
    int rc;

    rc = pthread_create(&dmd_thread, NULL, dmd_run, (void *)thread_id);

    if (rc) {
        printf("ERROR: Could not create main DMD cpu thread. Status=%d\n", rc);
        exit(-1);
    }

    app = gtk_application_new("com.loomcom.dmd", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    void *join_status;
    rc = pthread_join(dmd_thread, &join_status);
    if (rc) {
        printf("ERROR: Could not join thread. Status=%d\n", rc);
        exit(-1);
    }

    printf("Main: DMD thread is done.\n");

    g_object_unref(app);

    return status;
}

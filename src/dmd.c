#include <gtk/gtk.h>

#include <math.h>
#include <time.h>
#include <stdint.h>

static cairo_surface_t *surface = NULL;

extern int dmd_init();
extern int dmd_reset();
extern uint8_t *dmd_video_ram();
extern int dmd_step();
extern uint32_t dmd_get_pc();
extern uint8_t dmd_get_duart_output_port();
extern int dmd_rx_char(uint8_t c);
extern int dmd_rx_keyboard(uint8_t c);
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

    clear_surface();

    return TRUE;
}


static void
close_window(void)
{
    printf("[close_window]\n");
    gtk_main_quit();
    if (surface) {
        cairo_surface_destroy(surface);
    }
}

static gboolean
draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);

    return FALSE;
}

long get_current_time_ms()
{
    long ms;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6);

    return (s * 1000) + ms;
}

static void
refresh_display(GtkWidget *widget)
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                       TRUE,
                                       8,
                                       800,
                                       1024);

    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *p = pixels;
    uint32_t p_index = 0;
    uint8_t *vram = dmd_video_ram();

    if (vram == NULL) {
        printf("[WARNING] Video Ram is NULL!");
    } else {
        for (int y = 0; y < 1024; y++) {
            for (int x = 0; x < 100; x++) {
                // Get the byte
                uint8_t b = vram[y * 100 + x];

                for (int i = 0; i < 8; i++) {
                    uint8_t bit = (b >> (7-i)) & 1;
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

uint8_t last_kb_char;
uint8_t kb_pending = 0;

static gboolean
run_dmd(gpointer user_data)
{
    for (int i = 0; i < 40000; i++) {
        dmd_step();
    }

    // Poll.
    if (kb_pending && dmd_rx_keyboard(last_kb_char)) {
        printf("[run_dmd] Just sent char 0x%02x\n", last_kb_char);
        kb_pending = 0;
    }

    refresh_display((GtkWidget *)user_data);
    return G_SOURCE_CONTINUE;
}

static gboolean
keydown_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    last_kb_char = 0xae;
    kb_pending = 1;
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

    g_timeout_add(33, run_dmd, drawing_area);

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

    /* Hide the cursor */
    /* GdkWindow *gdk_window = gtk_widget_get_window(window); */
    /* GdkDisplay *gdk_display = gdk_display_get_default(); */

    /* gdk_window_set_cursor(gdk_window, gdk_cursor_new_for_display(gdk_display, GDK_BLANK_CURSOR)); */
}

int
main(int argc, char *argv[])
{

    GtkApplication *app;
    int status;

    dmd_init();
    dmd_reset();
    for (int i = 0; i < 1000000; i++) {
        dmd_step();
    }

    app = gtk_application_new("com.loomcom.dmd", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}

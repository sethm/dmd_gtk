#include <gtk/gtk.h>

#include <math.h>
#include <time.h>

static cairo_surface_t *surface = NULL;

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

static gboolean
button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer *data)
{
    if (surface == NULL) {
        printf("[button_press] WARNING! SURFACE IS NULL!\n");
        return FALSE;
    }

    long start_time = get_current_time_ms();

    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    printf("[button_press]  Width=%d, Height=%d\n", width, height);

    srand(time(NULL));

    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                       TRUE,
                                       8,
                                       800,
                                       1024);

    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *p;

    for (int y = 0; y < 1024; y++) {
        for (int x = 0; x < 800; x++) {
            p = pixels + y * rowstride + x * n_channels;
            if (y % 2 == 0 && x % 2 == 0) {
                p[0] = 0;
                p[1] = 0xff;
                p[2] = 0;
                p[3] = 0xff;
            } else {
                p[0] = 0xff;
                p[1] = 0xff;
                p[2] = 0xff;
                p[3] = 0xff;
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

    long end_time = get_current_time_ms();

    printf("Drawing took: %lu ms\n", end_time - start_time);

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

    // gtk_container_set_border_width(GTK_CONTAINER(window), 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(window), frame);

    drawing_area = gtk_drawing_area_new();
    /* set a minimum size */
    gtk_widget_set_size_request(drawing_area, 800, 1024);

    gtk_container_add(GTK_CONTAINER(frame), drawing_area);

    /* Signals used to handle the backing surface */
    g_signal_connect(drawing_area, "draw",
                     G_CALLBACK(draw_cb), NULL);
    g_signal_connect(drawing_area, "configure-event",
                     G_CALLBACK(configure_event_cb), NULL);

    /* UI signals */
    g_signal_connect(drawing_area, "button-press-event",
                     G_CALLBACK(button_press_event_cb), NULL);

    gtk_widget_set_events(drawing_area,
                          gtk_widget_get_events(drawing_area) | GDK_BUTTON_PRESS_MASK);

    gtk_widget_show_all(window);
}


extern unsigned char test_function();

int
main(int argc, char *argv[])
{

    GtkApplication *app;
    int status;

    printf("TEST_FUNCTION: %02x\n", test_function());

    app = gtk_application_new("com.loomcom.dmd", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}

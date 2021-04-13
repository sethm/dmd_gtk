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

#ifndef __DMD_5620_H__
#define __DMD_5620_H__

#include <stdint.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#define WIDTH 800
#define HEIGHT 1024
#define NVRAM_SIZE 2<<12
#define VIDRAM_SIZE 1024 * 100

struct color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

static const struct color COLOR_LIGHT = { 0, 255, 0, 255 };
static const struct color COLOR_DARK = { 0, 0, 0, 255 };

/* dmd_core exported functions */
extern uint8_t *dmd_video_ram();
extern int dmd_reset();
extern int dmd_step();
extern int dmd_step_loop(size_t steps);
extern int dmd_get_pc(uint32_t *pc);
extern int dmd_get_register(uint8_t reg, uint32_t *val);
extern int dmd_get_duart_output_port(uint8_t *val);
extern int dmd_rx_char(uint8_t c);
extern int dmd_rx_keyboard(uint8_t c);
extern int dmd_rs232_tx_poll(uint8_t *c);
extern int dmd_kb_tx_poll(uint8_t *c);
extern int dmd_mouse_move(uint16_t x, uint16_t y);
extern int dmd_mouse_down(uint8_t button);
extern int dmd_mouse_up(uint8_t button);
extern int dmd_set_nvram(uint8_t *buf);
extern int dmd_get_nvram(uint8_t *buf);

/* function prototypes */
static void int_handler(int signal);
/* static int tx_send(int sock, const char *buffer, size_t size); */
static void close_window();
static gboolean configure_handler(GtkWidget *widget,
                                  GdkEventConfigure *event,
                                  gpointer data);
static gboolean draw_handler(GtkWidget *widget, cairo_t *cr, gpointer data);
static gboolean refresh_display(gpointer data);
static gboolean mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean mouse_button(GtkWidget *widget, GdkEventButton *event, gpointer data);
static void *dmd_cpu_thread(void *threadid);
static gboolean keydown(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void gtk_setup(int *argc, char ***argv);

#endif

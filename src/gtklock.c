// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// gtklock application

#include <gtk/gtk.h>

#include "util.h"
#include "window.h"
#include "gtklock.h"
#include "module.h"
#include "input-inhibitor.h"

void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx == win) {
			g_array_remove_index_fast(gtklock->windows, idx);
			g_free(ctx);
			return;
		}
	}
}

void gtklock_focus_window(struct GtkLock *gtklock, struct Window* win) {
	struct Window *old = gtklock->focused_window;
	gtklock->focused_window = win;
	window_swap_focus(win, old);
	module_on_focus_change(gtklock, win, old);
}

void gtklock_update_clocks(struct GtkLock *gtklock) {
	GDateTime *time = g_date_time_new_now_local();
	if(time == NULL) return;
	if(gtklock->time) g_free(gtklock->time);
	gtklock->time = g_date_time_format(time, gtklock->time_format ? gtklock->time_format : "%R");
	g_date_time_unref(time);

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_update_clock(ctx);
	}
}

static int gtklock_update_clocks_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock *)data;
	gtklock_update_clocks(gtklock);
	return TRUE;
}

static int gtklock_idle_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock *)data;
	gtklock_idle_hide(gtklock);
	return TRUE;
}

void gtklock_idle_hide(struct GtkLock *gtklock) {
	if(!gtklock->use_idle_hide || gtklock->hidden || g_application_get_is_busy(G_APPLICATION(gtklock->app)))
		return;
	gtklock->hidden = TRUE;
	module_on_idle_hide(gtklock);

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_idle_hide(ctx);
	}
}

void gtklock_idle_show(struct GtkLock *gtklock) {
	if(gtklock->hidden) {
		gtklock->hidden = FALSE;
		module_on_idle_show(gtklock);
	}

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_idle_show(ctx);
	}

	if(!gtklock->use_idle_hide) return;
	if(gtklock->idle_hide_source > 0) g_source_remove(gtklock->idle_hide_source);
	gtklock->idle_hide_source = g_timeout_add_seconds(gtklock->idle_timeout, gtklock_idle_handler, gtklock);
}

#if GLIB_CHECK_VERSION(2, 74, 0)
	#define GTKLOCK_FLAGS G_APPLICATION_DEFAULT_FLAGS
#else
	#define GTKLOCK_FLAGS G_APPLICATION_FLAGS_NONE
#endif

struct GtkLock* create_gtklock(void) {
	struct GtkLock *gtklock = g_malloc0(sizeof(struct GtkLock));
	if(!gtklock) report_error_and_exit("Failed allocation");
	gtklock->app = gtk_application_new(NULL, GTKLOCK_FLAGS);
	gtklock->windows = g_array_new(FALSE, TRUE, sizeof(struct Window *));
	gtklock->messages = g_array_new(FALSE, TRUE, sizeof(char *));
	gtklock->errors = g_array_new(FALSE, TRUE, sizeof(char *));
	return gtklock;
}

void gtklock_activate(struct GtkLock *gtklock) {
	gtklock->draw_clock_source = g_timeout_add(1000, gtklock_update_clocks_handler, gtklock);
	gtklock_update_clocks(gtklock);
	if(gtklock->use_idle_hide) gtklock->idle_hide_source =
		g_timeout_add_seconds(gtklock->idle_timeout, gtklock_idle_handler, gtklock);
	if(gtklock->use_layer_shell) g_application_hold(G_APPLICATION(gtklock->app));
	if(gtklock->use_input_inhibit) input_inhibitor_get();
}

void gtklock_shutdown(struct GtkLock *gtklock) {
	if(gtklock->draw_clock_source > 0) {
		g_source_remove(gtklock->draw_clock_source);
		gtklock->draw_clock_source = 0;
	}
	if(gtklock->idle_hide_source > 0) {
		g_source_remove(gtklock->idle_hide_source);
		gtklock->idle_hide_source = 0;
	}
	if(gtklock->use_input_inhibit) input_inhibitor_destroy();
}

void gtklock_destroy(struct GtkLock *gtklock) {
	g_object_unref(gtklock->app);
	g_array_unref(gtklock->windows);
	g_free(gtklock);
}


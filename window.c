#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

#include "window.h"
#include "gtklock.h"

static void window_set_focus(struct Window *win, struct Window *old);

static void window_set_focus_layer_shell(struct Window *win, struct Window *old) {
	if(old != NULL) gtk_layer_set_keyboard_interactivity(GTK_WINDOW(old->window), FALSE);
	gtk_layer_set_keyboard_interactivity(GTK_WINDOW(win->window), TRUE);
}

static gboolean window_enter_notify(GtkWidget *widget, gpointer data) {
	struct Window *win = gtklock_window_by_widget(gtklock, widget);
	gtklock_focus_window(gtklock, win);
	return FALSE;
}

static void window_setup_layershell(struct Window *ctx) {
	gtk_widget_add_events(ctx->window, GDK_ENTER_NOTIFY_MASK);
	if(ctx->enter_notify_handler > 0) {
		g_signal_handler_disconnect(ctx->window, ctx->enter_notify_handler);
		ctx->enter_notify_handler = 0;
	}
	ctx->enter_notify_handler = g_signal_connect(ctx->window, "enter-notify-event", G_CALLBACK(window_enter_notify), NULL);

	gtk_layer_init_for_window(GTK_WINDOW(ctx->window));
	gtk_layer_set_layer(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_monitor(GTK_WINDOW(ctx->window), ctx->monitor);
	gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(ctx->window));
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
}

void window_update_clock(struct Window *ctx) {
	char time[48];
	int size = 96000;
	if(gtklock->focused_window == NULL || ctx == gtklock->focused_window) size = 32000;
	g_snprintf(time, 48, "<span size='%d'>%s</span>", size, gtklock->time);
	gtk_label_set_markup((GtkLabel*)ctx->clock_label, time);
}

static void window_empty(struct Window *ctx) {
	if(ctx->revealer != NULL) {
		gtk_widget_destroy(ctx->revealer);
		ctx->revealer = NULL;
	}

	ctx->window_box = NULL;
	ctx->clock_label = NULL;
	ctx->body = NULL;
	ctx->input_box = NULL;
	ctx->input_field = NULL;
}

static void window_setup_input(struct Window *ctx) {
		if(ctx->input_box != NULL) {
			gtk_widget_destroy(ctx->input_box);
			ctx->input_box = NULL;
		}
		ctx->input_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_add(GTK_CONTAINER(ctx->body), ctx->input_box);

		GtkWidget *question_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_widget_set_halign(question_box, GTK_ALIGN_END);
		gtk_container_add(GTK_CONTAINER(ctx->input_box), question_box);

		GtkWidget *label = gtk_label_new("Password:");
		gtk_widget_set_halign(label, GTK_ALIGN_END);
		gtk_container_add(GTK_CONTAINER(question_box), label);

		ctx->input_field = gtk_entry_new();
		gtk_entry_set_input_purpose((GtkEntry*)ctx->input_field, GTK_INPUT_PURPOSE_PASSWORD);
		gtk_entry_set_visibility((GtkEntry*)ctx->input_field, FALSE);
		//g_signal_connect(ctx->input_field, "activate", G_CALLBACK(action_answer_question), ctx);
		gtk_widget_set_size_request(ctx->input_field, 384, -1);
		gtk_widget_set_halign(ctx->input_field, GTK_ALIGN_END);
		gtk_container_add(GTK_CONTAINER(question_box), ctx->input_field);

		GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_widget_set_halign(button_box, GTK_ALIGN_END);
		gtk_container_add(GTK_CONTAINER(ctx->input_box), button_box);

		GtkWidget *unlock_button = gtk_button_new_with_label("Unlock");
		GtkStyleContext *unlock_button_style = gtk_widget_get_style_context(unlock_button);
		//g_signal_connect(unlock_button, "clicked", G_CALLBACK(action_answer_question), ctx);
		gtk_style_context_add_class(unlock_button_style, "suggested-action");
		gtk_widget_set_halign(unlock_button, GTK_ALIGN_END);
		gtk_container_add(GTK_CONTAINER(button_box), unlock_button);

		gtk_widget_show_all(ctx->window);

		if(ctx->input_field != NULL) gtk_widget_grab_focus(ctx->input_field);
}

static void window_setup(struct Window *ctx) {
	// Create general structure if it is missing
	if(ctx->revealer == NULL) {
		ctx->revealer = gtk_revealer_new();
		g_object_set(ctx->revealer, "margin-bottom", 100, NULL);
		g_object_set(ctx->revealer, "margin-top", 100, NULL);
		g_object_set(ctx->revealer, "margin-left", 100, NULL);
		g_object_set(ctx->revealer, "margin-right", 100, NULL);
		gtk_widget_set_valign(ctx->revealer, GTK_ALIGN_CENTER);
		gtk_container_add(GTK_CONTAINER(ctx->window), ctx->revealer);
		gtk_revealer_set_transition_type(GTK_REVEALER(ctx->revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
		gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->revealer), FALSE);
		gtk_revealer_set_transition_duration(GTK_REVEALER(ctx->revealer), 750);
	}

	if(ctx->window_box == NULL) {
		ctx->window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_name(ctx->window_box, "window");
		gtk_container_add(GTK_CONTAINER(ctx->revealer), ctx->window_box);

		ctx->clock_label = gtk_label_new("");
		gtk_widget_set_name(ctx->clock_label, "clock");
		g_object_set(ctx->clock_label, "margin-bottom", 10, NULL);
		gtk_container_add(GTK_CONTAINER(ctx->window_box), ctx->clock_label);
		window_update_clock(ctx);
	}

	// Update input area if necessary
	if(gtklock->focused_window == ctx || gtklock->focused_window == NULL) {
		if(ctx->body == NULL) {
			ctx->body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
			gtk_widget_set_halign(ctx->body, GTK_ALIGN_CENTER);
			gtk_widget_set_name(ctx->body, "body");
			gtk_widget_set_size_request(ctx->body, 384, -1);
			gtk_container_add(GTK_CONTAINER(ctx->window_box), ctx->body);
			window_update_clock(ctx);
		}
		window_setup_input(ctx);
	} else if(ctx->body != NULL) {
		gtk_widget_destroy(ctx->body);
		ctx->body = NULL;
		ctx->input_box = NULL;
		ctx->input_field = NULL;
		window_update_clock(ctx);
	}

	if(ctx->revealer != NULL) {
		gtk_revealer_set_transition_type(GTK_REVEALER(ctx->revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
		gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->revealer), TRUE);
	}
}

static void window_destroy_notify(GtkWidget *widget, gpointer data) {
	window_empty(gtklock_window_by_widget(gtklock, widget));
	gtklock_remove_window_by_widget(gtklock, widget);
}

static void window_set_focus(struct Window *win, struct Window *old) {
	assert(win != NULL);
	window_setup(win);

	if(old != NULL && old != win) {
		if(old->input_field != NULL && win->input_field != NULL) {
			// Get previous cursor position
			gint cursor_pos = 0;
			g_object_get((GtkEntry*)old->input_field, "cursor-position", &cursor_pos, NULL);

			// Move content
			gtk_entry_set_text((GtkEntry*)win->input_field, gtk_entry_get_text((GtkEntry*)old->input_field));
			gtk_entry_set_text((GtkEntry*)old->input_field, "");

			// Update new cursor position
			g_signal_emit_by_name((GtkEntry*)win->input_field, "move-cursor", GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
			g_signal_emit_by_name((GtkEntry*)win->input_field, "move-cursor", GTK_MOVEMENT_LOGICAL_POSITIONS, cursor_pos, FALSE);
		}
		window_setup(old);
		gtk_widget_show_all(old->window);
	}
	gtk_widget_show_all(win->window);
}

void window_swap_focus(struct Window *win, struct Window *old) {
	if(gtklock->use_layer_shell) window_set_focus_layer_shell(win, old);
	window_set_focus(win, old);
}

void window_configure(struct Window *w) {
	if(gtklock->use_layer_shell) window_setup_layershell(w);
	window_setup(w);
	gtk_widget_show_all(w->window);
}


static gboolean window_background(GtkWidget *widget, cairo_t *cr, gpointer data) {
	gdk_cairo_set_source_pixbuf(cr, gtklock->background, 0, 0);
	cairo_paint(cr);
	return FALSE;
}

struct Window *create_window(GdkMonitor *monitor) {
	struct Window *w = calloc(1, sizeof(struct Window));
	if(w == NULL) {
		fprintf(stderr, "failed to allocate Window instance\n");
		exit(1);
	}
	w->monitor = monitor;
	g_array_append_val(gtklock->windows, w);

	w->window = gtk_application_window_new(gtklock->app);
	g_signal_connect(w->window, "destroy", G_CALLBACK(window_destroy_notify), NULL);
	gtk_window_set_title(GTK_WINDOW(w->window), "Lockscreen");
	gtk_window_set_default_size(GTK_WINDOW(w->window), 200, 200);

	if(gtklock->background != NULL) {
		gtk_widget_set_app_paintable(w->window, TRUE);
		g_signal_connect(w->window, "draw", G_CALLBACK(window_background), NULL);
	}
	return w;
}
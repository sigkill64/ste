#include <gtk/gtk.h>
#include <stdio.h>
#include <vte/vte.h>

#include "config.h"
#include "color.h"

static const gchar *colors[] = {
    BLACK,        RED,           GREEN,       YELLOW,         BLUE,
    MAGENTA,      CYAN,          WHITE,       BRIGHT_BLACK,   BRIGHT_RED,
    BRIGHT_GREEN, BRIGHT_YELLOW, BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN,
    BRIGHT_WHITE};

size_t get_color_pallete(GdkRGBA **gcolors_p) {
  const size_t colors_count = sizeof(colors) / sizeof(colors[0]);
  *gcolors_p = calloc(sizeof(GdkRGBA), colors_count);

  if (*gcolors_p == NULL) {
    fprintf(stderr, "failed to allocate memory for GdkRGBA");
    return -1;
  }

  for (size_t i = 0; i < colors_count; i++) {
    GdkRGBA rgba;
    fprintf(stdout, "[%li] Parsing: %s\n", i, colors[i]);
    if (!gdk_rgba_parse(&rgba, colors[i])) {
      fprintf(stderr, "failed to parse color: %s\n", colors[i]);
      return -1;
    }
    (*gcolors_p)[i] = rgba;
  }

  return colors_count;
}

void spawn_cb(VteTerminal *terminal, GPid pid, GError *error,
              gpointer user_data) {
  if (error != NULL && pid == -1) {
    g_printerr("failed to launch shell: %s\n", error->message);
  } else {
    fprintf(stderr, "Shell launched: %i\n", pid);
  }
}

gboolean accel_copy_clipboard(GtkAccelGroup *accel_group,
                              GObject *acceleratable, guint keyval,
                              GdkModifierType modifier) {
  VteTerminal *terminal =
      g_object_get_data(G_OBJECT(acceleratable), "terminal");
  vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);

  return TRUE;
}

gboolean accel_paste_clipboard(GtkAccelGroup *accel_group,
                               GObject *acceleratable, guint keyval,
                               GdkModifierType modifier) {
  VteTerminal *terminal =
      g_object_get_data(G_OBJECT(acceleratable), "terminal");
  vte_terminal_paste_clipboard(terminal);

  return TRUE;
}

int main(int argc, char **argv) {
  GtkWidget *window, *terminal;

  GdkRGBA *colors = NULL;
  GdkRGBA color;
  const size_t colors_count = get_color_pallete(&colors);

  gtk_init(&argc, &argv);
  terminal = vte_terminal_new();
  vte_terminal_set_colors(VTE_TERMINAL(terminal), NULL, NULL, colors,
                          colors_count);
  free(colors);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "ste");
  gtk_widget_set_opacity(GTK_WIDGET(window), BACKGROUND_OPACITY);

  gdk_rgba_parse(&color, FOREGROUND_COLOR);
  vte_terminal_set_color_foreground(VTE_TERMINAL(terminal), &color);
  gdk_rgba_parse(&color, BACKGROUND_COLOR);
  vte_terminal_set_color_background(VTE_TERMINAL(terminal), &color);

  vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), SCROLLBACK_SIZE);
  vte_terminal_set_allow_hyperlink(VTE_TERMINAL(terminal), TRUE);
  vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
  // vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(terminal),
  // VTE_CURSOR_BLINK_OFF);

  /* Connect some signals */
  g_signal_connect(window, "delete-event", gtk_main_quit, NULL);
  g_signal_connect(terminal, "child-exited", gtk_main_quit, NULL);

  /* Hook the custom accelerators into the application */
  GtkAccelGroup *accelg = gtk_accel_group_new();
  /* copy and paste CLIPBOARD */
  gtk_accel_group_connect(accelg, gdk_keyval_from_name(KEYVAL_COPY_CLIPBOARD),
                          MODIFIER_MASK_COMMON, // key and mask
                          GTK_ACCEL_LOCKED,     // flags
                          g_cclosure_new(G_CALLBACK(accel_copy_clipboard),
                                         terminal, NULL) // callback
  );
  gtk_accel_group_connect(accelg, gdk_keyval_from_name(KEYVAL_PASTE_CLIPBOARD),
                          MODIFIER_MASK_COMMON, // key and mask
                          GTK_ACCEL_LOCKED,     // flags
                          g_cclosure_new(G_CALLBACK(accel_paste_clipboard),
                                         terminal, NULL) // callback
  );

  gtk_window_add_accel_group(GTK_WINDOW(window), accelg);

  /* Set font */
  PangoFontDescription *font_description = pango_font_description_new();
  pango_font_description_set_family_static(font_description,
                                           "WenQuanYi Micro Hei Mono");
  pango_font_description_set_size(font_description, 13 * PANGO_SCALE);
  vte_terminal_set_font(VTE_TERMINAL(terminal), font_description);
  pango_font_description_free(font_description);

  // attach the terminal as private data to the window
  g_object_set_data(G_OBJECT(window), "terminal", terminal);

  /* Start a new shell */
  gchar **envp = g_get_environ();
  gchar **command =
      (gchar *[]){g_strdup(g_environ_getenv(envp, "SHELL")), NULL};
  g_strfreev(envp);

  vte_terminal_spawn_async(VTE_TERMINAL(terminal), VTE_PTY_DEFAULT,
                           NULL,            // working directory
                           command,         // command
                           NULL,            // environment
                           G_SPAWN_DEFAULT, // spawn flags
                           NULL, NULL,      // child setup
                           NULL,            // child pid
                           -1,              // timeout
                           NULL,            // cancellable
                           spawn_cb,        // callback
                           NULL);           // user_data

  /* Put widgets together and run the main loop */
  gtk_container_add(GTK_CONTAINER(window), terminal);
  gtk_widget_show_all(window);
  gtk_main();
}

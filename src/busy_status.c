#include "common.h"
#include "shared.h"
#include "busy_status.h"

static GtkWidget *busy_window = NULL;
static GtkWidget *busy_label = NULL;
static GtkWidget *busy_progress = NULL;
static guint busy_pulse_source = 0;
static gboolean load_busy = FALSE;
static gboolean sweep_busy = FALSE;

static gboolean busy_pulse(gpointer unused)
{
  (void)unused;
  if( busy_progress == NULL ) return G_SOURCE_REMOVE;
  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(busy_progress));
  return G_SOURCE_CONTINUE;
}

static void busy_window_destroyed(GtkWidget *widget, gpointer unused)
{
  (void)widget; (void)unused;
  busy_window = NULL;
  busy_label = NULL;
  busy_progress = NULL;
  busy_pulse_source = 0;
}

static void busy_create(void)
{
  GtkWidget *box, *spinner, *title, *detail;

  if( busy_window != NULL ) return;
  busy_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(busy_window), _("xnec2c is calculating"));
  gtk_window_set_modal(GTK_WINDOW(busy_window), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW(busy_window), TRUE);
  gtk_window_set_deletable(GTK_WINDOW(busy_window), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(busy_window), FALSE);
  gtk_window_set_default_size(GTK_WINDOW(busy_window), 460, 180);
  gtk_window_set_position(GTK_WINDOW(busy_window), GTK_WIN_POS_CENTER_ON_PARENT);
  if( main_window != NULL )
    gtk_window_set_transient_for(GTK_WINDOW(busy_window), GTK_WINDOW(main_window));

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_container_set_border_width(GTK_CONTAINER(box), 24);
  gtk_container_add(GTK_CONTAINER(busy_window), box);

  spinner = gtk_spinner_new();
  gtk_widget_set_size_request(spinner, 48, 48);
  gtk_spinner_start(GTK_SPINNER(spinner));
  gtk_box_pack_start(GTK_BOX(box), spinner, FALSE, FALSE, 0);

  title = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(title),
      _("<span size=\"x-large\" weight=\"bold\">Please wait</span>"));
  gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

  busy_label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(box), busy_label, FALSE, FALSE, 0);

  busy_progress = gtk_progress_bar_new();
  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(busy_progress), 0.08);
  gtk_box_pack_start(GTK_BOX(box), busy_progress, FALSE, FALSE, 0);

  detail = gtk_label_new(_("Controls are temporarily disabled to protect the calculation."));
  gtk_label_set_line_wrap(GTK_LABEL(detail), TRUE);
  gtk_box_pack_start(GTK_BOX(box), detail, FALSE, FALSE, 0);

  g_signal_connect(busy_window, "destroy", G_CALLBACK(busy_window_destroyed), NULL);
  gtk_widget_show_all(busy_window);
  busy_pulse_source = g_timeout_add(90, busy_pulse, NULL);
}

static void busy_refresh(void)
{
  if( load_busy || sweep_busy )
  {
    busy_create();
    gtk_label_set_text(GTK_LABEL(busy_label), sweep_busy
        ? _("Calculating frequency sweep and generating graphs...")
        : _("Loading and validating the antenna model..."));
    if( main_window != NULL ) gtk_widget_set_sensitive(main_window, FALSE);
    gtk_window_present(GTK_WINDOW(busy_window));
  }
  else
  {
    if( main_window != NULL ) gtk_widget_set_sensitive(main_window, TRUE);
    if( busy_pulse_source != 0 )
    {
      g_source_remove(busy_pulse_source);
      busy_pulse_source = 0;
    }
    if( busy_window != NULL ) gtk_widget_destroy(busy_window);
  }
}

void busy_status_load_begin(void)
{
  load_busy = TRUE;
  busy_refresh();
  while( g_main_context_iteration(NULL, FALSE) ) {}
}

void busy_status_load_end(void)
{
  load_busy = FALSE;
  busy_refresh();
}

void busy_status_sweep_begin(void)
{
  sweep_busy = TRUE;
  busy_refresh();
}

void busy_status_sweep_end(void)
{
  sweep_busy = FALSE;
  busy_refresh();
}

static void busy_sweep_end_on_main(gpointer unused)
{
  (void)unused;
  busy_status_sweep_end();
}

void busy_status_sweep_end_async(void)
{
  g_idle_add_once(busy_sweep_end_on_main, NULL);
}

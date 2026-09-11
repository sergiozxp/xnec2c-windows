/* Friendly shortcuts for the NEC FR, GN and wire-conductivity LD cards. */

#include "quick_setup.h"
#include "shared.h"
#include "windows_native.h"

#include <math.h>

typedef enum {
  CARD_FREQUENCY,
  CARD_GROUND,
  CARD_MATERIAL
} card_change_t;

typedef struct {
  const char *label;
  int ground_type;
  double dielectric;
  double conductivity;
} ground_preset_t;

typedef struct {
  const char *label;
  double conductivity;
  gboolean user_value;
} material_preset_t;

static const ground_preset_t ground_presets[] = {
  { "Real average", 2, 13.0, 0.005 },
  { "Real dry",     2,  5.0, 0.001 },
  { "Real wet",     2, 20.0, 0.030 },
  { "Perfect",      1,  0.0, 0.0   },
  { "Free space",  -1,  0.0, 0.0   }
};

/* NEC models surface-current loss. Wire and pipe therefore use the same
 * bulk conductivity when the pipe wall is thicker than several skin depths;
 * separate entries keep the familiar antenna-material vocabulary. */
static const material_preset_t material_presets[] = {
  { "0. No Loss",    0.0,       FALSE },
  { "1. Cu wire",    5.80e7,    FALSE },
  { "2. Cu pipe",    5.80e7,    FALSE },
  { "3. Al wire",    3.50e7,    FALSE },
  { "4. Al pipe",    3.50e7,    FALSE },
  { "5. Fe wire",    1.00e7,    FALSE },
  { "6. Fe pipe",    1.00e7,    FALSE },
  { "7. User wire",  5.80e7,    TRUE  },
  { "8. User pipe",  5.80e7,    TRUE  }
};

static gboolean line_is_card(const char *line, gsize length, const char *card)
{
  while( length > 0 && (*line == ' ' || *line == '\t') )
  {
    line++;
    length--;
  }
  return length >= 2 && line[0] == card[0] && line[1] == card[1]
      && (length == 2 || g_ascii_isspace(line[2]));
}

static gboolean line_is_ld5(const char *line, gsize length)
{
  char *end = NULL;
  long type;

  if( !line_is_card(line, length, "LD") ) return FALSE;
  line += 2;
  while( *line == ' ' || *line == '\t' ) line++;
  type = strtol(line, &end, 10);
  return end != line && type == 5;
}

static gboolean setup_ready(void)
{
  if( rc_config.input_file[0] == '\0' )
  {
    Notice(GTK_BUTTONS_OK, _("Quick setup"),
        _("Open a saved NEC antenna file before changing its setup."));
    return FALSE;
  }
  if( nec2_edit_window != NULL )
  {
    Notice(GTK_BUTTONS_OK, _("Quick setup"),
        _("Close the NEC2 Editor before using Setup."));
    return FALSE;
  }
  if( isFlagSet(INPUT_PENDING) )
  {
    Notice(GTK_BUTTONS_OK, _("Quick setup"),
        _("Wait for the current calculation to finish."));
    return FALSE;
  }
  return TRUE;
}

static gboolean rewrite_card(card_change_t change, const char *replacement)
{
  gchar *contents = NULL;
  gsize length = 0;
  const gchar *cursor, *end;
  const gchar *newline;
  GString *output;
  GError *error = NULL;
  gboolean inserted = FALSE, new_file = FALSE;

  if( !setup_ready() ) return FALSE;
  if( !g_file_get_contents(rc_config.input_file, &contents, &length, &error) )
  {
    Notice(GTK_BUTTONS_OK, _("Quick setup"), "%s", error->message);
    g_clear_error(&error);
    return FALSE;
  }

  newline = strstr(contents, "\r\n") != NULL ? "\r\n" : "\n";
  output = g_string_sized_new(length + 128);
  cursor = contents;
  while( *cursor != '\0' )
  {
    gsize line_len, full_len;
    gboolean target = FALSE;

    end = strchr(cursor, '\n');
    line_len = end != NULL ? (gsize)(end - cursor) : strlen(cursor);
    if( line_len > 0 && cursor[line_len - 1] == '\r' ) line_len--;
    full_len = end != NULL ? (gsize)(end - cursor + 1) : strlen(cursor);

    if( change == CARD_FREQUENCY )
      target = line_is_card(cursor, line_len, "FR");
    else if( change == CARD_GROUND )
      target = line_is_card(cursor, line_len, "GN")
          || line_is_card(cursor, line_len, "GD");
    else
      target = line_is_ld5(cursor, line_len);

    if( target )
    {
      if( !inserted && replacement != NULL )
      {
        g_string_append(output, replacement);
        g_string_append(output, newline);
        inserted = TRUE;
      }
    }
    else
    {
      g_string_append_len(output, cursor, full_len);
      if( !inserted && replacement != NULL
          && line_is_card(cursor, line_len, "GE") )
      {
        g_string_append(output, replacement);
        g_string_append(output, newline);
        inserted = TRUE;
      }
    }
    if( end == NULL ) break;
    cursor = end + 1;
  }

  if( replacement != NULL && !inserted )
  {
    g_string_free(output, TRUE);
    g_free(contents);
    Notice(GTK_BUTTONS_OK, _("Quick setup"),
        _("The NEC file has no GE termination card."));
    return FALSE;
  }

  Close_File(&input_fp);
  if( !g_file_set_contents(rc_config.input_file, output->str,
        (gssize)output->len, &error) )
  {
    Notice(GTK_BUTTONS_OK, _("Quick setup"), "%s", error->message);
    g_clear_error(&error);
    g_string_free(output, TRUE);
    g_free(contents);
    Open_Input_File(&new_file);
    return FALSE;
  }

  g_string_free(output, TRUE);
  g_free(contents);
  Open_Input_File(&new_file);
  return TRUE;
}

static GtkWidget *number_spin(double value, double lower, double upper,
    double step, guint digits)
{
  GtkAdjustment *adj = gtk_adjustment_new(value, lower, upper, step,
      step * 10.0, 0.0);
  GtkWidget *spin = gtk_spin_button_new(adj, step, digits);
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
  gtk_widget_set_hexpand(spin, TRUE);
  return spin;
}

static void attach_row(GtkGrid *grid, int row, const char *label,
    GtkWidget *widget, const char *unit)
{
  GtkWidget *text = gtk_label_new(label);
  gtk_widget_set_halign(text, GTK_ALIGN_START);
  gtk_grid_attach(grid, text, 0, row, 1, 1);
  gtk_grid_attach(grid, widget, 1, row, 1, 1);
  text = gtk_label_new(unit);
  gtk_widget_set_halign(text, GTK_ALIGN_START);
  gtk_grid_attach(grid, text, 2, row, 1, 1);
}

static void on_frequency_activate(GtkMenuItem *item, gpointer unused)
{
  GtkWidget *dialog, *content, *grid, *start, *stop, *points;
  gint response;
  double default_start = 1.0, default_stop = 30.0;
  (void)item; (void)unused;
  if( !setup_ready() ) return;

  if( calc_data.FR_cards > 0 && calc_data.freq_loop_data != NULL )
  {
    default_start = calc_data.freq_loop_data[0].min_freq;
    default_stop = calc_data.freq_loop_data[calc_data.FR_cards - 1].max_freq;
  }

  dialog = gtk_dialog_new_with_buttons(_("Frequency sweep"),
      GTK_WINDOW(main_window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_APPLY, NULL);
  content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  grid = gtk_grid_new();
  gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
  gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);
  start = number_spin(default_start, 0.000001, 1000000.0, 0.1, 6);
  stop = number_spin(default_stop, 0.000001, 1000000.0, 0.1, 6);
  points = number_spin(calc_data.steps_total > 1 ? calc_data.steps_total : 101,
      1.0, 100000.0, 1.0, 0);
  attach_row(GTK_GRID(grid), 0, _("Start frequency"), start, _("MHz"));
  attach_row(GTK_GRID(grid), 1, _("End frequency"), stop, _("MHz"));
  attach_row(GTK_GRID(grid), 2, _("Number of points"), points, "");
  gtk_widget_show_all(dialog);

  while( (response = gtk_dialog_run(GTK_DIALOG(dialog))) == GTK_RESPONSE_APPLY )
  {
    double f0 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(start));
    double f1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(stop));
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(points));
    double step;
    gchar card[160];
    if( f1 < f0 || (n > 1 && f1 <= f0) )
    {
      Notice(GTK_BUTTONS_OK, _("Frequency sweep"),
          _("End frequency must be greater than start frequency."));
      continue;
    }
    step = n > 1 ? (f1 - f0) / (double)(n - 1) : 0.0;
    g_snprintf(card, sizeof(card), "FR 0 %d 0 0 %.12g %.12g", n, f0, step);
    if( rewrite_card(CARD_FREQUENCY, card) ) break;
  }
  gtk_widget_destroy(dialog);
}

static void on_ground_activate(GtkMenuItem *item, gpointer user_data)
{
  int idx = GPOINTER_TO_INT(user_data);
  const ground_preset_t *p = &ground_presets[idx];
  gchar card[128];
  (void)item;
  if( p->ground_type == 2 )
    g_snprintf(card, sizeof(card), "GN 2 0 0 0 %.8g %.8g",
        p->dielectric, p->conductivity);
  else
    g_snprintf(card, sizeof(card), "GN %d", p->ground_type);
  rewrite_card(CARD_GROUND, card);
}

static gboolean ask_conductivity(const char *title, double *value)
{
  GtkWidget *dialog, *area, *box, *spin;
  gint response;
  dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(main_window),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_APPLY, NULL);
  area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(box), 12);
  gtk_box_pack_start(GTK_BOX(area), box, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), gtk_label_new(_("Conductivity")), FALSE, FALSE, 0);
  spin = number_spin(*value, 1.0, 1.0e10, 100000.0, 0);
  gtk_box_pack_start(GTK_BOX(box), spin, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), gtk_label_new(_("S/m")), FALSE, FALSE, 0);
  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  if( response == GTK_RESPONSE_APPLY )
    *value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
  gtk_widget_destroy(dialog);
  return response == GTK_RESPONSE_APPLY;
}

static void on_material_activate(GtkMenuItem *item, gpointer user_data)
{
  int idx = GPOINTER_TO_INT(user_data);
  const material_preset_t *p = &material_presets[idx];
  double conductivity = p->conductivity;
  gchar card[128];
  (void)item;
  if( !setup_ready() ) return;
  if( idx == 0 )
  {
    rewrite_card(CARD_MATERIAL, NULL);
    return;
  }
  if( p->user_value && !ask_conductivity(_(p->label), &conductivity) ) return;
  g_snprintf(card, sizeof(card), "LD 5 0 0 0 %.12g 0 0", conductivity);
  rewrite_card(CARD_MATERIAL, card);
}

static void on_nec_resources_activate(GtkMenuItem *item, gpointer unused)
{
  (void)item; (void)unused;
#ifdef XNEC2C_NATIVE_WINDOWS
  if( !windows_open_nec_resources() )
    pr_warn("cannot open NEC resources URL with the Windows shell\n");
#else
  GError *error = NULL;
  if( !gtk_show_uri_on_window(GTK_WINDOW(main_window),
        "https://antenas.charlygolf.com/", GDK_CURRENT_TIME, &error) )
  {
    pr_warn("cannot open NEC resources URL: %s\n",
        error ? error->message : "unknown error");
    g_clear_error(&error);
  }
#endif
}

static GtkWidget *submenu_item(GtkWidget *menu, const char *label)
{
  GtkWidget *item = gtk_menu_item_new_with_label(label);
  GtkWidget *submenu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  return submenu;
}

void quick_setup_install(GtkBuilder *builder)
{
  GtkWidget *menubar = NULL, *help = NULL, *setup, *menu, *submenu, *item;
  GSList *objects, *it;
  GList *children, *node;
  int i, help_position = -1, position = 0;

  objects = gtk_builder_get_objects(builder);
  for( it = objects; it != NULL; it = it->next )
    if( GTK_IS_MENU_BAR(it->data)
        && gtk_widget_get_toplevel(GTK_WIDGET(it->data)) == main_window )
    { menubar = GTK_WIDGET(it->data); break; }
  g_slist_free(objects);
  if( menubar == NULL ) return;

  children = gtk_container_get_children(GTK_CONTAINER(menubar));
  for( node = children; node != NULL; node = node->next, position++ )
  {
    const char *label = GTK_IS_MENU_ITEM(node->data)
        ? gtk_menu_item_get_label(GTK_MENU_ITEM(node->data)) : NULL;
    if( label != NULL && g_strrstr(label, "Help") != NULL )
    { help = GTK_WIDGET(node->data); help_position = position; break; }
  }
  g_list_free(children);

  setup = gtk_menu_item_new_with_label(_("Setup"));
  menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(setup), menu);
  item = gtk_menu_item_new_with_label(_("Frequency sweep..."));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  g_signal_connect(item, "activate", G_CALLBACK(on_frequency_activate), NULL);

  submenu = submenu_item(menu, _("Ground type"));
  for( i = 0; i < (int)G_N_ELEMENTS(ground_presets); i++ )
  {
    item = gtk_menu_item_new_with_label(_(ground_presets[i].label));
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_ground_activate),
        GINT_TO_POINTER(i));
  }

  submenu = submenu_item(menu, _("Antenna material"));
  for( i = 0; i < (int)G_N_ELEMENTS(material_presets); i++ )
  {
    item = gtk_menu_item_new_with_label(_(material_presets[i].label));
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_material_activate),
        GINT_TO_POINTER(i));
  }
  if( help_position >= 0 )
    gtk_menu_shell_insert(GTK_MENU_SHELL(menubar), setup, help_position);
  else
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), setup);

  if( help != NULL )
  {
    GtkWidget *help_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(help));
    item = gtk_menu_item_new_with_label(_("NEC Resources — Antenas CharlyGolf"));
    gtk_menu_shell_prepend(GTK_MENU_SHELL(help_menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(on_nec_resources_activate), NULL);
  }
  gtk_widget_show_all(setup);
  if( help != NULL ) gtk_widget_show_all(help);
}

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

static gboolean rewrite_card(card_change_t change, const char *replacement,
    gboolean reopen)
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
  if( reopen ) Open_Input_File(&new_file);
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

typedef struct {
  double start_freq, stop_freq, conductivity;
  int points, ground_index, material_index;
  double ground_dielectric, ground_conductivity;
  double radius_min, radius_max;
  gboolean has_radius, custom_ground, custom_material;
} setup_values_t;

typedef struct {
  GtkComboBox *material;
  GtkSpinButton *conductivity;
} material_controls_t;

static int parse_card_fields(const char *line, gsize length,
    double *fields, int maximum)
{
  gchar *copy = g_strndup(line, length);
  char *p = copy;
  int count = 0;

  while( g_ascii_isspace(*p) ) p++;
  if( *p != '\0' ) p += MIN((gsize)2, strlen(p));
  while( count < maximum )
  {
    char *end;
    while( g_ascii_isspace(*p) ) p++;
    if( *p == '\0' ) break;
    fields[count] = g_ascii_strtod(p, &end);
    if( end == p ) break;
    count++;
    p = end;
  }
  g_free(copy);
  return count;
}

static gboolean nearly_equal(double a, double b)
{
  double scale = MAX(1.0, MAX(fabs(a), fabs(b)));
  return fabs(a - b) <= scale * 1.0e-5;
}

static void read_setup_values(setup_values_t *values)
{
  gchar *contents = NULL;
  gsize length = 0;
  const char *cursor;
  gboolean has_material = FALSE;

  memset(values, 0, sizeof(*values));
  values->start_freq = calc_data.freq_mhz > 0.0 ? calc_data.freq_mhz : 1.0;
  values->stop_freq = values->start_freq;
  values->points = MAX(1, calc_data.steps_total);
  values->ground_index = 4; /* No GN card means free space. */
  if( calc_data.FR_cards > 0 && calc_data.freq_loop_data != NULL )
  {
    values->start_freq = calc_data.freq_loop_data[0].min_freq;
    values->stop_freq =
        calc_data.freq_loop_data[calc_data.FR_cards - 1].max_freq;
  }
  if( !g_file_get_contents(rc_config.input_file, &contents, &length, NULL) )
    return;

  cursor = contents;
  while( *cursor != '\0' )
  {
    const char *end = strchr(cursor, '\n');
    gsize line_len = end != NULL ? (gsize)(end - cursor) : strlen(cursor);
    double f[12];
    int n;

    if( line_len > 0 && cursor[line_len - 1] == '\r' ) line_len--;
    if( line_is_card(cursor, line_len, "GN") )
    {
      n = parse_card_fields(cursor, line_len, f, G_N_ELEMENTS(f));
      if( n >= 1 && (int)f[0] == 1 ) values->ground_index = 3;
      else if( n >= 1 && (int)f[0] == -1 ) values->ground_index = 4;
      else if( n >= 6 && (int)f[0] == 2 )
      {
        int i;
        values->ground_dielectric = f[4];
        values->ground_conductivity = f[5];
        values->ground_index = -1;
        values->custom_ground = TRUE;
        for( i = 0; i < 3; i++ )
          if( nearly_equal(f[4], ground_presets[i].dielectric)
              && nearly_equal(f[5], ground_presets[i].conductivity) )
          {
            values->ground_index = i;
            values->custom_ground = FALSE;
            break;
          }
      }
    }
    else if( line_is_ld5(cursor, line_len) )
    {
      n = parse_card_fields(cursor, line_len, f, G_N_ELEMENTS(f));
      if( n >= 5 )
      {
        values->conductivity = f[4];
        has_material = TRUE;
      }
    }
    else if( line_is_card(cursor, line_len, "GW") )
    {
      n = parse_card_fields(cursor, line_len, f, G_N_ELEMENTS(f));
      if( n >= 9 && f[8] > 0.0 )
      {
        if( !values->has_radius )
          values->radius_min = values->radius_max = f[8];
        else
        {
          values->radius_min = MIN(values->radius_min, f[8]);
          values->radius_max = MAX(values->radius_max, f[8]);
        }
        values->has_radius = TRUE;
      }
    }
    if( end == NULL ) break;
    cursor = end + 1;
  }
  g_free(contents);

  values->material_index = 0;
  if( has_material )
  {
    if( nearly_equal(values->conductivity, material_presets[1].conductivity) )
      values->material_index = 1;
    else if( nearly_equal(values->conductivity, material_presets[3].conductivity) )
      values->material_index = 3;
    else if( nearly_equal(values->conductivity, material_presets[5].conductivity) )
      values->material_index = 5;
    else
    {
      values->material_index = -1;
      values->custom_material = TRUE;
    }
  }
}

static GtkWidget *section_frame(const char *title, GtkWidget **box)
{
  GtkWidget *frame = gtk_frame_new(title);
  *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(*box), 10);
  gtk_container_add(GTK_CONTAINER(frame), *box);
  return frame;
}

static GtkWidget *labelled_spin(const char *label, GtkWidget *spin,
    const char *unit)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
  GtkWidget *line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *text = gtk_label_new(label);
  gtk_widget_set_halign(text, GTK_ALIGN_START);
  gtk_box_pack_start(GTK_BOX(box), text, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(line), spin, TRUE, TRUE, 0);
  if( unit != NULL && *unit != '\0' )
    gtk_box_pack_start(GTK_BOX(line), gtk_label_new(unit), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), line, FALSE, FALSE, 0);
  return box;
}

static void material_changed(GtkComboBox *combo, gpointer user_data)
{
  material_controls_t *controls = user_data;
  int idx = gtk_combo_box_get_active(combo);
  gboolean custom = idx == (int)G_N_ELEMENTS(material_presets)
      || (idx >= 0 && idx < (int)G_N_ELEMENTS(material_presets)
          && material_presets[idx].user_value);

  if( idx > 0 && idx < (int)G_N_ELEMENTS(material_presets) && !custom )
    gtk_spin_button_set_value(controls->conductivity,
        material_presets[idx].conductivity);
  gtk_widget_set_sensitive(GTK_WIDGET(controls->conductivity), custom);
}

static void on_setup_activate(GtkMenuItem *item, gpointer unused)
{
  setup_values_t values;
  material_controls_t material_controls;
  GtkWidget *dialog, *content, *outer, *frame, *box, *row;
  GtkWidget *start, *stop, *points, *ground, *material, *conductivity;
  GtkWidget *dimension;
  gchar dimension_text[256], custom_ground[160], custom_material[160];
  int i;
  (void)item; (void)unused;
  if( !setup_ready() ) return;
  read_setup_values(&values);

  dialog = gtk_dialog_new_with_buttons(_("Antenna setup"),
      GTK_WINDOW(main_window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"), GTK_RESPONSE_APPLY, NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog), 720, -1);
  content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(outer), 12);
  gtk_box_pack_start(GTK_BOX(content), outer, TRUE, TRUE, 0);

  frame = section_frame(_("Frequency sweep"), &box);
  row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  start = number_spin(values.start_freq, 0.000001, 1000000.0, 0.1, 6);
  stop = number_spin(values.stop_freq, 0.000001, 1000000.0, 0.1, 6);
  points = number_spin(values.points, 1.0, 100000.0, 1.0, 0);
  gtk_box_pack_start(GTK_BOX(row), labelled_spin(_("Start frequency"), start,
      _("MHz")), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(row), labelled_spin(_("End frequency"), stop,
      _("MHz")), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(row), labelled_spin(_("Number of points"), points,
      ""), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(outer), frame, FALSE, FALSE, 0);

  frame = section_frame(_("Ground type"), &box);
  ground = gtk_combo_box_text_new();
  for( i = 0; i < (int)G_N_ELEMENTS(ground_presets); i++ )
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ground),
        _(ground_presets[i].label));
  if( values.custom_ground )
  {
    g_snprintf(custom_ground, sizeof(custom_ground),
        _("Custom value (Er %.12g, conductivity %.12g S/m)"),
        values.ground_dielectric, values.ground_conductivity);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ground), custom_ground);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ground), G_N_ELEMENTS(ground_presets));
  }
  else gtk_combo_box_set_active(GTK_COMBO_BOX(ground), values.ground_index);
  gtk_box_pack_start(GTK_BOX(box), ground, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(outer), frame, FALSE, FALSE, 0);

  frame = section_frame(_("Antenna material"), &box);
  row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  material = gtk_combo_box_text_new();
  for( i = 0; i < (int)G_N_ELEMENTS(material_presets); i++ )
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(material),
        _(material_presets[i].label));
  if( values.custom_material )
  {
    g_snprintf(custom_material, sizeof(custom_material),
        _("Custom value (conductivity %.12g S/m)"), values.conductivity);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(material), custom_material);
    gtk_combo_box_set_active(GTK_COMBO_BOX(material),
        G_N_ELEMENTS(material_presets));
  }
  else
    gtk_combo_box_set_active(GTK_COMBO_BOX(material), values.material_index);
  gtk_box_pack_start(GTK_BOX(row), labelled_spin(_("Material"), material, ""),
      TRUE, TRUE, 0);
  conductivity = number_spin(values.conductivity > 0.0 ? values.conductivity
      : material_presets[1].conductivity, 1.0, 1.0e10, 100000.0, 0);
  gtk_box_pack_start(GTK_BOX(row), labelled_spin(_("Conductivity"), conductivity,
      _("S/m")), TRUE, TRUE, 0);
  if( values.has_radius && nearly_equal(values.radius_min, values.radius_max) )
    g_snprintf(dimension_text, sizeof(dimension_text),
        _("Radius: %.6g m (%.3f mm)\nDiameter: %.6g m (%.3f mm)"),
        values.radius_min, values.radius_min * 1000.0,
        values.radius_min * 2.0, values.radius_min * 2000.0);
  else if( values.has_radius )
    g_snprintf(dimension_text, sizeof(dimension_text),
        _("Radius range: %.6g–%.6g m\nDiameter range: %.6g–%.6g m"),
        values.radius_min, values.radius_max,
        values.radius_min * 2.0, values.radius_max * 2.0);
  else
    g_snprintf(dimension_text, sizeof(dimension_text),
        "%s", _("Radius/diameter: not available for this geometry"));
  dimension = gtk_label_new(dimension_text);
  gtk_widget_set_halign(dimension, GTK_ALIGN_START);
  gtk_widget_set_valign(dimension, GTK_ALIGN_END);
  gtk_box_pack_start(GTK_BOX(row), dimension, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(outer), frame, FALSE, FALSE, 0);
  material_controls.material = GTK_COMBO_BOX(material);
  material_controls.conductivity = GTK_SPIN_BUTTON(conductivity);
  g_signal_connect(material, "changed", G_CALLBACK(material_changed),
      &material_controls);
  material_changed(GTK_COMBO_BOX(material), &material_controls);
  gtk_widget_show_all(dialog);

  while( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY )
  {
    double f0 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(start));
    double f1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(stop));
    int n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(points));
    int ground_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(ground));
    int material_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(material));
    double step, sigma;
    gchar frequency_card[160], ground_card[160], material_card[160];
    const char *material_replacement = material_card;

    if( f1 < f0 || (n > 1 && f1 <= f0) )
    {
      Notice(GTK_BUTTONS_OK, _("Frequency sweep"),
          _("End frequency must be greater than start frequency."));
      continue;
    }
    step = n > 1 ? (f1 - f0) / (double)(n - 1) : 0.0;
    g_snprintf(frequency_card, sizeof(frequency_card),
        "FR 0 %d 0 0 %.12g %.12g", n, f0, step);
    if( values.custom_ground
        && ground_idx == (int)G_N_ELEMENTS(ground_presets) )
      g_snprintf(ground_card, sizeof(ground_card), "GN 2 0 0 0 %.12g %.12g",
          values.ground_dielectric, values.ground_conductivity);
    else if( ground_idx >= 0 && ground_presets[ground_idx].ground_type == 2 )
      g_snprintf(ground_card, sizeof(ground_card), "GN 2 0 0 0 %.12g %.12g",
          ground_presets[ground_idx].dielectric,
          ground_presets[ground_idx].conductivity);
    else
      g_snprintf(ground_card, sizeof(ground_card), "GN %d",
          ground_presets[ground_idx].ground_type);
    if( material_idx == 0 ) material_replacement = NULL;
    else
    {
      sigma = (material_idx == (int)G_N_ELEMENTS(material_presets)
          || material_presets[material_idx].user_value)
          ? gtk_spin_button_get_value(GTK_SPIN_BUTTON(conductivity))
          : material_presets[material_idx].conductivity;
      g_snprintf(material_card, sizeof(material_card),
          "LD 5 0 0 0 %.12g 0 0", sigma);
    }
    if( rewrite_card(CARD_FREQUENCY, frequency_card, FALSE)
        && rewrite_card(CARD_GROUND, ground_card, FALSE)
        && rewrite_card(CARD_MATERIAL, material_replacement, TRUE) )
      break;
  }
  gtk_widget_destroy(dialog);
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

void quick_setup_install(GtkBuilder *builder)
{
  GtkWidget *menubar = NULL, *help = NULL, *setup, *item;
  GSList *objects, *it;
  GList *children, *node;
  int help_position = -1, position = 0;

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
  g_signal_connect(setup, "activate", G_CALLBACK(on_setup_activate), NULL);
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
  gtk_widget_show(setup);
  if( help != NULL ) gtk_widget_show_all(help);
}

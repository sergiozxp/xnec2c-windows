/*
 * Friendly whole-antenna transformations.
 *
 * The operations are represented by standard NEC GS/GM cards inserted just
 * before GE.  Existing wire tags therefore remain valid for EX, LD and TL
 * cards, and non-GW geometry is transformed by the NEC engine as well.
 */

#include "antenna_transform.h"
#include "callback_func.h"
#include "editors.h"
#include "prerender/prerender_state.h"
#include "shared.h"

#include <float.h>
#include <math.h>

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

static void grid_row(GtkGrid *grid, int row, const char *label,
    GtkWidget *widget, const char *unit)
{
  GtkWidget *l = gtk_label_new(label);
  gtk_widget_set_halign(l, GTK_ALIGN_START);
  gtk_grid_attach(grid, l, 0, row, 1, 1);
  gtk_grid_attach(grid, widget, 1, row, 1, 1);
  if( unit != NULL )
  {
    l = gtk_label_new(unit);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_grid_attach(grid, l, 2, row, 1, 1);
  }
}

static GtkWidget *transform_dialog(const char *title, GtkGrid **grid_out)
{
  GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
      main_window != NULL ? GTK_WINDOW(main_window) : NULL,
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      _("_Cancel"), GTK_RESPONSE_CANCEL,
      _("_Apply"), GTK_RESPONSE_OK, NULL);
  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid = gtk_grid_new();
  gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
  gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
  *grid_out = GTK_GRID(grid);
  return dialog;
}

static gboolean minimum_height(double *height)
{
  double zmin = DBL_MAX;
  int i;

  if( data.n > 0 && data.segments != NULL )
  {
    for( i = 0; i < data.n; i++ )
    {
      zmin = MIN(zmin, data.segments[i].z1);
      zmin = MIN(zmin, data.segments[i].z2);
    }
  }
  if( data.m > 0 && geom_pre.patch_corners != NULL )
  {
    /* Render-time patch corners are wavelength-normalized. */
    for( i = 0; i < data.m; i++ )
    {
      const patch_corners_t *pc = &geom_pre.patch_corners[i];
      zmin = MIN(zmin, pc->c0z * data.wlam);
      zmin = MIN(zmin, pc->c1z * data.wlam);
      zmin = MIN(zmin, pc->c2z * data.wlam);
      zmin = MIN(zmin, pc->c3z * data.wlam);
    }
  }
  else if( data.m > 0 && data.patches != NULL )
  {
    /* A center is a safe fallback before render aggregates exist. */
    for( i = 0; i < data.m; i++ )
      zmin = MIN(zmin, data.patches[i].pz * data.wlam);
  }
  if( zmin == DBL_MAX ) return FALSE;
  *height = zmin;
  return TRUE;
}

static gboolean ensure_geometry_editor(void)
{
  if( data.n <= 0 && data.m <= 0 )
  {
    Notice(GTK_BUTTONS_OK, _("Transform antenna"),
        _("Open a valid NEC antenna model before applying a transformation."));
    return FALSE;
  }
  if( nec2_edit_window == NULL ) Open_Nec2_Editor(NEC2_EDITOR_RELOAD);
  return geom_store != NULL;
}

static gboolean insert_geometry_card(const char *name, const gint iv[2],
    const gdouble fv[7])
{
  GtkTreeModel *model;
  GtkTreeIter iter, before;
  gboolean valid, found_ge = FALSE;

  if( !ensure_geometry_editor() ) return FALSE;
  model = GTK_TREE_MODEL(geom_store);
  valid = gtk_tree_model_get_iter_first(model, &before);
  while( valid )
  {
    gchar *row_name = NULL;
    gtk_tree_model_get(model, &before, GEOM_COL_NAME, &row_name, -1);
    if( row_name != NULL && strcmp(row_name, "GE") == 0 ) found_ge = TRUE;
    g_free(row_name);
    if( found_ge ) break;
    valid = gtk_tree_model_iter_next(model, &before);
  }
  if( !found_ge )
  {
    Notice(GTK_BUTTONS_OK, _("Transform antenna"),
        _("The geometry has no GE termination card."));
    return FALSE;
  }

  gtk_list_store_insert_before(geom_store, &iter, &before);
  Zero_Store(geom_store, &iter, GEOM_NUM_COLS, GEOM_COL_NAME, -1);
  gtk_list_store_set(geom_store, &iter,
      GEOM_COL_NAME, name,
      GEOM_COL_I1, iv[0], GEOM_COL_I2, iv[1],
      GEOM_COL_F1, fv[0], GEOM_COL_F2, fv[1],
      GEOM_COL_F3, fv[2], GEOM_COL_F4, fv[3],
      GEOM_COL_F5, fv[4], GEOM_COL_F6, fv[5],
      GEOM_COL_F7, fv[6], -1);
  SetFlag(NEC2_EDIT_SAVE);
  gtk_window_present(GTK_WINDOW(nec2_edit_window));
  return TRUE;
}

static gboolean add_gm(double rx, double ry, double rz,
    double dx, double dy, double dz)
{
  const gint iv[2] = { 0, 0 };
  const gdouble fv[7] = { rx, ry, rz, dx, dy, dz, 0.0 };
  return insert_geometry_card("GM", iv, fv);
}

static gboolean add_gs(double factor)
{
  const gint iv[2] = { 0, 0 };
  const gdouble fv[7] = { factor, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  return insert_geometry_card("GS", iv, fv);
}

typedef struct
{
  GtkSpinButton *z;
  GtkLabel *result;
  double current;
} move_preview_t;

static void move_preview_changed(GtkSpinButton *spin, gpointer user_data)
{
  move_preview_t *p = user_data;
  gchar text[96];
  (void)spin;
  snprintf(text, sizeof(text), _("Resulting minimum height: %.4f m"),
      p->current + gtk_spin_button_get_value(p->z));
  gtk_label_set_text(p->result, text);
}

void on_transform_move_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  GtkGrid *grid;
  GtkWidget *dialog, *sx, *sy, *sz, *current, *result;
  move_preview_t preview;
  double height;
  gchar text[96];
  (void)menuitem; (void)user_data;

  if( !minimum_height(&height) )
  {
    ensure_geometry_editor();
    return;
  }
  dialog = transform_dialog(_("Move antenna"), &grid);
  sx = number_spin(0.0, -1000000.0, 1000000.0, 0.1, 4);
  sy = number_spin(0.0, -1000000.0, 1000000.0, 0.1, 4);
  sz = number_spin(0.0, -1000000.0, 1000000.0, 0.1, 4);
  grid_row(grid, 0, _("Along X"), sx, _("m"));
  grid_row(grid, 1, _("Along Y"), sy, _("m"));
  grid_row(grid, 2, _("Along Z"), sz, _("m"));
  snprintf(text, sizeof(text), _("Current minimum height: %.4f m"), height);
  current = gtk_label_new(text);
  gtk_widget_set_halign(current, GTK_ALIGN_START);
  gtk_grid_attach(grid, current, 0, 3, 3, 1);
  result = gtk_label_new("");
  gtk_widget_set_halign(result, GTK_ALIGN_START);
  gtk_grid_attach(grid, result, 0, 4, 3, 1);
  preview.z = GTK_SPIN_BUTTON(sz); preview.result = GTK_LABEL(result);
  preview.current = height;
  g_signal_connect(sz, "value-changed", G_CALLBACK(move_preview_changed), &preview);
  move_preview_changed(GTK_SPIN_BUTTON(sz), &preview);
  gtk_widget_show_all(dialog);

  if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK )
  {
    double dz = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sz));
    double resulting = height + dz;
    if( resulting < 0.0 )
    {
      GtkWidget *warning = gtk_message_dialog_new(GTK_WINDOW(dialog),
          GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
          _("The antenna will extend %.4f m below the ground plane.\nApply anyway?"),
          -resulting);
      gint response = gtk_dialog_run(GTK_DIALOG(warning));
      gtk_widget_destroy(warning);
      if( response != GTK_RESPONSE_OK ) { gtk_widget_destroy(dialog); return; }
    }
    add_gm(0.0, 0.0, 0.0,
        gtk_spin_button_get_value(GTK_SPIN_BUTTON(sx)),
        gtk_spin_button_get_value(GTK_SPIN_BUTTON(sy)), dz);
  }
  gtk_widget_destroy(dialog);
}

void on_transform_rotate_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  GtkGrid *grid;
  GtkWidget *dialog, *axis, *angle, *ccw, *cw, *note;
  (void)menuitem; (void)user_data;
  if( data.n <= 0 && data.m <= 0 ) { ensure_geometry_editor(); return; }

  dialog = transform_dialog(_("Rotate antenna"), &grid);
  axis = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(axis), _("X axis"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(axis), _("Y axis"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(axis), _("Z axis"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(axis), 2);
  angle = number_spin(90.0, 0.0, 360.0, 1.0, 2);
  grid_row(grid, 0, _("Around"), axis, NULL);
  grid_row(grid, 1, _("Angle"), angle, _("deg"));
  ccw = gtk_radio_button_new_with_label(NULL, _("Counter-clockwise (CCW)"));
  cw = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ccw),
      _("Clockwise (CW)"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ccw), TRUE);
  gtk_grid_attach(grid, ccw, 0, 2, 3, 1);
  gtk_grid_attach(grid, cw, 0, 3, 3, 1);
  note = gtk_label_new(_("Rotation is about the global origin (0, 0, 0)."));
  gtk_widget_set_halign(note, GTK_ALIGN_START);
  gtk_grid_attach(grid, note, 0, 4, 3, 1);
  gtk_widget_show_all(dialog);

  if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK )
  {
    double a = gtk_spin_button_get_value(GTK_SPIN_BUTTON(angle));
    int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(axis));
    if( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw)) ) a = -a;
    add_gm(selected == 0 ? a : 0.0, selected == 1 ? a : 0.0,
        selected == 2 ? a : 0.0, 0.0, 0.0, 0.0);
  }
  gtk_widget_destroy(dialog);
}

typedef struct
{
  GtkSpinButton *old_freq;
  GtkSpinButton *new_freq;
  GtkSpinButton *factor;
  gboolean updating;
} scale_link_t;

static void scale_frequency_changed(GtkSpinButton *spin, gpointer user_data)
{
  scale_link_t *link = user_data;
  double oldf, newf;
  (void)spin;
  if( link->updating ) return;
  oldf = gtk_spin_button_get_value(link->old_freq);
  newf = gtk_spin_button_get_value(link->new_freq);
  if( newf <= 0.0 ) return;
  link->updating = TRUE;
  gtk_spin_button_set_value(link->factor, oldf / newf);
  link->updating = FALSE;
}

static void scale_factor_changed(GtkSpinButton *spin, gpointer user_data)
{
  scale_link_t *link = user_data;
  double oldf, factor;
  (void)spin;
  if( link->updating ) return;
  oldf = gtk_spin_button_get_value(link->old_freq);
  factor = gtk_spin_button_get_value(link->factor);
  if( factor <= 0.0 ) return;
  link->updating = TRUE;
  gtk_spin_button_set_value(link->new_freq, oldf / factor);
  link->updating = FALSE;
}

void on_transform_scale_activate(GtkMenuItem *menuitem, gpointer user_data)
{
  GtkGrid *grid;
  GtkWidget *dialog, *oldf, *newf, *factor, *radius, *preserve, *note;
  scale_link_t link;
  double height = 0.0, current_freq;
  (void)menuitem; (void)user_data;
  if( !minimum_height(&height) ) { ensure_geometry_editor(); return; }

  current_freq = calc_data.freq_mhz > 0.0 ? calc_data.freq_mhz : 1.0;
  dialog = transform_dialog(_("Scale antenna"), &grid);
  oldf = number_spin(current_freq, 0.000001, 1000000.0, 0.1, 6);
  newf = number_spin(current_freq, 0.000001, 1000000.0, 0.1, 6);
  factor = number_spin(1.0, 0.000001, 1000000.0, 0.01, 6);
  grid_row(grid, 0, _("Reference frequency"), oldf, _("MHz"));
  grid_row(grid, 1, _("New frequency"), newf, _("MHz"));
  grid_row(grid, 2, _("Scale factor"), factor, NULL);
  radius = gtk_check_button_new_with_label(_("Scale wire radius"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radius), TRUE);
  gtk_widget_set_sensitive(radius, FALSE);
  gtk_widget_set_tooltip_text(radius,
      _("The NEC GS card scales coordinates and wire radii together."));
  preserve = gtk_check_button_new_with_label(_("Preserve minimum height"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(preserve), TRUE);
  gtk_grid_attach(grid, radius, 0, 3, 3, 1);
  gtk_grid_attach(grid, preserve, 0, 4, 3, 1);
  note = gtk_label_new(_("Geometry is scaled about the global origin."));
  gtk_widget_set_halign(note, GTK_ALIGN_START);
  gtk_grid_attach(grid, note, 0, 5, 3, 1);
  link.old_freq = GTK_SPIN_BUTTON(oldf);
  link.new_freq = GTK_SPIN_BUTTON(newf);
  link.factor = GTK_SPIN_BUTTON(factor);
  link.updating = FALSE;
  g_signal_connect(oldf, "value-changed", G_CALLBACK(scale_frequency_changed), &link);
  g_signal_connect(newf, "value-changed", G_CALLBACK(scale_frequency_changed), &link);
  g_signal_connect(factor, "value-changed", G_CALLBACK(scale_factor_changed), &link);
  gtk_widget_show_all(dialog);

  if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK )
  {
    double scale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(factor));
    if( add_gs(scale) &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(preserve)) )
      add_gm(0.0, 0.0, 0.0, 0.0, 0.0, height * (1.0 - scale));
  }
  gtk_widget_destroy(dialog);
}

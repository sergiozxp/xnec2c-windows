/* Whole-antenna transformation helpers and UI. */
#ifndef ANTENNA_TRANSFORM_H
#define ANTENNA_TRANSFORM_H 1

#include "common.h"

void on_transform_move_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_transform_rotate_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_transform_scale_activate(GtkMenuItem *menuitem, gpointer user_data);

#endif

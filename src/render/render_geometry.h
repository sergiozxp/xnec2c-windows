/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef __RENDER_GEOMETRY_H
#define __RENDER_GEOMETRY_H 1

#include "render_dispatch.h"

/* Point sink receiving one drawn world-space vertex and its model scale. */
typedef void (*render_geom_point_fn)(void *user,
    double x, double y, double z, float scale);

/** render_nearfield_fields() - Resolve active near-field vector sets
 * @fstep: frequency step index
 * @sets:  receives active origin, vector, and color sets
 *
 * Returns the number of populated entries in @sets.
 */
int render_nearfield_fields(int fstep,
    field_vector_set_t sets[NF_FIELD_SETS_MAX]);

/** render_farfield_vectors() - Resolve the far-zone instantaneous field set
 * @fstep: frequency step index
 * @ff:    far-field draw parameters, supplying the pattern-space excitation
 *         translation the arrows attach through
 * @set:   receives the origin, vector, and color arrays
 *
 * Returns the number of populated entries in @set, zero while the pattern
 * window shows no animated gain surface.
 */
int render_farfield_vectors(int fstep, const ff_draw_params_t *ff,
    field_vector_set_t *set);

/** render_geom_walk() - Emit the active view's drawn world geometry
 * @view: view selecting structure, far-field, or near-field content
 * @sink: receives each drawn vertex and its model scale
 * @user: opaque pointer passed to @sink
 *
 * Returns the active content reference extent, or zero when unavailable.
 * The caller holds freq_data_lock.
 */
float render_geom_walk(view_t *view, render_geom_point_fn sink, void *user);

/** render_geometry_free() - Release the published origin buffers
 */
void render_geometry_free(void);

#endif /* __RENDER_GEOMETRY_H */

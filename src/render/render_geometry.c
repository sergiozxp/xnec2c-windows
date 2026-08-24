/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "render_geometry.h"

#include "../chroma/chroma_farfield.h"
#include "../chroma/chroma_nearfield.h"
#include "../rdpattern_ui.h"
#include "../shared.h"
#include "../structure_ui.h"

typedef struct
{
  gboolean     active;
  nf_channel_t channel;

} nearfield_field_spec_t;

/* Drawn vector origins, one buffer per domain grid.  The near-field grid is
 * copied from its sample points; the far-zone grid borrows the surface
 * vertices unless an excitation translation moves them. */
static point_3d_t *nf_origins;
static point_3d_t *ff_origins;

/** render_geometry_free() - Release the published origin buffers
 */
  void
render_geometry_free(void)
{
  mem_array_free(&nf_origins);
  mem_array_free(&ff_origins);

} /* render_geometry_free() */

/** geom_walk_structure() - Emit structure endpoints and patch corners
 * @sink:  point receiver
 * @user:  opaque sink context
 * @scale: model scale applied by the renderer
 */
  static void
geom_walk_structure(render_geom_point_fn sink, void *user, float scale)
{
  int idx;

  for( idx = 0; idx < data.n; idx++ )
  {
    sink(user, (double)data.segments[idx].x1, (double)data.segments[idx].y1,
        (double)data.segments[idx].z1, scale);
    sink(user, (double)data.segments[idx].x2, (double)data.segments[idx].y2,
        (double)data.segments[idx].z2, scale);
  }

  if( geom_pre.patch_corners != NULL )
  {
    for( idx = 0; idx < data.m; idx++ )
    {
      const patch_corners_t *pc = &geom_pre.patch_corners[idx];

      sink(user, pc->c0x, pc->c0y, pc->c0z, scale);
      sink(user, pc->c1x, pc->c1y, pc->c1z, scale);
      sink(user, pc->c2x, pc->c2y, pc->c2z, scale);
      sink(user, pc->c3x, pc->c3y, pc->c3z, scale);
    }
  }

} /* geom_walk_structure() */

/** render_nearfield_fields() - Resolve active near-field vector sets
 * @fstep: frequency step index
 * @sets:  receives active origin, vector, and color sets
 *
 * Returns the number of populated entries in @sets.
 */
  int
render_nearfield_fields(int fstep, field_vector_set_t sets[NF_FIELD_SETS_MAX])
{
  const nearfield_field_spec_t specs[NF_FIELD_SETS_MAX] =
  {
    { draw_efield_active() && (fpat.nfeh & NEAR_EFIELD), NF_CHAN_E },
    { draw_hfield_active() && (fpat.nfeh & NEAR_HFIELD), NF_CHAN_H },
    { draw_poynting_active() && (fpat.nfeh & NEAR_EFIELD)
        && (fpat.nfeh & NEAR_HFIELD), NF_CHAN_POV }
  };
  const near_field_t *nf = &near_field_fstep[fstep];
  int npts = fpat.nrx * fpat.nry * fpat.nrz;
  int count = 0;
  int idx;

  if( npts <= 0 || nf->points == NULL )
    return 0;

  /* Publish the sample positions as drawn origins, so one point type reaches
   * the vector capability from either domain */
  mem_array_realloc(&nf_origins, npts);
  for( idx = 0; idx < npts; idx++ )
  {
    double px = nf->points[idx].px;
    double py = nf->points[idx].py;
    double pz = nf->points[idx].pz;

    nf_origins[idx].x = px;
    nf_origins[idx].y = py;
    nf_origins[idx].z = pz;
    nf_origins[idx].r = sqrt(px * px + py * py + pz * pz);
  }

  for( idx = 0; idx < NF_FIELD_SETS_MAX; idx++ )
  {
    field_frame_t frame;

    if( !specs[idx].active )
      continue;

    frame = chroma_proj_frame_nearfield(fstep, specs[idx].channel);
    if( frame.vecs == NULL )
      continue;

    sets[count].origins = nf_origins;
    sets[count].vecs    = frame.vecs;
    sets[count].colors  = frame.colors;
    sets[count].npts    = npts;
    sets[count].extent  = frame.extent;
    count++;
  }

  return count;

} /* render_nearfield_fields() */

/** render_farfield_vectors() - Resolve the far-zone instantaneous field set
 * @fstep: frequency step index
 * @ff:    far-field draw parameters, supplying the pattern-space excitation
 *         translation the arrows attach through
 * @set:   receives the origin, vector, and color arrays
 *
 * Returns the number of populated entries in @set, zero while the pattern
 * window shows no animated gain surface.
 */
  int
render_farfield_vectors(int fstep, const ff_draw_params_t *ff,
    field_vector_set_t *set)
{
  field_frame_t frame;
  ff_pre_t *fp;
  int npts, idx;

  if( !rdpat_farfield_phase_active() || ff_pre == NULL )
    return 0;

  fp = &ff_pre[fstep];
  npts = fpat.nth * fpat.nph;
  if( npts <= 0 || fp->vertices == NULL )
    return 0;

  frame = chroma_proj_frame_farfield(fstep);
  if( frame.vecs == NULL )
    return 0;

  /* The arrows attach to the surface, so they carry its translation */
  if( ff->off_len > FF_EXCITATION_OFFSET_MIN )
  {
    mem_array_realloc(&ff_origins, npts);
    for( idx = 0; idx < npts; idx++ )
    {
      ff_origins[idx].x = fp->vertices[idx].x + (double)ff->x;
      ff_origins[idx].y = fp->vertices[idx].y + (double)ff->y;
      ff_origins[idx].z = fp->vertices[idx].z + (double)ff->z;

      /* The translation moves the pattern bodily, so each cell keeps the
       * pattern radius the untranslated path carries */
      ff_origins[idx].r = fp->vertices[idx].r;
    }
    set->origins = ff_origins;
  }
  else
    set->origins = fp->vertices;

  set->vecs   = frame.vecs;
  set->colors = frame.colors;
  set->npts   = npts;
  set->extent = frame.extent;

  return 1;

} /* render_farfield_vectors() */

/** geom_walk_nearfield() - Emit active near-field segment endpoints
 * @fstep: frequency step index
 * @sink:  point receiver
 * @user:  opaque sink context
 */
  static void
geom_walk_nearfield(int fstep, render_geom_point_fn sink, void *user)
{
  field_vector_set_t sets[NF_FIELD_SETS_MAX] = {{0}};
  int n_sets = render_nearfield_fields(fstep, sets);
  int set_idx;
  int point_idx;

  for( set_idx = 0; set_idx < n_sets; set_idx++ )
  {
    const point_3d_t *origins = sets[set_idx].origins;
    const field_vector_t *vecs = sets[set_idx].vecs;

    for( point_idx = 0; point_idx < sets[set_idx].npts; point_idx++ )
    {
      sink(user, origins[point_idx].x, origins[point_idx].y,
          origins[point_idx].z, 1.0f);
      sink(user, origins[point_idx].x + (double)vecs[point_idx].dx,
          origins[point_idx].y + (double)vecs[point_idx].dy,
          origins[point_idx].z + (double)vecs[point_idx].dz, 1.0f);
    }
  }

} /* geom_walk_nearfield() */

/** render_geom_walk() - Emit the active view's drawn world geometry
 * @view: view selecting structure, far-field, or near-field content
 * @sink: receives each drawn vertex and its model scale
 * @user: opaque pointer passed to @sink
 *
 * Returns the active content reference extent, or zero when unavailable.
 * The caller holds freq_data_lock.
 */
  float
render_geom_walk(view_t *view, render_geom_point_fn sink, void *user)
{
  render_check_result_t result = render_check(view->type);
  float extent = 0.0f;
  int idx;

  if( result.status != RENDER_OK )
    return 0.0f;

  switch( result.mode )
  {
    case RENDER_MODE_FARFIELD:
    {
      ff_pre_t *farfield = (ff_pre != NULL) ? &ff_pre[result.fstep] : NULL;
      ff_draw_params_t params = {0};
      float model_scale;
      int nverts;

      if( farfield == NULL || farfield->vertices == NULL )
        break;

      model_scale = render_overlay_model_scale(result.fstep);
      render_overlay_excitation_offset(model_scale, result.overlay_active,
          &params);
      extent = farfield->pattern_radius;
      nverts = mem_array_count(farfield->vertices);

      for( idx = 0; idx < nverts; idx++ )
        sink(user, farfield->vertices[idx].x + (double)params.x,
            farfield->vertices[idx].y + (double)params.y,
            farfield->vertices[idx].z + (double)params.z, 1.0f);

      if( result.overlay_active )
        geom_walk_structure(sink, user, model_scale);
      break;
    }

    case RENDER_MODE_NEARFIELD:
    {
      const near_field_t *nearfield = &near_field_fstep[result.fstep];

      if( nearfield->points == NULL )
        break;

      extent = (float)nearfield->r_max;
      geom_walk_nearfield(result.fstep, sink, user);
      if( result.overlay_active )
        geom_walk_structure(sink, user, 1.0f);
      break;
    }

    case RENDER_MODE_STRUCTURE:
      extent = (float)geom_pre.scene_radius;
      geom_walk_structure(sink, user, 1.0f);
      break;

    case RENDER_MODE_NONE:
    case RENDER_MODE_COUNT:
      BUG("render_geom_walk: unresolved render mode %d\n", result.mode);
      break;
  }

  return extent;

} /* render_geom_walk() */

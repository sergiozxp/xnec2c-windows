/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  The official website and doumentation for xnec2c is available here:
 *    https://www.xnec2c.org/
 */

/*
 * chroma_farfield: parent draw-time far-zone field color and geometry resolver.
 *
 * The far zone joins wire, patch, and near field on one color lifecycle: the
 * immutable phasor pair in rad_pattern[fstep].phasor is the source, this
 * resolver derives the instantaneous field vector, its scaled displacement,
 * and its palette color at draw, and the render backends consume the resolved
 * arrays.  The far-zone field is transverse, so the displacement lies tangent
 * to the pattern sphere and the gain surface it attaches to is untouched.
 */
#include "chroma_farfield.h"
#include "../color/color_edge.h"
#include "../color/color_palette.h"
#include "../color/color_tone.h"
#include "../shared.h"

/* Resolved-frame buffers and the input edge gating their rebuild.  One draw's
 * geometry, color, and scratch magnitude live here, parallel and indexed by
 * pattern cell. */
static field_vector_t *ff_vec_buf;
static rgb_f_t        *ff_col_buf;
static double         *ff_mag_buf;

static color_edge_t ff_edge;
static gboolean     ff_edge_valid;

/* Complex operator carrying the stored pair to the drawn component pair:
 *   drawn theta = a * E_theta + b * E_phi
 *   drawn phi   = c * E_theta + d * E_phi */
typedef struct
{
  complex double a, b, c, d;
} ff_operator_t;

/*-----------------------------------------------------------------------*/

/**
 * ff_compose() - Apply one operator after another
 * @outer: operator applied second
 * @inner: operator applied first
 */
  static ff_operator_t
ff_compose(ff_operator_t outer, ff_operator_t inner)
{
  return (ff_operator_t){
    outer.a * inner.a + outer.b * inner.c,
    outer.a * inner.b + outer.b * inner.d,
    outer.c * inner.a + outer.d * inner.c,
    outer.c * inner.b + outer.d * inner.d };

} /* ff_compose() */

/*-----------------------------------------------------------------------*/

/**
 * ff_pol_operator() - Operator selecting the drawn polarization component
 * @pol: polarization selection the gain surface under the arrows is scaled by
 * @cs:  cosine of the angle the linear pair is turned by
 * @sn:  sine of the angle the linear pair is turned by
 *
 * The surface radius carries Polarization_Factor(), a power fraction taken
 * from the axial ratio and tilt of this same phasor pair.  These operators are
 * that fraction in vector form, so the arrow and the surface beneath it
 * describe one quantity while the arrow keeps the direction and the phase the
 * power fraction discards.
 *
 * The linear pair is turned by (cs, sn): the world basis passes (1, 0) and
 * recovers theta_hat and phi_hat, Ludwig-3 passes the cell azimuth and
 * recovers the co-polar and cross-polar directions.  The circular selections
 * take no turn, since turning their basis multiplies each cell by a phase and
 * breaks the phase relation the animation reads between lobes.
 */
  static ff_operator_t
ff_pol_operator(int pol, double cs, double sn)
{
  ff_operator_t op = { 1.0, 0.0, 0.0, 1.0 };

  switch( pol )
  {
    case POL_TOTAL:
      break;

    case POL_VERT:
      /* Co-polar member of the linear pair */
      op = (ff_operator_t){ cs * cs, -cs * sn, -cs * sn, sn * sn };
      break;

    case POL_HORIZ:
      /* Cross-polar member of the linear pair */
      op = (ff_operator_t){ sn * sn, cs * sn, cs * sn, cs * cs };
      break;

    case POL_RHCP:
      /* (E_theta + j E_phi) / sqrt(2), re-expanded on theta_hat and phi_hat.
       * The solver marks right-hand sense where E_phi lags E_theta, so the
       * right-hand direction is theta_hat - j phi_hat and this row keeps the
       * same hand the axial-ratio sign carries to the surface. */
      op = (ff_operator_t){ 0.5, 0.5 * I, -0.5 * I, 0.5 };
      break;

    case POL_LHCP:
      /* (E_theta - j E_phi) / sqrt(2), re-expanded on theta_hat and phi_hat */
      op = (ff_operator_t){ 0.5, -0.5 * I, 0.5 * I, 0.5 };
      break;

    case NUM_POL:
    default:
      BUG("far-zone arrows: unresolved polarization %d\n", pol);
      break;
  }

  return op;

} /* ff_pol_operator() */

/*-----------------------------------------------------------------------*/

  gboolean
ff_frame_turns_pol(int pol)
{
  return (pol == POL_VERT) || (pol == POL_HORIZ);

} /* ff_frame_turns_pol() */

/*-----------------------------------------------------------------------*/

/**
 * ff_follow_display_rotation() - Carry resolved tangents into the drawn frame
 * @rot:   display rotation the presentation applied to the pattern vertices
 * @vecs:  displacements rotated in place
 * @total: pattern cell count
 *
 * Noise mode tilts the drawn pattern so the sky and earth boundary reads
 * horizontal, while the resolve pass builds its tangent basis from the
 * untilted spherical angles.  The same rotation carries each displacement
 * onto the surface it attaches to.  An untilted pattern keeps the resolved
 * displacement.
 */
  static void
ff_follow_display_rotation(const ff_rotation_t *rot, field_vector_t *vecs,
    int total)
{
  int idx;

  if( !ff_rotation_tilted(rot) )
    return;

  for( idx = 0; idx < total; idx++ )
  {
    double xr, yr, zr;

    ant_temp_rotate_vector((double)vecs[idx].dx, (double)vecs[idx].dy,
        (double)vecs[idx].dz, rot->axis_phi, rot->angle, &xr, &yr, &zr);

    vecs[idx].dx = (float)xr;
    vecs[idx].dy = (float)yr;
    vecs[idx].dz = (float)zr;
  }

} /* ff_follow_display_rotation() */

/*-----------------------------------------------------------------------*/

  field_frame_t
chroma_proj_frame_farfield(int fstep)
{
  field_frame_t out = { NULL, NULL, 0.0 };
  const point_3d_t *verts;
  ff_operator_t quantity;
  color_tone_t fam;
  tone_param_t tp;
  color_edge_t want;
  double phase, cos_ph, sin_ph, env_peak, ratio;
  int total, nth, nph, idx;

  if( rad_pattern == NULL || ff_pre == NULL || fstep < 0 || !save.fstep[fstep] )
    return out;

  total = fpat.nth * fpat.nph;
  if( total <= 0 || rad_pattern[fstep].phasor == NULL ||
      ff_pre[fstep].vertices == NULL )
    return out;

  /* State drives the phase and the selections; the resolver never mutates
   * the stored phasors */
  phase = (double)flow_phase;
  ratio = rc_config.ff_vector_length_ratio;
  fam   = color_tone_active();
  tone_param_init(&tp, fam);
  verts = ff_pre[fstep].vertices;

  /* The far-zone magnetic field is r_hat x E over the free-space impedance:
   * a quarter turn in the tangent plane, in time phase with the electric
   * field.  The impedance divides every cell alike while the arrow length
   * follows the surface and the color normalizes against this frame's own
   * peak, so it cancels and the turn is what remains. */
  if( rc_config.ff_quantity == FF_QTY_HFIELD )
    quantity = (ff_operator_t){ 0.0, -1.0, 1.0, 0.0 };
  else
    quantity = (ff_operator_t){ 1.0, 0.0, 0.0, 1.0 };

  /* Each arrow spans this fraction of its own cell radius, so the longest
   * spans the same fraction of the pattern radius */
  out.extent = (double)ff_pre[fstep].pattern_radius * ratio;

  /* The phase enters the edge, so a scrub back to a drawn phase hits the
   * cache.  The presentation generation enters it because the arrows attach
   * to the surface that generation placed, the polarization, reference, and
   * quantity because they select what is drawn, and the length ratio, tone
   * parameter, and dB floor so a slider change invalidates the frame. */
  want = (color_edge_t){ .fstep = fstep, .elem = calc_data.pol_type,
      .chan = rc_config.ff_quantity, .fam = (int)fam,
      .proj = rc_config.ff_frame, .phase = phase, .cmax = ratio,
      .param = tp.param, .flr = tp.floor_ratio,
      .freq_mhz = calc_data.freq_mhz, .gen_a = ff_pre[fstep].generation,
      .palette = color_palette_generation() };

  if( ff_edge_valid && color_edge_eq(&ff_edge, &want) )
  {
    out.vecs   = ff_vec_buf;
    out.colors = ff_col_buf;
    return out;
  }

  mem_array_realloc(&ff_vec_buf, total);
  mem_array_realloc(&ff_col_buf, total);
  mem_array_realloc(&ff_mag_buf, total);

  cos_ph   = cos(phase);
  sin_ph   = sin(phase);
  env_peak = 0.0;
  idx      = 0;

  /* Geometry pass: the selected component of the stored pair evaluated at the
   * phase and composed as e_th * theta_hat + e_ph * phi_hat, stretched to its
   * own cell's share of the pattern radius so the arrows follow whatever gain
   * style shaped the surface.  Scan the standing envelope peak for the
   * colorize pass; that peak carries no phase, so the color scale holds still
   * while the arrows sweep through it. */
  for( nph = 0; nph < fpat.nph; nph++ )
  {
    ff_operator_t op;
    double cs, sn;

    if( rc_config.ff_frame == FF_FRAME_LUDWIG3 )
    {
      /* Co-polar direction held fixed across the pattern by the cell azimuth */
      cs = geom_pre.cos_phi[nph];
      sn = geom_pre.sin_phi[nph];
    }
    else
    {
      /* World spherical basis, the linear pair left unturned */
      cs = 1.0;
      sn = 0.0;
    }

    op = ff_compose(quantity, ff_pol_operator(calc_data.pol_type, cs, sn));

    for( nth = 0; nth < fpat.nth; nth++ )
    {
      const ff_phasor_t *ph = &rad_pattern[fstep].phasor[idx];
      complex double eth = op.a * ph->eth + op.b * ph->eph;
      complex double eph = op.c * ph->eth + op.d * ph->eph;
      double env = sqrt(creal(eth * conj(eth)) + creal(eph * conj(eph)));
      double e_th = creal(eth) * cos_ph - cimag(eth) * sin_ph;
      double e_ph = creal(eph) * cos_ph - cimag(eph) * sin_ph;
      double len_scale, th_disp, ph_disp, th_xy;

      /* A cell carrying no field draws an arrow of no length */
      if( env > 0.0 )
        len_scale = ratio * verts[idx].r / env;
      else
        len_scale = 0.0;

      th_disp = e_th * len_scale;
      ph_disp = e_ph * len_scale;
      th_xy   = th_disp * geom_pre.cos_theta[nth];

      ff_vec_buf[idx].dx = (float)(th_xy * geom_pre.cos_phi[nph] -
                                   ph_disp * geom_pre.sin_phi[nph]);
      ff_vec_buf[idx].dy = (float)(th_xy * geom_pre.sin_phi[nph] +
                                   ph_disp * geom_pre.cos_phi[nph]);
      ff_vec_buf[idx].dz = (float)(-th_disp * geom_pre.sin_theta[nth]);

      ff_mag_buf[idx] = sqrt(e_th * e_th + e_ph * e_ph);
      if( env > env_peak )
        env_peak = env;

      idx++;
    }
  }

  /* Colorize pass: amplitude ramp of the instantaneous magnitude against the
   * standing envelope peak through the active tone */
  for( idx = 0; idx < total; idx++ )
    ff_col_buf[idx] = field_ramp_color(fam, &tp, ff_mag_buf[idx], env_peak);

  /* The vertices these arrows attach to may be tilted; follow that tilt */
  ff_follow_display_rotation(&ff_pre[fstep].rotation, ff_vec_buf, total);

  ff_edge       = want;
  ff_edge_valid = TRUE;

  out.vecs   = ff_vec_buf;
  out.colors = ff_col_buf;
  return out;

} /* chroma_proj_frame_farfield() */

/*-----------------------------------------------------------------------*/

  void
chroma_ff_free(void)
{
  mem_array_free(&ff_vec_buf);
  mem_array_free(&ff_col_buf);
  mem_array_free(&ff_mag_buf);
  ff_edge       = (color_edge_t){ 0 };
  ff_edge_valid = FALSE;

} /* chroma_ff_free() */

/*-----------------------------------------------------------------------*/

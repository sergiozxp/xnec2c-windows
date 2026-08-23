/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  The official website and doumentation for xnec2c is available here:
 *    https://www.xnec2c.org/
 */

/* Plot per-frequency solver precision loss from the shared measurement row. */

#include "../freqplots_internal.h"
#include "../../shared.h"

/* Reuse the solver-conditioning trace across rendered frames. */
static double *precision_lost = NULL;

/**
 * fp_cond_enabled() - report whether the conditioning panel is selected
 *
 * Return: nonzero when the panel is selected.
 */
  int
fp_cond_enabled(void)
{
  return rc_config.freqplots_cond_togglebutton;
}

/**
 * fp_cond_free() - release the solver-conditioning trace
 */
  void
fp_cond_free( void )
{
  mem_array_free( &precision_lost );

} /* fp_cond_free() */

/**
 * fp_cond_render() - render the solver-conditioning panel
 * @ctx: frame context carrying the per-frequency measurement rows
 *
 * Return: TRUE after depositing the panel.
 */
  gboolean
fp_cond_render(fp_plot_ctx_t *ctx)
{
  char *titles[3];

  titles[0] = _("Precision lost");
  titles[1] = _("Solver conditioning - precision lost in decimal digits");
  titles[2] = "        ";

  mem_array_realloc( &precision_lost, ctx->num_fsteps );

  fp_meas_column_t col = { precision_lost, MEAS_PRECISION_LOST };
  fp_fill_meas_columns( ctx, &col, 1 );

  fp_plot_panel( ctx, precision_lost, NULL, titles, FP_PANEL_COND );

  return TRUE;
}

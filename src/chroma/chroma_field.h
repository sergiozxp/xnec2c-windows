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

#ifndef CHROMA_FIELD_H
#define CHROMA_FIELD_H  1

#include "../prerender/prerender_state.h"
#include "../color/color_palette.h"
#include "../color/color_tone.h"

/* One draw's resolved arrays for one field set, chroma-owned and parallel:
 * geometry displacement beside its palette color, both derived at draw. */
typedef struct
{
  const field_vector_t *vecs;    /* [npts] pre-scaled displacement (geometry) */
  const rgb_f_t        *colors;  /* [npts] palette colors, parallel to vecs */
  double                extent;  /* longest displacement the frame draws */
} field_frame_t;

/**
 * field_ramp_color() - Amplitude-ramp colorize leaf
 * @fam: active tone family
 * @tp:  tone parameter for the family
 * @mag: point magnitude
 * @max: frame magnitude maximum
 *
 * The one field colorize: the family transfer of mag/max looked up on the
 * ramp palette, shared by the near-field and far-zone resolvers.
 */
static inline rgb_f_t
field_ramp_color(color_tone_t fam, const tone_param_t *tp, double mag, double max)
{
  return palette_lookup_scaled(palette_get(PALETTE_RAMP),
      color_tone_transfer_norm(fam, tp, mag, max), 1.0);
}

#endif

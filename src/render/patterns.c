#include "render.h"
#include <stdint.h>
#include <stddef.h>
#include "common/macros.h"

void
render_jl(uint32_t pixels[], uint32_t bg_argb, uint32_t width)
{
	uint8_t a = (bg_argb >> 24) & 0xff;
	uint8_t r = (bg_argb >> 16) & 0xff;
	uint8_t g = (bg_argb >> 8) & 0xff;;
	uint8_t b = bg_argb & 0xff;

	/* darker outline */
	uint8_t r0 = r / 2;
	uint8_t g0 = g / 2;
	uint8_t b0 = b / 2;

	/* highlight */
	uint8_t r1 = MIN(r * 5 / 4, a);
	uint8_t g1 = MIN(g * 5 / 4, a);
	uint8_t b1 = MIN(b * 5 / 4, a);

	uint32_t outline = ((uint32_t)a << 24) | (r0 << 16) | (g0 << 8) | b0;
	uint32_t highlight = ((uint32_t)a << 24) | (r1 << 16) | (g1 << 8) | b1;

	pixels[0] = outline;
	pixels[1] = highlight;
	for (size_t i = 2; i < width - 1; i++) {
		pixels[i] = bg_argb;
	}
	pixels[width - 1] = outline;
}

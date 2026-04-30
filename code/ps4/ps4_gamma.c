/*
 * ps4_gamma.c - PS4 gamma control (stub)
 *
 * Replaces code/sdl/sdl_gamma.c.
 * PS4 does not expose gamma ramp control through Piglet.
 * Gamma correction can be done in the fragment shader if needed.
 */

#include "../renderercommon/tr_common.h"

void GLimp_SetGamma(unsigned char red[256], unsigned char green[256],
	unsigned char blue[256])
{
	// No hardware gamma on PS4 via Piglet
}

#include "Convert.h"

enum {
	CLIP_SIZE = 811,
	CLIP_OFFSET = 277,
	YMUL = 298,
	RMUL = 409,
	BMUL = 516,
	G1MUL = -100,
	G2MUL = -208,
};

static int tables_initialized = 0;

static int yuv2rgb_y[256];
static int yuv2rgb_r[256];
static int yuv2rgb_b[256];
static int yuv2rgb_g1[256];
static int yuv2rgb_g2[256];

static unsigned long yuv2rgb_clip[CLIP_SIZE];
static unsigned long yuv2rgb_clip8[CLIP_SIZE];
static unsigned long yuv2rgb_clip16[CLIP_SIZE];

#define COMPOSE_RGB(yc, rc, gc, bc)		\
	( 0xff000000 |				\
	  yuv2rgb_clip16[(yc) + (rc)] |		\
	  yuv2rgb_clip8[(yc) + (gc)] |		\
	  yuv2rgb_clip[(yc) + (bc)] )

static void init_yuv2rgb_tables(void)
{
	int i;

	for (i = 0; i < 256; ++i) {
		yuv2rgb_y[i] = (YMUL * (i - 16) + 128) >> 8;
		yuv2rgb_r[i] = (RMUL * (i - 128)) >> 8;
		yuv2rgb_b[i] = (BMUL * (i - 128)) >> 8;
		yuv2rgb_g1[i] = (G1MUL * (i - 128)) >> 8;
		yuv2rgb_g2[i] = (G2MUL * (i - 128)) >> 8;
	}
	for (i = 0 ; i < CLIP_OFFSET; ++i) {
		yuv2rgb_clip[i] = 0;
		yuv2rgb_clip8[i] = 0;
		yuv2rgb_clip16[i] = 0;
	}
	for (; i < CLIP_OFFSET + 256; ++i) {
		yuv2rgb_clip[i] = i - CLIP_OFFSET;
		yuv2rgb_clip8[i] = (i - CLIP_OFFSET) << 8;
		yuv2rgb_clip16[i] = (i - CLIP_OFFSET) << 16;
	}
	for (; i < CLIP_SIZE; ++i) {
		yuv2rgb_clip[i] = 255;
		yuv2rgb_clip8[i] = 255 << 8;
		yuv2rgb_clip16[i] = 255 << 16;
	}

	tables_initialized = 1;
}

/*
 * Convert i420 to RGB32 (0xBBGGRRAA).
 * NOTE: size of dest must be >= width * height * 4
 *
 * This function uses precalculated tables that are initialized
 * on the first run.
 */
int
I420toRGB32(int width, int height, const char *src, char *dst)
{
	int i, j;
	unsigned int *dst_odd;
	unsigned int *dst_even;
	const unsigned char *u;
	const unsigned char *v;
	const unsigned char *y_odd;
	const unsigned char *y_even;

	if (!tables_initialized)
		init_yuv2rgb_tables();

	dst_even = (unsigned int *)dst;
	dst_odd = dst_even + width;

	y_even = (const unsigned char *)src;
	y_odd = y_even + width;
	u = y_even + width * height;
	v = u + ((width * height) >> 2);

	for (i = 0; i < height / 2; ++i) {
		for (j = 0; j < width / 2; ++j) {
			const int rc = yuv2rgb_r[*v];
			const int gc = yuv2rgb_g1[*v] + yuv2rgb_g2[*u];
			const int bc = yuv2rgb_b[*u];
			const int yc0_even = CLIP_OFFSET + yuv2rgb_y[*y_even++];
			const int yc1_even = CLIP_OFFSET + yuv2rgb_y[*y_even++];
			const int yc0_odd = CLIP_OFFSET + yuv2rgb_y[*y_odd++];
			const int yc1_odd = CLIP_OFFSET + yuv2rgb_y[*y_odd++];

			*dst_even++ = COMPOSE_RGB(yc0_even, bc, gc, rc);
			*dst_even++ = COMPOSE_RGB(yc1_even, bc, gc, rc);
			*dst_odd++ = COMPOSE_RGB(yc0_odd, bc, gc, rc);
			*dst_odd++ = COMPOSE_RGB(yc1_odd, bc, gc, rc);

			++u;
			++v;
		}

		y_even += width;
		y_odd += width;
		dst_even += width;
		dst_odd += width;
	}

	return 0;
}

/*
 * Convert RGB32 to i420. NOTE: size of dest must be >= width * height * 3 / 2
 * Based on formulas found at http://en.wikipedia.org/wiki/YUV  (libvidcap)
 */
int
RGB32toI420(int width, int height, const char *src, char *dst)
{
    int i, j;
    unsigned char *dst_y_even;
    unsigned char *dst_y_odd;
    unsigned char *dst_u;
    unsigned char *dst_v;
    const unsigned char *src_even;
    const unsigned char *src_odd;

    src_even = (const unsigned char *)src;
    src_odd = src_even + width * 4;

    dst_y_even = (unsigned char *)dst;
    dst_y_odd = dst_y_even + width;
    dst_u = dst_y_even + width * height;
    dst_v = dst_u + ((width * height) >> 2);

    for (i = 0; i < height / 2; ++i) {
        for (j = 0; j < width / 2; ++j) {
            short r, g, b;
            r = *src_even++;
            g = *src_even++;
            b = *src_even++;

            ++src_even;
            *dst_y_even++ = (unsigned char)
                ((( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16);
            *dst_u++ = (unsigned char)
                ((( r * -38 - g * 74 + b * 112 + 128 ) >> 8 ) + 128);
            *dst_v++ = (unsigned char)
                ((( r * 112 - g * 94 - b * 18 + 128 ) >> 8 ) + 128);

            r = *src_even++;
            g = *src_even++;
            b = *src_even++;
            ++src_even;
            *dst_y_even++ = (unsigned char)
                ((( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;
            ++src_odd;
            *dst_y_odd++ = (unsigned char)
                ((( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16);

            r = *src_odd++;
            g = *src_odd++;
            b = *src_odd++;
            ++src_odd;
            *dst_y_odd++ = (unsigned char)
                ((( r * 66 + g * 129 + b * 25 + 128 ) >> 8 ) + 16);
        }

        dst_y_odd += width;
        dst_y_even += width;
        src_odd += width * 4;
        src_even += width * 4;
    }

    return 0;
}


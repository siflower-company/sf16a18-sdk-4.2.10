#ifndef _UAPI_SFAX8_FB_H
#define _UAPI_SFAX8_FB_H

#include <linux/types.h>
#include <asm/ioctl.h>

struct fb_fillrect;

enum fb_bpp_mode {
	pallet_1_bpp = 0,
	pallet_2_bpp = 1,
	pallet_4_bpp = 2,
	pallet_8_bpp = 3,
	a1_rgb232    = 4,
	rgb565       = 5,
	a1_rgb555    = 6,
	i1_rgb555    = 7,
	rgb666       = 8,
	a1_rgb665    = 9,
	a1_rgb666    = 10,
	rgb888       = 11,
	a1_rgb887    = 12,
	a1_rgb888    = 13,
	a4_rgb888    = 14,
	a4_rgb444    = 15,
	a8_rgb888    = 16,
	ycbcr_420    = 17,
	rgb888_a8    = 18,
	rgb555_a1    = 19,
	rgb555_i1    = 20,
	bpp_mode_unknown = 21,
};

enum blend_category {
	per_plane,
	per_pixel,
};

struct sfax8_fb_alpha {
	enum blend_category blend_category;

	/* When Per plane blending case( blend_category == per_plane)
	 *	0 = using alpha_0 values
	 *	1 = using alpha_1 values
	 * When Per pixel blending ( blend_category == per_pixel)
	 *	0 = selected by AEN (A value) or Chroma key
	 *	1 = using DATA[27:24] data (when BPPMODE=a4_rgb888
	 *	    &a4_rgb444), using DATA[31:24] data (when
	 *	    BPPMODE=a8_rgb888)
	 */
#define USING_ALPHA_0 0
#define USING_ALPHA_1 1
#define USING_AEN 0
#define USING_DATA_BITS 1
	__u8 alpha_sel;

	/* used when blend_category == per_plane, and has no meaning
	 * when blend_category == per_pixel
	 * alpha_0/1[8:11] = Red alpha value
	 * alpha_0/1[4:7] = Green alpha value
	 * alpha_0/1[0:3] = Blue alpha value
	 */
#define ALPHA(r, g, b) ((((r) & 0xf) << 8) | \
			(((g) & 0xf) << 4) | \
			(((b) & 0xf) << 0))
	__u16 alpha_0;
	__u16 alpha_1;
};

/* sfax8 framebuffer ioctls */
#define SFFB_GET_ALPHA	_IOR('F', 0x40, struct sfax8_fb_alpha)
#define SFFB_PUT_ALPHA	_IOW('F', 0x41, struct sfax8_fb_alpha)
#define SFFB_FILLRECT	_IOW('F', 0x42, struct fb_fillrect)

#endif

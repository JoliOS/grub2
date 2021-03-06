/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/env.h>
#include <grub/misc.h>
#include <grub/xnu.h>
#include <grub/mm.h>
#include <grub/cpu/xnu.h>
#include <grub/video_fb.h>
#include <grub/bitmap_scale.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

#define DEFAULT_VIDEO_MODE "auto"

/* Setup video for xnu. */
grub_err_t
grub_xnu_set_video (struct grub_xnu_boot_params *params)
{
  struct grub_video_mode_info mode_info;
  int ret;
  char *tmp;
  const char *modevar;
  void *framebuffer;
  grub_err_t err;
  struct grub_video_bitmap *bitmap = NULL;

  modevar = grub_env_get ("gfxpayload");
  /* Consider only graphical 32-bit deep modes.  */
  if (! modevar || *modevar == 0)
    err = grub_video_set_mode (DEFAULT_VIDEO_MODE,
			       GRUB_VIDEO_MODE_TYPE_PURE_TEXT
			       | GRUB_VIDEO_MODE_TYPE_DEPTH_MASK,
			       32 << GRUB_VIDEO_MODE_TYPE_DEPTH_POS);
  else
    {
      tmp = grub_xasprintf ("%s;" DEFAULT_VIDEO_MODE, modevar);
      if (! tmp)
	return grub_errno;
      err = grub_video_set_mode (tmp,
				 GRUB_VIDEO_MODE_TYPE_PURE_TEXT
				 | GRUB_VIDEO_MODE_TYPE_DEPTH_MASK,
				 32 << GRUB_VIDEO_MODE_TYPE_DEPTH_POS);
      grub_free (tmp);
    }

  if (err)
    return err;

  ret = grub_video_get_info (&mode_info);
  if (ret)
    return grub_error (GRUB_ERR_IO, "couldn't retrieve video parameters");

  if (grub_xnu_bitmap)
    {
      if (grub_xnu_bitmap_mode == GRUB_XNU_BITMAP_STRETCH)
	err = grub_video_bitmap_create_scaled (&bitmap,
					       mode_info.width,
					       mode_info.height,
					       grub_xnu_bitmap,
					       GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST);
      else
	bitmap = grub_xnu_bitmap;
    }

  if (bitmap)
    {
      int x, y;

      x = mode_info.width - bitmap->mode_info.width;
      x /= 2;
      y = mode_info.height - bitmap->mode_info.height;
      y /= 2;
      err = grub_video_blit_bitmap (bitmap,
				    GRUB_VIDEO_BLIT_REPLACE,
				    x > 0 ? x : 0,
				    y > 0 ? y : 0,
				    x < 0 ? -x : 0,
				    y < 0 ? -y : 0,
				    min (bitmap->mode_info.width,
					 mode_info.width),
				    min (bitmap->mode_info.height,
					 mode_info.height));
    }
  if (err)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      bitmap = 0;
    }

  ret = grub_video_get_info_and_fini (&mode_info, &framebuffer);
  if (ret)
    return grub_error (GRUB_ERR_IO, "couldn't retrieve video parameters");

  params->lfb_width = mode_info.width;
  params->lfb_height = mode_info.height;
  params->lfb_depth = mode_info.bpp;
  params->lfb_line_len = mode_info.pitch;

  params->lfb_base = PTR_TO_UINT32 (framebuffer);
  params->lfb_mode = bitmap ? GRUB_XNU_VIDEO_SPLASH 
    : GRUB_XNU_VIDEO_TEXT_IN_VIDEO;

  return GRUB_ERR_NONE;
}


/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PAINT_FUNCS_H__
#define __PAINT_FUNCS_H__


/*  copy gray pixels to intensity-alpha pixels.  This function
 *  essentially takes a source that is only a grayscale image and
 *  copies it to the destination, expanding to RGB if necessary and
 *  adding an alpha channel.  (OPAQUE)
 */
void  copy_gray_to_inten_a_pixels         (const guchar *src,
                                           guchar       *dest,
                                           guint         length,
                                           guint         bytes);

/*  lay down the initial pixels in the case of only one
 *  channel being visible and no layers...In this singular
 *  case, we want to display a grayscale image w/o transparency
 */
void  initial_channel_pixels              (const guchar *src,
                                           guchar       *dest,
                                           guint         length,
                                           guint         bytes);

/*  lay down the initial pixels in the case of an indexed image.
 *  This process obviously requires no composition
 */
void  initial_indexed_pixels              (const guchar *src,
                                           guchar       *dest,
                                           const guchar *cmap,
                                           guint         length);

/*  lay down the initial pixels in the case of an indexed image.
 *  This process obviously requires no composition
 */
void  initial_indexed_a_pixels            (const guchar *src,
                                           guchar       *dest,
                                           const guchar *mask,
                                           const guchar *no_mask,
                                           const guchar *cmap,
                                           guint         opacity,
                                           guint         length);

/*  lay down the initial pixels for the base layer.
 *  This process obviously requires no composition.
 */
void  initial_inten_pixels                (const guchar   *src,
                                           guchar         *dest,
                                           const guchar   *mask,
                                           const guchar   *no_mask,
                                           guint           opacity,
                                           const gboolean *affect,
                                           guint           length,
                                           guint           bytes);

/*  lay down the initial pixels for the base layer.
 *  This process obviously requires no composition.
 */
void  initial_inten_a_pixels              (const guchar   *src,
                                           guchar         *dest,
                                           const guchar   *mask,
                                           guint           opacity,
                                           const gboolean *affect,
                                           guint           length,
                                           guint           bytes);

/*  combine indexed images with an optional mask which
 *  is interpreted as binary...destination is indexed...
 */
void  combine_indexed_and_indexed_pixels  (const guchar   *src1,
                                           const guchar   *src2,
                                           guchar         *dest,
                                           const guchar   *mask,
                                           guint           opacity,
                                           const gboolean *affect,
                                           guint           length,
                                           guint           bytes);

/*  combine indexed images with indexed-alpha images
 *  result is an indexed image
 */
void  combine_indexed_and_indexed_a_pixels (const guchar *src1,
                                            const guchar *src2,
                                            guchar       *dest,
                                            const guchar *mask,
                                            guint         opacity,
                                            const gint   *affect,
                                            guint         length,
                                            guint         bytes);

/*  combine indexed-alpha images with indexed-alpha images
 *  result is an indexed-alpha image.  use this for painting
 *  to an indexed floating sel
 */
void  combine_indexed_a_and_indexed_a_pixels(const guchar   *src1,
                                             const guchar   *src2,
                                             guchar         *dest,
                                             const guchar   *mask,
                                             guint           opacity,
                                             const gboolean *affect,
                                             guint           length,
                                             guint           bytes);

void combine_inten_a_and_indexed_pixels     (const guchar   *src1,
                                             const guchar   *src2,
                                             guchar         *dest,
                                             const guchar   *mask,
                                             const guchar   *cmap,
                                             guint           opacity,
                                             guint           length,
                                             guint           bytes);

/*  combine intensity with indexed, destination is
 *  intensity-alpha...use this for an indexed floating sel
 */
void  combine_inten_a_and_indexed_a_pixels (const guchar *src1,
                                            const guchar *src2,
                                            guchar       *dest,
                                            const guchar *mask,
                                            const guchar *cmap,
                                            guint         opacity,
                                            guint         length,
                                            guint         bytes);

/*  combine RGB image with RGB or GRAY with GRAY
 *  destination is intensity-only...
 */
void  combine_inten_and_inten_pixels       (const guchar   *src1,
                                            const guchar   *src2,
                                            guchar         *dest,
                                            const guchar   *mask,
                                            guint           opacity,
                                            const gboolean *affect,
                                            guint           length,
                                            guint           bytes);

/*  combine an RGBA or GRAYA image with an RGB or GRAY image
 *  destination is intensity-only...
 */
void  combine_inten_and_inten_a_pixels    (const guchar   *src1,
                                           const guchar   *src2,
                                           guchar         *dest,
                                           const guchar   *mask,
                                           guint           opacity,
                                           const gboolean *affect,
                                           guint           length,
                                           guint           bytes);

/*  combine an RGB or GRAY image with an RGBA or GRAYA image
 *  destination is intensity-alpha...
 */
void  combine_inten_a_and_inten_pixels    (const guchar   *src1,
                                           const guchar   *src2,
                                           guchar         *dest,
                                           const guchar   *mask,
                                           guint           opacity,
                                           const gboolean *affect,
                                           gboolean        mode_affect,
                                           guint           length,
                                           guint           bytes);

/*  combine an RGBA or GRAYA image with an RGBA or GRAYA image
 *  destination is of course intensity-alpha...
 */
void  combine_inten_a_and_inten_a_pixels   (const guchar   *src1,
                                            const guchar   *src2,
                                            guchar         *dest,
                                            const guchar   *mask,
                                            guint           opacity,
                                            const gboolean *affect,
                                            gboolean        mode_affect,
                                            guint           length,
                                            guint           bytes);

/*  combine a channel with intensity-alpha pixels based
 *  on some opacity, and a channel color...
 *  destination is intensity-alpha
 */
void  combine_inten_a_and_channel_mask_pixels(const guchar *src,
                                              const guchar *channel,
                                              guchar       *dest,
                                              const guchar *col,
                                              guint         opacity,
                                              guint         length,
                                              guint         bytes);

void  combine_inten_a_and_channel_selection_pixels(const guchar *src,
                                                   const guchar *channel,
                                                   guchar       *dest,
                                                   const guchar *col,
                                                   guint         opacity,
                                                   guint         length,
                                                   guint         bytes);

void  paint_funcs_color_erase_helper      (GimpRGB       *src,
                                           const GimpRGB *color);


/*  Region functions  */

void  convolve_region                     (PixelRegion         *srcR,
                                           PixelRegion         *destR,
                                           const gfloat        *matrix,
                                           gint                 size,
                                           gdouble              divisor,
                                           GimpConvolutionType  mode,
                                           gboolean             alpha_weighting);

void  smooth_region                       (PixelRegion *region);
void  erode_region                        (PixelRegion *region);
void  dilate_region                       (PixelRegion *region);


/*  Copy a gray image to an intensity-alpha region  */
void  copy_gray_to_region                 (PixelRegion *src,
                                           PixelRegion *dest);

void  initial_region                      (PixelRegion    *src,
                                           PixelRegion    *dest,
                                           PixelRegion    *mask,
                                           const guchar   *data,
                                           guint           opacity,
                                           GimpLayerModeEffects  mode,
                                           const gboolean *affect,
                                           InitialMode     type);

void  combine_regions                     (PixelRegion    *src1,
                                           PixelRegion    *src2,
                                           PixelRegion    *dest,
                                           PixelRegion    *mask,
                                           const guchar   *data,
                                           guint           opacity,
                                           GimpLayerModeEffects  mode,
                                           const gboolean *affect,
                                           CombinationMode type);

void  combine_regions_replace             (PixelRegion    *src1,
                                           PixelRegion    *src2,
                                           PixelRegion    *dest,
                                           PixelRegion    *mask,
                                           const guchar   *data,
                                           guint           opacity,
                                           const gboolean *affect,
                                           CombinationMode type);


#endif  /*  __PAINT_FUNCS_H__  */

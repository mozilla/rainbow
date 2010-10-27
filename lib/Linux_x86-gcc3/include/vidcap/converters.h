/*
 * libvidcap - a cross-platform video capture library
 *
 * Copyright 2007 Wimba, Inc.
 *
 * Contributors:
 * Peter Grayson <jpgrayson@gmail.com>
 * Bill Cholewka <bcholew@gmail.com>
 *
 * libvidcap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * libvidcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _CONVERTERS_H
#define _CONVERTERS_H

#ifdef __cplusplus
extern "C" {
#endif

int
vidcap_i420_to_rgb32(int width, int height, const char * src, char * dest);

int
vidcap_i420_to_yuy2(int width, int height, const char * src, char * dest);

int
vidcap_yuy2_to_i420(int width, int height, const char * src, char * dest);

int
vidcap_yuy2_to_rgb32(int width, int height, const char * src, char * dest);

int
vidcap_rgb32_to_i420(int width, int height, const char * src, char * dest);

int
vidcap_rgb32_to_yuy2(int width, int height, const char * src, char * dest);

#ifdef __cplusplus
}
#endif

#endif

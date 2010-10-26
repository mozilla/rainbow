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

#ifndef _VIDCAP_H
#define _VIDCAP_H

#ifdef __cplusplus
extern "C" {
#endif

#define VIDCAP_NAME_LENGTH 256

enum vidcap_fourccs {
	VIDCAP_FOURCC_I420   = 100,
	VIDCAP_FOURCC_YUY2   = 101,
	VIDCAP_FOURCC_RGB32  = 102,
};

enum vidcap_log_level {
	VIDCAP_LOG_NONE  = 0,
	VIDCAP_LOG_ERROR = 10,
	VIDCAP_LOG_WARN  = 20,
	VIDCAP_LOG_INFO  = 30,
	VIDCAP_LOG_DEBUG = 40
};

typedef void vidcap_state;
typedef void vidcap_sapi;
typedef void vidcap_src;

struct vidcap_sapi_info
{
	char identifier[VIDCAP_NAME_LENGTH];
	char description[VIDCAP_NAME_LENGTH];
};

struct vidcap_src_info
{
	char identifier[VIDCAP_NAME_LENGTH];
	char description[VIDCAP_NAME_LENGTH];
};

struct vidcap_fmt_info
{
	int width;
	int height;
	int fourcc;
	int fps_numerator;
	int fps_denominator;
};

struct vidcap_capture_info
{
	const char * video_data;
	int video_data_size;
	int error_status;
	long capture_time_sec;
	long capture_time_usec;
};

typedef int (*vidcap_src_capture_callback) (vidcap_src *,
				void * user_data,
				struct vidcap_capture_info * cap_info);

typedef int (*vidcap_sapi_notify_callback) (vidcap_sapi *, void * user_data);

vidcap_state *
vidcap_initialize(void);

void
vidcap_destroy(vidcap_state *);

int
vidcap_log_level_set(enum vidcap_log_level level);

int
vidcap_sapi_enumerate(vidcap_state *,
		int index,
		struct vidcap_sapi_info *);

vidcap_sapi *
vidcap_sapi_acquire(vidcap_state *,
		const struct vidcap_sapi_info *);

int
vidcap_sapi_release(vidcap_sapi *);

int
vidcap_sapi_info_get(vidcap_sapi *,
		struct vidcap_sapi_info *);

int
vidcap_srcs_notify(vidcap_sapi *,
		vidcap_sapi_notify_callback callback,
		void * user_data);

int
vidcap_src_list_update(vidcap_sapi * sapi);

int
vidcap_src_list_get(vidcap_sapi * sapi,
		int list_len,
		struct vidcap_src_info * src_list);

vidcap_src *
vidcap_src_acquire(vidcap_sapi *,
		const struct vidcap_src_info *);

int
vidcap_src_release(vidcap_src *);

int
vidcap_src_info_get(vidcap_src * src,
		struct vidcap_src_info * src_info);

int
vidcap_format_enumerate(vidcap_src *,
		int index,
		struct vidcap_fmt_info *);

int
vidcap_format_bind(vidcap_src *,
		const struct vidcap_fmt_info *);

int
vidcap_format_info_get(vidcap_src * src,
		struct vidcap_fmt_info * fmt_info);

int
vidcap_src_capture_start(vidcap_src *,
		vidcap_src_capture_callback callback,
		void * user_data);

int
vidcap_src_capture_stop(vidcap_src *);

const char *
vidcap_fourcc_string_get(int fourcc);

#ifdef __cplusplus
}
#endif

#endif

/*
 * libmedia-thumbnail
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunjun Ko <zzoon.ko@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "media-util.h"
#include <tzplatform_config.h>

#ifndef _MEDIA_THUMB_UTIL_H_
#define _MEDIA_THUMB_UTIL_H_

#define SAFE_FREE(src)      { if(src) {free(src); src = NULL;}}
#define THUMB_MALLOC(src, size)	{ if (size <= 0) {src = NULL;} \
							else { src = malloc(size); if(src) memset(src, 0x0, size);} }

typedef enum {
	MEDIA_THUMB_BGRA,			/* BGRA, especially provided for evas users */
	MEDIA_THUMB_RGB888,			/* RGB888 */
} media_thumb_format;

#define THUMB_NONE_TYPE    -1	/* None */
#define THUMB_IMAGE_TYPE   0	/* Image */
#define THUMB_VIDEO_TYPE   1	/* Video */

#define THUMB_PATH_PHONE 	MEDIA_ROOT_PATH_INTERNAL 	/**< File path prefix of files stored in phone */
#define THUMB_PATH_MMC 		MEDIA_ROOT_PATH_SDCARD		/**< File path prefix of files stored in mmc card */

#define THUMB_PHONE_PATH	tzplatform_mkpath(TZ_USER_SHARE, "media/.thumb/phone")
#define THUMB_MMC_PATH		tzplatform_mkpath(TZ_USER_SHARE, "media/.thumb/mmc")

#define THUMB_DEFAULT_PATH	tzplatform_mkpath(TZ_USER_SHARE, "media/.thumb/thumb_default.png")

typedef enum
{
	THUMB_PHONE,			/**< Stored only in phone */
	THUMB_MMC				/**< Stored only in MMC */
} media_thumb_store_type;

int _media_thumb_get_store_type_by_path(const char *full_path);

int _media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len);

int _media_thumb_get_file_type(const char *file_full_path);

char* _media_thumb_generate_hash_name(const char *file);

int _media_thumb_get_hash_name(const char *file_full_path, char *thumb_hash_path, size_t max_thumb_path, uid_t uid);

int _media_thumb_remove_file(const char *path);

#endif /*_MEDIA_THUMB_UTIL_H_*/


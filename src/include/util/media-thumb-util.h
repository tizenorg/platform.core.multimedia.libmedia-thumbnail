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

#include "media-thumb-types.h"
#include "media-thumb-debug.h"
#include "media-util.h"
#include <tzplatform_config.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifndef _MEDIA_THUMB_UTIL_H_
#define _MEDIA_THUMB_UTIL_H_

#define SAFE_FREE(src)      { if(src) {free(src); src = NULL;}}

#define THUMB_NONE_TYPE    -1	/* None */
#define THUMB_IMAGE_TYPE   0	/* Image */
#define THUMB_VIDEO_TYPE   1	/* Video */

#define THUMB_PATH_PHONE 	MEDIA_ROOT_PATH_INTERNAL 	/**< File path prefix of files stored in phone */
#define THUMB_PATH_MMC 		MEDIA_ROOT_PATH_SDCARD		/**< File path prefix of files stored in mmc card */

#define THUMB_PHONE_PATH	tzplatform_mkpath(TZ_USER_DATA, "file-manager-service/.thumb/phone")
#define THUMB_MMC_PATH		tzplatform_mkpath(TZ_USER_DATA, "file-manager-service/.thumb/mmc")

#define THUMB_DEFAULT_PATH	tzplatform_mkpath(TZ_USER_DATA, "file-manager-service/.thumb/thumb_default.png")

typedef enum
{
	THUMB_PHONE,			/**< Stored only in phone */
	THUMB_MMC				/**< Stored only in MMC */
} media_thumb_store_type;

int _media_thumb_get_length(media_thumb_type thumb_type);

int _media_thumb_get_store_type_by_path(const char *full_path);

int _media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len);

int _media_thumb_get_file_type(const char *file_full_path);

int _media_thumb_remove_file(const char *path);

char* _media_thumb_generate_hash_name(const char *file);

int _media_thumb_get_hash_name(const char *file_full_path, char *thumb_hash_path, size_t max_thumb_path, uid_t uid);

int _media_thumb_save_to_file_with_gdk(GdkPixbuf *data,
											int w,
											int h,
											gboolean alpha,
											char *thumb_path);

int _media_thumb_get_width(media_thumb_type thumb_type);

int _media_thumb_get_height(media_thumb_type thumb_type);

#endif /*_MEDIA_THUMB_UTIL_H_*/


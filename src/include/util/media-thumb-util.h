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

#include "media-thumb-error.h"
#include "media-thumb-types.h"
#include "media-thumb-debug.h"

#ifndef _MEDIA_THUMB_UTIL_H_
#define _MEDIA_THUMB_UTIL_H_

#define SAFE_FREE(src)      { if(src) {free(src); src = NULL;}}

#define THUMB_NONE_TYPE    0x00000001  /* Image */
#define THUMB_IMAGE_TYPE    0x00000001  /* Image */
#define THUMB_VIDEO_TYPE    0x00000002  /* Image */

#define THUMB_PATH_PHONE 	"/opt/media" 	/**< File path prefix of files stored in phone */
#define THUMB_PATH_MMC 	"/opt/storage/sdcard"	/**< File path prefix of files stored in mmc card */

#define THUMB_PHONE_PATH	"/opt/data/file-manager-service/.thumb/phone"
#define THUMB_MMC_PATH		"/opt/data/file-manager-service/.thumb/mmc"

#define THUMB_DEFAULT_PATH		"/opt/data/file-manager-service/.thumb/thumb_default.png"

typedef enum
{
	THUMB_PHONE,			/**< Stored only in phone */
	THUMB_MMC				/**< Stored only in MMC */	
} media_thumb_store_type;

int _media_thumb_get_length(media_thumb_type thumb_type);

int _media_thumb_get_store_type_by_path(const char *full_path);

int
_media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len);

int
_media_thumb_get_file_type(const char *file_full_path);

char
*_media_thumb_generate_hash_name(const char *file);

int
_media_thumb_get_hash_name(const char *file_full_path,
				 char *thumb_hash_path, size_t max_thumb_path);

int
_media_thumb_save_to_file_with_evas(unsigned char *data, 
											int w,
											int h,
											char *thumb_path);

int
_media_thumb_get_width(media_thumb_type thumb_type);

int
_media_thumb_get_height(media_thumb_type thumb_type);

/**
 *	_thumbnail_get_data:
 *  This function generates thumbnail raw data, which is wanted by user
 *  This api is closed for a while until being independent from evas object to be thread-safe
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *  @param[in]				origin_path      The path of the original image
 *  @param[in]				thumb_type       The type of the returned thumbnail data
 *  @param[in]				format           The format of the returned data
 *  @param[out]				data             The data of generated thumbnail.
 *  @param[out]				size             The size of generated thumbnail.
 *  @param[out]				width             The width of generated thumbnail.
 *  @param[out]				height             The height of generated thumbnail.
 *  @param[out]				origin_width        The width of original image.
 *  @param[out]				origin_height       The height of original image.
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 *	@remark		None.	
 * 	@par example
 * 	@code

#include <media-thumbnail.h>

void test_get_thumb_data()
{
	int ret = MEDIA_THUMB_ERROR_NONE;
	int thumb_type = 0;
	const char *origin_path = "/opt/media/test.jpg";
	void *data = NULL;
	

	ret = _thumbnail_get_data(origin_path, thumb_type, thumb_path, &data);

	if (ret < 0)
	{
		printf( "_thumbnail_get_data fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
*/
int _thumbnail_get_data(const char *origin_path,
						media_thumb_type thumb_type,
						media_thumb_format format,
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						int *origin_width,
						int *origin_height);

#endif /*_MEDIA_THUMB_UTIL_H_*/


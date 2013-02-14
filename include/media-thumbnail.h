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


#ifndef _MEDIA_THUMBNAIL_H_
#define _MEDIA_THUMBNAIL_H_

#include "media-thumb-types.h"
#include "media-thumb-error.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
	@defgroup	THUMB_GENERATE	Thumbnail generator
	 @{
	  * @file			media-thumbnail.h
	  * @brief		This file defines API's for thumbnail generator.
	  * @version	 	1.0
 */

/**
        @defgroup THUMB_API    Thumbnail generator API
        @{

        @par
        generates thumbnail data and return the path of the generated thumbnail file.
 */


/**
 * Callback function, which is used to call thumbnail_request_from_db_async
 */

typedef int (*ThumbFunc) (int error_code, char* path, void* data);


/**
 *	thumbnail_request_from_db:
 * 	This function connects to the media database and find thumbnail path of the passed original image. 
 *  If found, the thumbnail path will be returned, or starts to generate thumbnail
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *  @param[in]				origin_path      The path of the original image
 *  @param[out]				thumb_path       The path of generated thumbnail image.
 *  @param[in]				max_length       The max length of the returned thumbnail path.
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 *	@remark	The database name is "/opt/usr/dbspace/.media.db".
 * 	@par example
 * 	@code

#include <media-thumbnail.h>

void gen_thumbs()
{
	int ret = MEDIA_THUMB_ERROR_NONE;
	const char *origin_path = "/opt/usr/media/test.jpg";
	char thumb_path[255];

	ret = thumbnail_request_from_db(origin_path, thumb_path, 255);

	if (ret < 0)
	{
		printf( "thumbnail_request_from_db fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
 */
int thumbnail_request_from_db(const char *origin_path, char *thumb_path, int max_length);

/**
 *	thumbnail_request_from_db_async:
 * 	This function connects to the media database and find thumbnail path of the passed original image. 
 *  If found, the thumbnail path will be returned through callback, which is registered by user.
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *  @param[in]				origin_path     The path of the original image
 *  @param[in]				func            The callback, which is registered by user
 *  @param[in]				user_data       User data, which is used by user in callback
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 *	@remark	The database name is "/opt/usr/dbspace/.media.db".
 * 	@par example
 * 	@code

#include <media-thumbnail.h>

int _thumb_cb(int error_code, char *path, void *user_data)
{
	printf("Error code : %d\n", error_code);
	printf("Thumb path : %s\n", path);
}

void gen_thumbs()
{
	int ret = MEDIA_THUMB_ERROR_NONE;
	const char *origin_path = "/opt/usr/media/test.jpg";
	char thumb_path[255];

	ret = thumbnail_request_from_db_async(origin_path, _thumb_cb, NULL);

	if (ret < 0)
	{
		printf( "thumbnail_request_from_db_async fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
 */
int thumbnail_request_from_db_async(const char *origin_path, ThumbFunc func, void *user_data);

/**
 *	thumbnail_request_save_to_file:
 * 	This function generates thumbnail of the original path and save it to the file system as jpeg format with the passed thumbnail path.
 *  This function doesn't care about media DB.
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *  @param[in]				origin_path      The path of the original image
 *  @param[in]				thumb_type       The type of the returned thumbnail data.
 *  @param[in]				thumb_path       The path of generated thumbnail image.
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 * 	@par example
 * 	@code

#include <media-thumbnail.h>

void save_thumbs()
{
	int ret = MEDIA_THUMB_ERROR_NONE;
	const char *origin_path = "/opt/usr/media/test.jpg";
	const char *thumb_path = "/my_dir/thumb.jpg";

	ret = thumbnail_request_save_to_file(origin_path, thumb_path);

	if (ret < 0) {
		printf( "thumbnail_request_save_to_file fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
 */
int thumbnail_request_save_to_file(const char *origin_path,
									media_thumb_type thumb_type,
									const char *thumb_path);


/**
 *	thumbnail_request_extract_all_thumbs:
 * 	This function generates thumbnail of all media, which don't have thumbnail yet and save it to the file system as jpeg format.
 *  Once thumbnail generated, the thumbnail path and original image's width and height will be updated to media database.
 *
 *	@return		This function returns zero(MEDIA_THUMB_ERROR_NONE) on success, or negative value with error code.
 *				Please refer 'media-thumb-error.h' to know the exact meaning of the error.
 *	@see		None.
 *	@pre		None.
 *	@post		None.
 * 	@par example
 * 	@code

#include <media-thumbnail.h>

void extract_all_thumbs()
{
	int ret = MEDIA_THUMB_ERROR_NONE;

	ret = thumbnail_request_extract_all_thumbs();

	if (ret < 0) {
		printf( "thumbnail_request_extract_all_thumbs fails. error code->%d", ret);
	}

	return;
}

 * 	@endcode
 */

int thumbnail_request_extract_all_thumbs(void);

int thumbnail_request_from_db_with_size(const char *origin_path, char *thumb_path, int max_length, int *origin_width, int *origin_height);

/* Cancel APIs that a request to extract thumbnail */
int thumbnail_request_cancel_media(const char *origin_path);

/* Cancel APIs that all requests to extract thumbnail of a process */
int thumbnail_request_cancel_all();

/** @} */

/**
	@}
 */

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_THUMBNAIL_H_*/


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

#include "media-thumbnail.h"
#include "media-thumb-debug.h"
#include "media-thumb-util.h"
#include "media-thumb-internal.h"
#include "media-thumb-ipc.h"
//#include "media-thumb-db.h"

#include <glib.h>

int thumbnail_request_from_db(const char *origin_path, char *thumb_path, int max_length)
{
	int err = -1;
	//int need_update_db = 0;
	media_thumb_info thumb_info;

	if (origin_path == NULL || thumb_path == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (max_length <= 0) {
		thumb_err("Length is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	int store_type = -1;
	store_type = _media_thumb_get_store_type_by_path(origin_path);

	if ((store_type != THUMB_PHONE) && (store_type != THUMB_MMC)) {
		thumb_err("origin path(%s) is invalid", origin_path);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_err("Path : %s", origin_path);
/*
	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	err = _media_thumb_get_thumb_from_db(origin_path, thumb_path, max_length, &need_update_db);
	if (err == 0) {
		_media_thumb_db_disconnect();
		return MEDIA_THUMB_ERROR_NONE;
	}
*/
	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_DB_INSERT, MEDIA_THUMB_LARGE, origin_path, thumb_path, max_length, &thumb_info);
	if (err < 0) {
		thumb_err("_media_thumb_request failed : %d", err);
		//_media_thumb_db_disconnect();
		return err;
	}
/*
	// Need to update DB once generating thumb is done
	if (need_update_db) {
		err = _media_thumb_update_db(origin_path, thumb_path, thumb_info.origin_width, thumb_info.origin_height);
		if (err < 0) {
			thumb_err("_media_thumb_update_db failed : %d", err);
		}
	}

	_media_thumb_db_disconnect();
*/
	return MEDIA_THUMB_ERROR_NONE;
}

int thumbnail_request_save_to_file(const char *origin_path, media_thumb_type thumb_type, const char *thumb_path)
{
	int err = -1;

	if (origin_path == NULL || thumb_path == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	media_thumb_info thumb_info;
	char tmp_thumb_path[MAX_PATH_SIZE] = {0,};

	strncpy(tmp_thumb_path, thumb_path, sizeof(tmp_thumb_path));

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_SAVE_FILE, thumb_type, origin_path, tmp_thumb_path, sizeof(tmp_thumb_path), &thumb_info);
	if (err < 0) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int thumbnail_request_from_db_with_size(const char *origin_path, char *thumb_path, int max_length, int *origin_width, int *origin_height)
{
	int err = -1;
	//int need_update_db = 0;
	media_thumb_info thumb_info;

	if (origin_path == NULL || thumb_path == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (origin_width == NULL || origin_height == NULL) {
		thumb_err("Invalid parameter ( width or height )");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (max_length <= 0) {
		thumb_err("Length is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	int store_type = -1;
	store_type = _media_thumb_get_store_type_by_path(origin_path);

	if ((store_type != THUMB_PHONE) && (store_type != THUMB_MMC)) {
		thumb_err("origin path(%s) is invalid", origin_path);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_err("Path : %s", origin_path);
/*
	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	err = _media_thumb_get_thumb_from_db_with_size(origin_path, thumb_path, max_length, &need_update_db, &width, &height);
	if (err == 0) {
		_media_thumb_db_disconnect();
		return MEDIA_THUMB_ERROR_NONE;
	}
*/
	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_DB_INSERT, MEDIA_THUMB_LARGE, origin_path, thumb_path, max_length, &thumb_info);
	if (err < 0) {
		thumb_err("_media_thumb_request failed : %d", err);
		//_media_thumb_db_disconnect();
		return err;
	}

	*origin_width = thumb_info.origin_width;
	*origin_height = thumb_info.origin_height;

	//_media_thumb_db_disconnect();
	return MEDIA_THUMB_ERROR_NONE;
}

int thumbnail_request_extract_all_thumbs(void)
{
	int err = -1;

	media_thumb_info thumb_info;
	media_thumb_type thumb_type = MEDIA_THUMB_LARGE;
	char tmp_origin_path[MAX_PATH_SIZE] = {0,};
	char tmp_thumb_path[MAX_PATH_SIZE] = {0,};

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_ALL_MEDIA, thumb_type, tmp_origin_path, tmp_thumb_path, sizeof(tmp_thumb_path), &thumb_info);
	if (err < 0) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int thumbnail_request_from_db_async(const char *origin_path, ThumbFunc func, void *user_data)
{
	int err = -1;

	if (origin_path == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumbUserData *userData = (thumbUserData*)malloc(sizeof(thumbUserData));
	userData->func = (ThumbFunc)func;
	userData->user_data = user_data;

	int store_type = -1;
	store_type = _media_thumb_get_store_type_by_path(origin_path);

	if ((store_type != THUMB_PHONE) && (store_type != THUMB_MMC)) {
		thumb_err("origin path(%s) is invalid", origin_path);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_err("Path : %s", origin_path);

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request_async(THUMB_REQUEST_DB_INSERT, MEDIA_THUMB_LARGE, origin_path, userData);
	if (err < 0) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

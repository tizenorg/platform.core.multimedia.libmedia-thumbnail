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

#include <glib.h>

int thumbnail_request_from_db_with_size(const char *origin_path, char *thumb_path, int max_length, int *origin_width, int *origin_height, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	//int need_update_db = 0;
	media_thumb_info thumb_info;

	if (origin_path == NULL || thumb_path == NULL) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (origin_width == NULL || origin_height == NULL) {
		thumb_err("Invalid parameter ( width or height )");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (max_length <= 0) {
		thumb_err("Length is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	int store_type = -1;
	store_type = _media_thumb_get_store_type_by_path(origin_path);

	if ((store_type != THUMB_PHONE) && (store_type != THUMB_MMC)) {
		thumb_err("origin path(%s) is invalid", origin_path);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_dbg_slog("Path : %s", origin_path);

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_DB_INSERT, origin_path, thumb_path, max_length, &thumb_info, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	*origin_width = thumb_info.origin_width;
	*origin_height = thumb_info.origin_height;

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_extract_all_thumbs(uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	media_thumb_info thumb_info;
	char tmp_origin_path[MAX_PATH_SIZE] = {0,};
	char tmp_thumb_path[MAX_PATH_SIZE] = {0,};

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request(THUMB_REQUEST_ALL_MEDIA, tmp_origin_path, tmp_thumb_path, sizeof(tmp_thumb_path), &thumb_info, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_from_db_async(const char *origin_path, ThumbFunc func, void *user_data, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	if (origin_path == NULL) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	int store_type = -1;
	store_type = _media_thumb_get_store_type_by_path(origin_path);

	if ((store_type != THUMB_PHONE) && (store_type != THUMB_MMC)) {
		thumb_err("origin path(%s) is invalid", origin_path);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_dbg_slog("Path : %s", origin_path);

	thumbUserData *userData = (thumbUserData*)malloc(sizeof(thumbUserData));
	userData->func = (ThumbFunc)func;
	userData->user_data = user_data;

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_thumb_request_async(THUMB_REQUEST_DB_INSERT, origin_path, userData, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_request failed : %d", err);
		SAFE_FREE(userData);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_extract_raw_data_async(int request_id, const char *origin_path, int width, int height, ThumbRawFunc func, void *user_data, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	if (origin_path == NULL || request_id == 0) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path(%s) doesn't exist.", origin_path);
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}
	thumb_dbg_slog("Path : %s", origin_path);

	thumbRawUserData *userData = (thumbRawUserData*)malloc(sizeof(thumbRawUserData));
	userData->func = func;
	userData->user_data = user_data;

	err = _media_thumb_request_raw_data_async(THUMB_REQUEST_RAW_DATA, request_id, origin_path, width, height, userData, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_raw_thumb_request failed : %d", err);
		SAFE_FREE(userData);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_cancel_media(const char *origin_path)
{
	int err = MS_MEDIA_ERR_NONE;

	if (origin_path == NULL) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	err = _media_thumb_request_async(THUMB_REQUEST_CANCEL_MEDIA,  origin_path, NULL, 0);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_cancel_raw_data(int request_id)
{
	int err = MS_MEDIA_ERR_NONE;

	if (request_id == 0) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	err = _media_thumb_request_raw_data_async(THUMB_REQUEST_CANCEL_RAW_DATA, request_id, NULL, 0, 0, NULL, 0);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int thumbnail_request_cancel_all(bool is_raw_data)
{
	int err = MS_MEDIA_ERR_NONE;

	if(is_raw_data) {
		err = _media_thumb_request_cancel_all(true);
	} else {
		err = _media_thumb_request_cancel_all(false);
	}

	return err;
}

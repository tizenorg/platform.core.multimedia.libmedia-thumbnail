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

#include "media-thumb-db.h"
#include "media-thumb-util.h"
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <util-func.h>

static __thread  sqlite3 *db_handle;

static int _media_thumb_busy_handler(void *pData, int count)
{
	usleep(50000);
	thumb_dbg("_media_thumb_busy_handler called : %d\n", count);

	return 100 - count;
}

sqlite3 *_media_thumb_db_get_handle()
{
	return db_handle;
}

int
_media_thumb_sqlite_connect(sqlite3 **handle)
{
	int err = -1;
	err = db_util_open(MEDIA_DATABASE_NAME, handle,
			 DB_UTIL_REGISTER_HOOK_METHOD);

	if (SQLITE_OK != err) {
		*handle = NULL;
		return -1;
	}

	/*Register busy handler*/
	err = sqlite3_busy_handler(*handle, _media_thumb_busy_handler, NULL);
	if (SQLITE_OK != err) {
		if (*handle) thumb_err("[sqlite] %s\n", sqlite3_errmsg(*handle));

		db_util_close(*handle);
		*handle = NULL;

		return -1;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_sqlite_disconnect(sqlite3 *handle)
{
	int err = -1;
	if (handle != NULL) {
		err = db_util_close(handle);

		if (SQLITE_OK != err) {
			handle = NULL;
			return -1;
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_get_type_from_db(sqlite3 *handle,
									const char *origin_path,
									int *type)
{
	thumb_dbg("Origin path : %s", origin_path);

	if (handle == NULL) {
		thumb_err("DB handle is NULL");
		return -1;
	}

	int err = -1;
	char *path_string = NULL;
	char *query_string = NULL;
	sqlite3_stmt *stmt = NULL;

	path_string = sqlite3_mprintf("%s", origin_path);
	query_string = sqlite3_mprintf(SELECT_TYPE_BY_PATH, path_string);

	thumb_dbg("Query: %s", query_string);

	err = sqlite3_prepare_v2(handle, query_string, strlen(query_string), &stmt, NULL);

	sqlite3_free(query_string);
	sqlite3_free(path_string);

	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]\n", sqlite3_errmsg(handle));
		return -1;
	}

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW) {
		thumb_err("end of row [%s]\n", sqlite3_errmsg(handle));
		sqlite3_finalize(stmt);
		return -1;
	}

	*type = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	return MEDIA_THUMB_ERROR_NONE;
}


int _media_thumb_get_wh_from_db(sqlite3 *handle,
									const char *origin_path,
									int *width,
									int *height)
{
	thumb_dbg("Origin path : %s", origin_path);

	if (handle == NULL) {
		thumb_err("DB handle is NULL");
		return -1;
	}

	int err = -1;
	char *path_string = NULL;
	char *query_string = NULL;
	sqlite3_stmt *stmt = NULL;

	path_string = sqlite3_mprintf("%s", origin_path);
	query_string = sqlite3_mprintf(SELECT_WH_BY_PATH, path_string);

	thumb_dbg("Query: %s", query_string);

	err = sqlite3_prepare_v2(handle, query_string, strlen(query_string), &stmt, NULL);

	sqlite3_free(query_string);
	sqlite3_free(path_string);

	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]\n", sqlite3_errmsg(handle));
		return -1;
	}

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW) {
		thumb_err("end of row [%s]\n", sqlite3_errmsg(handle));
		sqlite3_finalize(stmt);
		return -1;
	}

	*width = sqlite3_column_int(stmt, 0);
	*height = sqlite3_column_int(stmt, 1);
	sqlite3_finalize(stmt);

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_get_thumb_path_from_db(sqlite3 *handle,
									const char *origin_path,
									char *thumb_path,
									int max_length)
{
	thumb_dbg("Origin path : %s", origin_path);

	if (handle == NULL) {
		thumb_err("DB handle is NULL");
		return -1;
	}

	int err = -1;
	char *path_string = NULL;
	char *query_string = NULL;
	sqlite3_stmt *stmt = NULL;

	path_string = sqlite3_mprintf("%s", origin_path);
	query_string = sqlite3_mprintf(SELECT_MEDIA_BY_PATH, path_string);

	thumb_dbg("Query: %s", query_string);

	err = sqlite3_prepare_v2(handle, query_string, strlen(query_string), &stmt, NULL);

	sqlite3_free(query_string);
	sqlite3_free(path_string);

	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]\n", sqlite3_errmsg(handle));
		return -1;
	}

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW) {
		thumb_err("end of row [%s]\n", sqlite3_errmsg(handle));
		sqlite3_finalize(stmt);
		return -1;
	}

	if (sqlite3_column_text(stmt, 0))
		strncpy(thumb_path, (const char *)sqlite3_column_text(stmt, 0), max_length);
	else
		thumb_path[0] = '\0';

	sqlite3_finalize(stmt);

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_update_thumb_path_to_db(sqlite3 *handle,
									const char *origin_path,
									char *thumb_path)
{
	thumb_dbg("");
	int err = -1;
	char *path_string = NULL;
	char *thumbpath_string = NULL;
	char *query_string = NULL;
	char *err_msg = NULL;

	if (handle == NULL) {
		thumb_err("DB handle is NULL");
		return -1;
	}

	path_string = sqlite3_mprintf("%s", origin_path);
	thumbpath_string = sqlite3_mprintf("%s", thumb_path);
	query_string = sqlite3_mprintf(UPDATE_THUMB_BY_PATH, thumbpath_string, path_string);

	err = sqlite3_exec(handle, query_string, NULL, NULL, &err_msg);

	thumb_dbg("Query : %s", query_string);

	sqlite3_free(query_string);
	sqlite3_free(thumbpath_string);
	sqlite3_free(path_string);

	if (SQLITE_OK != err) {
		if (err_msg) {
			thumb_err("Failed to query[ %s ]", err_msg);
			sqlite3_free(err_msg);
			err_msg = NULL;
		}

		return -1;
	}

	if (err_msg)
		sqlite3_free(err_msg);

	thumb_dbg("Query success");

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_update_wh_to_db(sqlite3 *handle,
								const char *origin_path,
								int width,
								int height)
{
	thumb_dbg("");
	int err = -1;
	char *path_string = NULL;
	char *query_string = NULL;
	char *err_msg = NULL;

	if (handle == NULL) {
		thumb_err("DB handle is NULL");
		return -1;
	}

	path_string = sqlite3_mprintf("%s", origin_path);
	query_string = sqlite3_mprintf(UPDATE_WH_BY_PATH, width, height, path_string);

	err = sqlite3_exec(handle, query_string, NULL, NULL, &err_msg);

	thumb_dbg("Query : %s", query_string);

	sqlite3_free(query_string);
	sqlite3_free(path_string);

	if (SQLITE_OK != err) {
		if (err_msg) {
			thumb_err("Failed to query[ %s ]", err_msg);
			sqlite3_free(err_msg);
			err_msg = NULL;
		}

		return -1;
	}

	if (err_msg)
		sqlite3_free(err_msg);

	thumb_dbg("Query success");

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_db_connect()
{
	int err = -1;
/*
	err = media_svc_connect(&mb_svc_handle);
	if (err < 0) {
		thumb_err("media_svc_connect failed: %d", err);
		mb_svc_handle = NULL;
		return err;
	}
*/
	err = _media_thumb_sqlite_connect(&db_handle);
	if (err < 0) {
		thumb_err("_media_thumb_sqlite_connect failed: %d", err);
		db_handle = NULL;
		return err;
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_db_disconnect()
{
	int err = -1;
/*
	err = media_svc_disconnect(mb_svc_handle);

	if (err < 0) {
		thumb_err("media_svc_disconnect failed: %d", err);
	}

	mb_svc_handle = NULL;
*/
	err = _media_thumb_sqlite_disconnect(db_handle);
	if (err < 0) {
		thumb_err("_media_thumb_sqlite_disconnect failed: %d", err);
		db_handle = NULL;
		return err;
	}

	db_handle = NULL;
	return err;
}

int
_media_thumb_get_thumb_from_db(const char *origin_path,
								char *thumb_path,
								int max_length,
								int *need_update_db)
{
	thumb_dbg("");
	int err = -1;

	//err = minfo_get_thumb_path(mb_svc_handle, origin_path, thumb_path, max_length);
	err = _media_thumb_get_thumb_path_from_db(db_handle, origin_path, thumb_path, max_length);
	if (err < 0) {
		thumb_warn("Original path doesn't exist in DB");
		return -1;
	}

	if (strlen(thumb_path) == 0) {
		thumb_warn("thumb path doesn't exist in DB");
		*need_update_db = 1;
		return -1;
	}

	thumb_dbg("Thumb path in DB is %s", thumb_path);

	if (!g_file_test(thumb_path, 
					G_FILE_TEST_EXISTS)) {
		thumb_warn("thumb path doesn't exist in file system");
		*need_update_db = 1;
		return -1;
	} else {
		thumb_dbg("This thumb path already exist");
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_get_thumb_from_db_with_size(const char *origin_path,
								char *thumb_path,
								int max_length,
								int *need_update_db,
								int *width,
								int *height)
{
	thumb_dbg("");
	int err = -1;

	//err = minfo_get_thumb_path(mb_svc_handle, origin_path, thumb_path, max_length);
	err = _media_thumb_get_thumb_path_from_db(db_handle, origin_path, thumb_path, max_length);
	if (err < 0) {
		thumb_warn("Original path doesn't exist in DB");
		return -1;
	}

	if (strlen(thumb_path) == 0) {
		thumb_warn("thumb path doesn't exist in DB");
		*need_update_db = 1;
		return -1;
	}

	thumb_dbg("Thumb path in DB is %s", thumb_path);

	if (!g_file_test(thumb_path, 
					G_FILE_TEST_EXISTS)) {
		thumb_warn("thumb path doesn't exist in file system");
		*need_update_db = 1;
		return -1;
	} else {
		thumb_dbg("This thumb path already exist");
		int orig_w = 0;
		int orig_h = 0;

		err = _media_thumb_get_wh_from_db(db_handle, origin_path, &orig_w, &orig_h);
		if (err < 0) {
			thumb_err("_media_thumb_get_wh_from_db failed : %d", err);
		} else {
			thumb_err("_media_thumb_get_wh_from_db Success ( w:%d, h:%d )", orig_w, orig_h);
			*width = orig_w;
			*height = orig_h;
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_update_db(const char *origin_path,
									char *thumb_path,
									int width,
									int height)
{
	thumb_dbg("");
	int err = -1;

#if 0
	Mitem *item = NULL;

	err = minfo_get_item(mb_svc_handle, origin_path, &item);
	if (err < 0) {
		thumb_err("minfo_get_item (%s) failed: %d", origin_path, err);
		return MEDIA_THUMB_ERROR_DB;
	}

	err = minfo_update_media_thumb(mb_svc_handle, item->uuid, thumb_path);
	if (err < 0) {
		thumb_err("minfo_update_media_thumb (ID:%s, %s) failed: %d",
							item->uuid, thumb_path, err);
		minfo_destroy_mtype_item(item);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (item->type == MINFO_ITEM_IMAGE) {
		err = minfo_update_image_meta_info_int(mb_svc_handle, item->uuid, 
												MINFO_IMAGE_META_WIDTH, width,
												MINFO_IMAGE_META_HEIGHT, height, -1);
	
		if (err < 0) {
			thumb_err("minfo_update_image_meta_info_int failed: %d", err);
			minfo_destroy_mtype_item(item);
			return MEDIA_THUMB_ERROR_DB;
		}
	}
	
	err = minfo_destroy_mtype_item(item);
#endif

	int media_type = THUMB_NONE_TYPE;
	err = _media_thumb_get_type_from_db(db_handle, origin_path, &media_type);
	if (err < 0) {
		thumb_err("_media_thumb_get_type_from_db (%s) failed: %d", origin_path, err);
		return MEDIA_THUMB_ERROR_DB;
	}

	err = _media_thumb_update_thumb_path_to_db(db_handle, origin_path, thumb_path);
	if (err < 0) {
		thumb_err("_media_thumb_update_thumb_path_to_db (%s) failed: %d", origin_path, err);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (media_type == THUMB_IMAGE_TYPE && width > 0 && height > 0) {
		err = _media_thumb_update_wh_to_db(db_handle, origin_path, width, height);
		if (err < 0) {
			thumb_err("_media_thumb_update_wh_to_db (%s) failed: %d", origin_path, err);
			return MEDIA_THUMB_ERROR_DB;
		}
	}

	thumb_dbg("_media_thumb_update_db success");

	return MEDIA_THUMB_ERROR_NONE;
}


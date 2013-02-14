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

#include <sqlite3.h>
#include <media-util.h>
#include "media-thumb-error.h"
#include "media-thumb-types.h"
#include "media-thumb-debug.h"

#ifndef _MEDIA_THUMB_DB_H_
#define _MEDIA_THUMB_DB_H_

#define MEDIA_DATABASE_NAME MEDIA_DB_NAME /* defined in media-util.h */

#ifndef _USE_NEW_MEDIA_DB_
#define SELECT_PATH_FROM_UNEXTRACTED_THUMB_MEDIA "SELECT path from visual_media where thumbnail_path='' and valid=1;"
#define SELECT_MEDIA_BY_PATH "SELECT thumbnail_path FROM visual_media WHERE path='%q';"
#define SELECT_TYPE_BY_PATH "SELECT content_type FROM visual_media WHERE path='%q';"
#define UPDATE_THUMB_BY_PATH "UPDATE visual_media SET thumbnail_path = '%q' WHERE path='%q';"
#define UPDATE_WH_BY_PATH "UPDATE image_meta SET width=%d,height=%d WHERE visual_uuid=(SELECT visual_uuid FROM visual_media WHERE path='%q');"

#else
#define SELECT_PATH_FROM_UNEXTRACTED_THUMB_MEDIA "SELECT path from media where thumbnail_path is null and validity=1 and (media_type=0 or media_type=1);"
#define SELECT_MEDIA_BY_PATH "SELECT thumbnail_path FROM media WHERE path='%q';"
#define SELECT_TYPE_BY_PATH "SELECT media_type FROM media WHERE path='%q';"
#define SELECT_WH_BY_PATH "SELECT width, height FROM media WHERE path='%q';"
#define UPDATE_THUMB_BY_PATH "UPDATE media SET thumbnail_path = '%q' WHERE path='%q';"
#define UPDATE_WH_BY_PATH "UPDATE media SET width=%d,height=%d WHERE path='%q';"
#define UPDATE_THUMB_WH_BY_PATH "UPDATE media SET thumbnail_path = '%q', width=%d,height=%d WHERE path='%q';"
#endif

sqlite3 *_media_thumb_db_get_handle();

int
_media_thumb_db_connect();

int
_media_thumb_db_disconnect();

int
_media_thumb_get_thumb_from_db(const char *origin_path,
								char *thumb_path,
								int max_length,
								int *need_update_db);

int
_media_thumb_get_thumb_from_db_with_size(const char *origin_path,
								char *thumb_path,
								int max_length,
								int *need_update_db,
								int *width,
								int *height);

int
_media_thumb_update_db(const char *origin_path,
									char *thumb_path,
									int width,
									int height);

#endif /*_MEDIA_THUMB_DB_H_*/


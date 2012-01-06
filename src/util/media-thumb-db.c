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
#include <media-svc.h>
#include <glib.h>
#include <string.h>

int
_media_thumb_get_thumb_from_db(const char *origin_path,
								char *thumb_path,
								int max_length,
								int *need_update_db)
{
	thumb_dbg("");
	int err = -1;

	err = minfo_get_thumb_path(origin_path, thumb_path, max_length);
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

	return 0;
}

int
_media_thumb_update_db(const char *origin_path,
									char *thumb_path,
									int width,
									int height)
{
	thumb_dbg("");
	int err = -1;
	Mitem *item = NULL;

	err = minfo_get_item(origin_path, &item);
	if (err < 0) {
		thumb_err("minfo_get_item (%s) failed: %d", origin_path, err);
		return MEDIA_THUMB_ERROR_DB;
	}

	err = minfo_update_media_thumb(item->uuid, thumb_path);
	if (err < 0) {
		thumb_err("minfo_update_media_thumb (ID:%s, %s) failed: %d",
							item->uuid, thumb_path, err);
		minfo_destroy_mtype_item(item);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (item->type == MINFO_ITEM_IMAGE) {
		err = minfo_update_image_meta_info_int(item->uuid, MINFO_IMAGE_META_WIDTH, width,
													MINFO_IMAGE_META_HEIGHT, height, -1);
	
		if (err < 0) {
			thumb_err("minfo_update_image_meta_info_int failed: %d", err);
			minfo_destroy_mtype_item(item);
			return MEDIA_THUMB_ERROR_DB;
		}
	}
	
	err = minfo_destroy_mtype_item(item);

	thumb_dbg("_media_thumb_update_db success");

	return 0;
}


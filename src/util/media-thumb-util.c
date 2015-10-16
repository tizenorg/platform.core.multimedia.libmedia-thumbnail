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

#include "media-thumb-util.h"
#include "media-thumb-internal.h"
#include "media-thumb-debug.h"

#include <glib.h>
#include <aul.h>
#include <string.h>

int
_media_thumb_get_file_type(const char *file_full_path)
{
	int ret = 0;
	char mimetype[255] = {0,};
	const char *unsupported_type = "image/tiff";

	if (file_full_path == NULL)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	/* get content type and mime type from file. */
	ret = aul_get_mime_from_file(file_full_path, mimetype, sizeof(mimetype));
	if (ret < 0) {
		thumb_warn
			("aul_get_mime_from_file fail.. Now trying to get type by extension");

		char ext[255] = { 0 };
		ret = _media_thumb_get_file_ext(file_full_path, ext, sizeof(ext));
		if (ret < 0) {
			thumb_err("_media_thumb_get_file_ext failed");
			return THUMB_NONE_TYPE;
		}

		if (strcasecmp(ext, "JPG") == 0 ||
			strcasecmp(ext, "JPEG") == 0 ||
			strcasecmp(ext, "PNG") == 0 ||
			strcasecmp(ext, "GIF") == 0 ||
			strcasecmp(ext, "AGIF") == 0 ||
			strcasecmp(ext, "XWD") == 0 ||
			strcasecmp(ext, "BMP") == 0 ||
			strcasecmp(ext, "WBMP") == 0) {
			return THUMB_IMAGE_TYPE;
		} else if (strcasecmp(ext, "AVI") == 0 ||
			strcasecmp(ext, "MPEG") == 0 ||
			strcasecmp(ext, "MP4") == 0 ||
			strcasecmp(ext, "DCF") == 0 ||
			strcasecmp(ext, "WMV") == 0 ||
			strcasecmp(ext, "3GPP") == 0 ||
			strcasecmp(ext, "3GP") == 0) {
			return THUMB_VIDEO_TYPE;
		} else {
			return THUMB_NONE_TYPE;
		}
	}

	thumb_dbg("mime type : %s", mimetype);

	/* categorize from mimetype */
	if (strstr(mimetype, "image") != NULL) {
		if (!strcmp(mimetype, unsupported_type)) {
			thumb_warn("This is unsupport file type");
			return THUMB_NONE_TYPE;
		}
		return THUMB_IMAGE_TYPE;
	} else if (strstr(mimetype, "video") != NULL) {
		return THUMB_VIDEO_TYPE;
	}

	return THUMB_NONE_TYPE;
}

int _media_thumb_get_store_type_by_path(const char *full_path)
{
	if (full_path != NULL && THUMB_PATH_PHONE != NULL && THUMB_PATH_MMC != NULL) {
		if (strncmp(full_path, THUMB_PATH_PHONE, strlen(THUMB_PATH_PHONE)) == 0) {
			return THUMB_PHONE;
		} else if (strncmp(full_path, THUMB_PATH_MMC, strlen(THUMB_PATH_MMC)) == 0) {
			return THUMB_MMC;
		}
	}

	return -1;
}

int _media_thumb_remove_file(const char *path)
{
	int result = -1;

	result = remove(path);
	if (result == 0) {
		thumb_dbg("success to remove file");
		return TRUE;
	} else {
		thumb_stderror("fail to remove file[%s] result");
		return FALSE;
	}
}

int _media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len)
{
	int i = 0;

	for (i = (int)strlen(file_path); i >= 0; i--) {
		if ((file_path[i] == '.') && (i < (int)strlen(file_path))) {
			strncpy(file_ext, &file_path[i + 1], max_len);
			return 0;
		}

		/* meet the dir. no ext */
		if (file_path[i] == '/') {
			return -1;
		}
	}

	return -1;
}
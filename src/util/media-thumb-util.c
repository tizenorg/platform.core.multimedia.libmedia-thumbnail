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

#include <glib.h>
#include <aul.h>
#include <string.h>
#include <drm_client.h>

#include <Evas.h>
#include <Ecore_Evas.h>

int _media_thumb_get_width(media_thumb_type thumb_type)
{
	if (thumb_type == MEDIA_THUMB_LARGE) {
		return THUMB_LARGE_WIDTH;
	} else if (thumb_type == MEDIA_THUMB_SMALL) {
		return  THUMB_SMALL_WIDTH;
	} else {
		return -1;
	}
}

int _media_thumb_get_height(media_thumb_type thumb_type)
{
	if (thumb_type == MEDIA_THUMB_LARGE) {
		return THUMB_LARGE_HEIGHT;
	} else if (thumb_type == MEDIA_THUMB_SMALL) {
		return  THUMB_SMALL_HEIGHT;
	} else {
		return -1;
	}
}

int _media_thumb_get_file_ext(const char *file_path, char *file_ext, int max_len)
{
	int i = 0;

	for (i = strlen(file_path); i >= 0; i--) {
		if ((file_path[i] == '.') && (i < strlen(file_path))) {
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

int
_media_thumb_get_file_type(const char *file_full_path)
{
	int ret = 0;
	drm_bool_type_e drm_type;
	drm_file_type_e drm_file_type;
	char mimetype[255];

	if (file_full_path == NULL)
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;

	ret = drm_is_drm_file(file_full_path, &drm_type);
	if (ret < 0) {
		thumb_err("drm_is_drm_file falied : %d", ret);
		drm_type = DRM_FALSE;
	}

	if (drm_type == DRM_TRUE) {
		thumb_dbg("DRM file : %s", file_full_path);

		ret = drm_get_file_type(file_full_path, &drm_file_type);
		if (ret < 0) {
			thumb_err("drm_get_file_type falied : %d", ret);
			return THUMB_NONE_TYPE;
		}

		if (drm_file_type == DRM_TYPE_UNDEFINED) {
			return THUMB_NONE_TYPE;
		} else {
			drm_content_info_s contentInfo;
			memset(&contentInfo, 0x00, sizeof(drm_content_info_s));

			ret = drm_get_content_info(file_full_path, &contentInfo);
			if (ret != DRM_RETURN_SUCCESS) {
				thumb_err("drm_get_content_info() fails. : %d", ret);
				return THUMB_NONE_TYPE;
			}
			thumb_dbg("DRM mime type: %s", contentInfo.mime_type);

			strncpy(mimetype, contentInfo.mime_type, sizeof(mimetype) - 1);
			mimetype[sizeof(mimetype) - 1] = '\0';
		}
	} else {
		/* get content type and mime type from file. */
		ret =
			aul_get_mime_from_file(file_full_path, mimetype, sizeof(mimetype));
		if (ret < 0) {
			thumb_warn
				("aul_get_mime_from_file fail.. Now trying to get type by extension");
	
			char ext[255] = { 0 };
			int ret = _media_thumb_get_file_ext(file_full_path, ext, sizeof(ext));
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
	}

	thumb_dbg("mime type : %s", mimetype);

	/* categorize from mimetype */
	if (strstr(mimetype, "image") != NULL) {
		return THUMB_IMAGE_TYPE;
	} else if (strstr(mimetype, "video") != NULL) {
		return THUMB_VIDEO_TYPE;
	}

	return THUMB_NONE_TYPE;
}

int _media_thumb_get_store_type_by_path(const char *full_path)
{
	if (full_path != NULL) {
		if (strncmp
		    (full_path, THUMB_PATH_PHONE,
		     strlen(THUMB_PATH_PHONE)) == 0) {
			return THUMB_PHONE;
		} else
		    if (strncmp
			(full_path, THUMB_PATH_MMC,
			 strlen(THUMB_PATH_MMC)) == 0) {
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
		thumb_err("fail to remove file[%s] result errno = %s", path, strerror(errno));
		return FALSE;
	}
}

int
_media_thumb_get_hash_name(const char *file_full_path,
				 char *thumb_hash_path, size_t max_thumb_path)
{
	char *hash_name;
	char *thumb_dir = NULL;
	char file_ext[255] = { 0 };
	media_thumb_store_type store_type = -1;

	if (file_full_path == NULL || thumb_hash_path == NULL
	    || max_thumb_path <= 0) {
		thumb_err
		    ("file_full_path==NULL || thumb_hash_path == NULL || max_thumb_path <= 0");
		return -1;
	}

	_media_thumb_get_file_ext(file_full_path, file_ext, sizeof(file_ext));

	store_type = _media_thumb_get_store_type_by_path(file_full_path);
	if (store_type == THUMB_PHONE) {
		thumb_dir = THUMB_PHONE_PATH;
	} else if (store_type == THUMB_MMC) {
		thumb_dir = THUMB_MMC_PATH;
	} else {
		thumb_dir = THUMB_PHONE_PATH;
	}

	hash_name = _media_thumb_generate_hash_name(file_full_path);

	int ret_len;
	ret_len =
	    snprintf(thumb_hash_path, max_thumb_path - 1, "%s/.%s-%s.jpg",
		     thumb_dir, file_ext, hash_name);
	if (ret_len < 0) {
		thumb_err("Error when snprintf");
		return -1;
	} else if (ret_len > max_thumb_path) {
		thumb_err("Error for the length of thumb pathname");
		return -1;
	}

	//thumb_dbg("thumb hash : %s", thumb_hash_path);

	return 0;
}


int _media_thumb_save_to_file_with_evas(unsigned char *data, 
											int w,
											int h,
											int alpha,
											char *thumb_path)
{	
	Ecore_Evas *ee =
		ecore_evas_buffer_new(w, h);
	if (ee == NULL) {
		thumb_err("Failed to create a new ecore evas buffer\n");
		return -1;
	}

	Evas *evas = ecore_evas_get(ee);
	if (evas == NULL) {
		thumb_err("Failed to ecore_evas_get\n");
		ecore_evas_free(ee);
		return -1;
	}

	Evas_Object *img = NULL;
	img = evas_object_image_add(evas);

	if (img == NULL) {
		thumb_err("image object is NULL\n");
		ecore_evas_free(ee);
		return -1;
	}

	evas_object_image_colorspace_set(img, EVAS_COLORSPACE_ARGB8888);
	evas_object_image_size_set(img, w, h);
	evas_object_image_fill_set(img, 0, 0, w, h);

	if (alpha) evas_object_image_alpha_set(img, 1);

	evas_object_image_data_set(img, data);
	evas_object_image_data_update_add(img, 0, 0, w, h);

	if (evas_object_image_save
		(img, thumb_path, NULL,	"quality=100 compress=1")) {
		thumb_dbg("evas_object_image_save success\n");
		ecore_evas_free(ee);

		return 0;
	} else {
		thumb_dbg("evas_object_image_save failed\n");
		ecore_evas_free(ee);
		return -1;
	}
}


int _thumbnail_get_data(const char *origin_path, 
						media_thumb_type thumb_type, 
						media_thumb_format format, 
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						int *origin_width,
						int *origin_height,
						int *alpha)
{
	int err = -1;
	int thumb_width = -1;
	int thumb_height = -1;

	if (origin_path == NULL || size == NULL 
			|| width == NULL || height == NULL) {
		thumb_err("Invalid parameter");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (format < MEDIA_THUMB_BGRA || format > MEDIA_THUMB_RGB888) {
		thumb_err("parameter format is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path (%s) does not exist", origin_path);
			return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_width = _media_thumb_get_width(thumb_type);
	if (thumb_width < 0) {
		thumb_err("media_thumb_type is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_height = _media_thumb_get_height(thumb_type);
	if (thumb_height < 0) {
		thumb_err("media_thumb_type is invalid");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	thumb_dbg("Origin path : %s", origin_path);

	int file_type = THUMB_NONE_TYPE;
	media_thumb_info thumb_info = {0,};
	file_type = _media_thumb_get_file_type(origin_path);

	if (file_type == THUMB_IMAGE_TYPE) {
		err = _media_thumb_image(origin_path, thumb_width, thumb_height, format, &thumb_info);
		if (err < 0) {
			thumb_err("_media_thumb_image failed");
			return err;
		}

	} else if (file_type == THUMB_VIDEO_TYPE) {
		err = _media_thumb_video(origin_path, thumb_width, thumb_height, format, &thumb_info);
		if (err < 0) {
			thumb_err("_media_thumb_image failed");
			return err;
		}
	}

	if (size) *size = thumb_info.size;
	if (width) *width = thumb_info.width;
	if (height) *height = thumb_info.height;
	*data = thumb_info.data;
	if (origin_width) *origin_width = thumb_info.origin_width;
	if (origin_height) *origin_height = thumb_info.origin_height;
	if (alpha) *alpha = thumb_info.alpha;

	thumb_dbg("Thumb data is generated successfully (Size:%d, W:%d, H:%d) 0x%x",
				*size, *width, *height, *data);

	return MEDIA_THUMB_ERROR_NONE;
}

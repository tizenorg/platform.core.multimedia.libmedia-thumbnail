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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <Evas.h>
#include <Ecore_Evas.h>
#include <mm_util_imgp.h>
#include <mm_util_jpeg.h>

#include "media-thumbnail.h"
#include "media-thumbnail-private.h"
#include "media-thumb-debug.h"
#include "media-thumb-ipc.h"
#include "media-thumb-util.h"

int save_to_file_with_evas(unsigned char *data, int w, int h, int is_bgra)
{
	ecore_evas_init();
	
	Ecore_Evas *ee =
		ecore_evas_buffer_new(w, h);
	Evas *evas = ecore_evas_get(ee);

	Evas_Object *img = NULL;
	img = evas_object_image_add(evas);

	if (img == NULL) {
		printf("image object is NULL\n");
		ecore_evas_free(ee);
		ecore_evas_shutdown();
		return -1;
	}

	evas_object_image_colorspace_set(img, EVAS_COLORSPACE_ARGB8888);
	evas_object_image_size_set(img, w, h);
	evas_object_image_fill_set(img, 0, 0, w, h);

	if (!is_bgra) {
	unsigned char *m = NULL;
	m = evas_object_image_data_get(img, 1);
#if 1				/* Use self-logic to convert from RGB888 to RGBA */
	int i = 0, j;
	for (j = 0; j < w * 3 * h;
		j += 3) {
		m[i++] = (data[j + 2]);
		m[i++] = (data[j + 1]);
		m[i++] = (data[j]);
		m[i++] = 0x0;
	}

#else				/* Use mmf api to convert from RGB888 to RGBA */
	int mm_ret = 0;
	if ((mm_ret =
		mm_util_convert_colorspace(data,
					w,
					h,
					MM_UTIL_IMG_FMT_RGB888,
					m,
					MM_UTIL_IMG_FMT_BGRA8888))
		< 0) {
		printf
			("Failed to change from rgb888 to argb8888 %d\n",
			mm_ret);
		return -1;
	}
#endif				/* End of use mmf api to convert from RGB888 to RGBA */

	evas_object_image_data_set(img, m);
	evas_object_image_data_update_add(img, 0, 0, w, h);
	} else {
	evas_object_image_data_set(img, data);
	evas_object_image_data_update_add(img, 0, 0, w, h);
	}

	if (evas_object_image_save
		(img, "/mnt/nfs/test.jpg", NULL,
		"quality=50 compress=2")) {
		printf("evas_object_image_save success\n");
	} else {
		printf("evas_object_image_save failed\n");
	}

	ecore_evas_shutdown();

	return 0;
}

int main(int argc, char *argv[])
{
	int mode;
	int err = -1;
	const char *origin_path = NULL;

	if ((argc != 3) && (argc != 4)) {
		printf("Usage: %s [test mode number] [path]\n", argv[0]);
		return -1;
	}

	mode = atoi(argv[1]);
	origin_path = argv[2];

	if (origin_path && (mode == 1)) {
		printf("Test _thumbnail_get_data\n");

		unsigned char *data = NULL;
		int thumb_size = 0;
		int thumb_w = 0;
		int thumb_h = 0;
		int origin_w = 0;
		int origin_h = 0;
		int alpha = FALSE;

		media_thumb_type thumb_type = MEDIA_THUMB_LARGE;
		//media_thumb_type thumb_type = MEDIA_THUMB_SMALL;
		media_thumb_format thumb_format = MEDIA_THUMB_BGRA;
		//media_thumb_format thumb_format = MEDIA_THUMB_RGB888;
		int is_bgra = 1;
		//int is_bgra = 0;

		long start = thumb_get_debug_time();

		err = _thumbnail_get_data(origin_path, thumb_type, thumb_format, &data, &thumb_size, &thumb_w, &thumb_h, &origin_w, &origin_h, &alpha);
		if (err < 0) {
			printf("_thumbnail_get_data failed - %d\n", err);
			return -1;
		}
	
		printf("Size : %d, W:%d, H:%d\n", thumb_size, thumb_w, thumb_h);	
		printf("Origin W:%d, Origin H:%d\n", origin_w, origin_h);

		err = save_to_file_with_evas(data, thumb_w, thumb_h, is_bgra);
		if (err < 0) {
			printf("_thumbnail_get_data failed - %d\n", err);
			return -1;
		} else {
			printf("file save success\n");
		}

		SAFE_FREE(data);

		long end = thumb_get_debug_time();
		printf("Time : %f\n", ((double)(end - start) / (double)CLOCKS_PER_SEC));

	} else if (mode == 2) {
		printf("Test thumbnail_request_from_db\n");
		//const char *origin_path = "/opt/media/test/movie1.mp4";
		//const char *origin_path = "/opt/media/test/high.jpg";
		//const char *origin_path = "/opt/media/test/movie2.avi";
		char thumb_path[MAX_PATH_SIZE + 1];

		err = thumbnail_request_from_db(origin_path, thumb_path, sizeof(thumb_path));
		if (err < 0) {
			printf("thumbnail_request_from_db failed : %d\n", err);
			return -1;
		}

		printf("Success!! (%s)\n", thumb_path);
	} else if (mode == 3) {
		printf("Test thumbnail_request_save_to_file\n");
		const char *thumb_path = NULL;

		if (argv[3]) {
		thumb_path = argv[3];
		} else {
			printf("3 mode requires target path of thumbnail\n");
			return -1;
		}

		err = thumbnail_request_save_to_file(origin_path, MEDIA_THUMB_LARGE, thumb_path);
		if (err < 0) {
			printf("thumbnail_request_save_to_file failed : %d\n", err);
			return -1;
		}

		printf("Success!!\n");
	} else if (origin_path && mode == 4) {
		printf("Test thumbnail_generate_hash_code\n");
		char hash[255] = {0,};

		err = thumbnail_generate_hash_code(origin_path, hash, sizeof(hash));
		if (err < 0) {
			printf("thumbnail_generate_hash_code failed : %d\n", err);
			return -1;
		} else {
			printf("Hash : %s\n", hash);
		}

		printf("Success!!\n");
	} else if (mode == 5) {
		printf("Test thumbnail_request_extract_all_thumbs\n");

		err = thumbnail_request_extract_all_thumbs();
		if (err < 0) {
			printf("thumbnail_request_extract_all_thumbs failed : %d\n", err);
			return -1;
		} else {
			printf("thumbnail_request_extract_all_thumbs success!\n");
		}
	} else if (mode == 6) {
		printf("Test thumbnail_request_cancel_media\n");

		err = thumbnail_request_cancel_media(origin_path);
		if (err < 0) {
			printf("thumbnail_request_cancel_media failed : %d\n", err);
			return -1;
		} else {
			printf("thumbnail_request_cancel_media success!\n");
		}
	} else if (mode == 7) {
		printf("Test thumbnail_request_cancel_all\n");

		err = thumbnail_request_cancel_all();
		if (err < 0) {
			printf("thumbnail_request_cancel_all failed : %d\n", err);
			return -1;
		} else {
			printf("thumbnail_request_cancel_all success!\n");
		}
	}

	thumb_dbg("Test is Completed");

	return 0;
}


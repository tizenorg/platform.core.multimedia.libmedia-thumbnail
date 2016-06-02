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

#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif




typedef int (*ThumbFunc) (int error_code, char* path, void* data);

typedef void (*ThumbRawFunc) (int error_code, int request_id, const char* org_path, int thumb_width, int thumb_height, unsigned char* thumb_data, int thumb_size, void* data);

int thumbnail_request_from_db_async(unsigned int request_id, const char *origin_path, ThumbFunc func, void *user_data, uid_t uid);

int thumbnail_request_extract_raw_data_async(int request_id, const char *origin_path, int width, int height, ThumbRawFunc func, void *user_data, uid_t uid);

int thumbnail_request_extract_all_thumbs(uid_t uid);

int thumbnail_request_from_db_with_size(const char *origin_path, char *thumb_path, int max_length, int *origin_width, int *origin_height, uid_t uid);

int thumbnail_request_cancel_media(unsigned int request_id, const char *origin_path);

int thumbnail_request_cancel_raw_data(int request_id);

int thumbnail_request_cancel_all(bool is_raw_data);

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_THUMBNAIL_H_*/


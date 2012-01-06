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


#include "media-thumb-error.h"
#include "media-thumb-types.h"
#include "media-thumb-debug.h"

#ifndef _MEDIA_THUMB_INTERNAL_H_
#define _MEDIA_THUMB_INTERNAL_H_

#define THUMB_WIDTH_MAX
/* The maximum of resolution to be able to get thumbnail is 3000 x 3000, except for only jpeg */
#define THUMB_MAX_ALLOWED_MEM_FOR_THUMB 9000000


typedef struct {
	int size;
	int width;
	int height;
	int origin_width;
	int origin_height;
	unsigned char *data;
} media_thumb_info;

enum Exif_Orientation {
    NOT_AVAILABLE=0,
    NORMAL  =1,
    HFLIP   =2,
    ROT_180 =3,
    VFLIP   =4,
    TRANSPOSE   =5,
    ROT_90  =6,
    TRANSVERSE  =7,
    ROT_270 =8
};

int
_media_thumb_image(const char *origin_path, 
					int thumb_length, 
					media_thumb_format format, 
					media_thumb_info *thumb_info);

int
_media_thumb_video(const char *origin_path, 
					int thumb_length, 
					media_thumb_format format,
					media_thumb_info *thumb_info);

#endif /*_MEDIA_THUMB_INTERNAL_H_*/

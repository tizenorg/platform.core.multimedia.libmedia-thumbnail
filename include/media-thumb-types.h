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
#include "media-util.h"

#ifndef _MEDIA_THUMB_TYPES_H_
#define _MEDIA_THUMB_TYPES_H_

typedef enum {
	MEDIA_THUMB_LARGE,
	MEDIA_THUMB_SMALL,
} media_thumb_type;

typedef enum {
	MEDIA_THUMB_BGRA,			/* BGRA, especially provided for evas users */
	MEDIA_THUMB_RGB888,			/* RGB888 */
} media_thumb_format;

#endif /*_MEDIA_THUMB_TYPES_H_*/

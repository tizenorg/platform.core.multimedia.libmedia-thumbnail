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


/**
* @file		utc_media_thumb_common.h
* @author
* @brief	This is the implementaion file for the test case of thumbnail service

* @version	Initial Creation Version 0.1
* @date		2010-10-13
*/

#ifndef __UTS_MEDIA_THUMB_COMMON_H_
#define __UTS_MEDIA_THUMB_COMMON_H_

#include <media-thumbnail.h>
#include <tet_api.h>

#define UTC_THUMB_LOG(fmt, args...)	tet_printf("[%s(L%d)]:"fmt"\n", __FUNCTION__, __LINE__, ##args)
#define API_NAME "libmedia-thumbnail"


void startup()
{
}

void cleanup()
{
}


#endif //__UTS_MEDIA_THUMB_COMMON_H_

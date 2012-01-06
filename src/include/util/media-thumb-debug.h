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

#ifndef _MEDIA_THUMB_DEBUG_H_
#define _MEDIA_THUMB_DEBUG_H_

#include <stdio.h>
#include <stdlib.h>
#include <dlog.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif


#define _PERFORMANCE_CHECK_


#define LOG_TAG "Media-Thumb"

#define FONT_COLOR_RESET    "\033[0m"
#define FONT_COLOR_RED      "\033[31m"
#define FONT_COLOR_GREEN    "\033[32m"
#define FONT_COLOR_YELLOW   "\033[33m"
#define FONT_COLOR_BLUE     "\033[34m"
#define FONT_COLOR_PURPLE   "\033[35m"
#define FONT_COLOR_CYAN     "\033[36m"
#define FONT_COLOR_GRAY     "\033[37m"

#define thumb_dbg(fmt, arg...)	 LOGD(FONT_COLOR_RESET"[%s : %d] " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define thumb_warn(fmt, arg...)	 LOGW(FONT_COLOR_GREEN"[%s : %d] " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define thumb_err(fmt, arg...)	 LOGE(FONT_COLOR_RED"[%s : %d] " fmt "\n", __FUNCTION__, __LINE__, ##arg)

#ifdef _USE_LOG_FILE_
void thumb_init_file_debug();
void thumb_close_file_debug();
FILE* get_fp();
#define thumb_file_dbg(fmt,arg...)      fprintf( get_fp(), "[%s: %d] " fmt "\n", __FUNCTION__, __LINE__, ##arg)

#endif


#ifdef _PERFORMANCE_CHECK_
long
thumb_get_debug_time(void);
void
thumb_reset_debug_time(void);
void
thumb_print_debug_time(char* time_string);
void
thumb_print_debug_time_ex(long start, long end, const char* func_name, char* time_string);
#endif

#endif /*_MEDIA_THUMB_DEBUG_H_*/


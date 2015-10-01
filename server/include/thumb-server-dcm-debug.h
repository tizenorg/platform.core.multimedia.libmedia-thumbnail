/*
 *  DCM Service
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Yong Yeon Kim <yy9875.kim@samsung.com>
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

#include <dlog.h>
#include "media-thumb-util.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "DCM_SERVICE"

/* anci c color type */
#define FONT_COLOR_RESET    "\033[0m"
#define FONT_COLOR_RED      "\033[31m"
#define FONT_COLOR_GREEN    "\033[32m"
#define FONT_COLOR_YELLOW   "\033[33m"
#define FONT_COLOR_BLUE     "\033[34m"
#define FONT_COLOR_PURPLE   "\033[35m"
#define FONT_COLOR_CYAN     "\033[36m"
#define FONT_COLOR_GRAY     "\033[37m"

#define dcm_dbg(fmt, arg...)  LOGD(FONT_COLOR_CYAN fmt FONT_COLOR_RESET, ##arg)
#define dcm_dbgI(fmt, arg...) LOGI(FONT_COLOR_BLUE fmt FONT_COLOR_RESET, ##arg)
#define dcm_dbgW(fmt, arg...) LOGW(FONT_COLOR_GREEN fmt FONT_COLOR_RESET, ##arg)
#define dcm_dbgE(fmt, arg...) LOGE(FONT_COLOR_RED fmt FONT_COLOR_RESET, ##arg)

#define dcm_sdbg(fmt, arg...)  SECURE_LOGD(FONT_COLOR_CYAN fmt FONT_COLOR_RESET, ##arg)
#define dcm_sdbgW(fmt, arg...) SECURE_LOGW(FONT_COLOR_GREEN fmt FONT_COLOR_RESET, ##arg)
#define dcm_sdbgE(fmt, arg...) SECURE_LOGE(FONT_COLOR_RED fmt FONT_COLOR_RESET, ##arg)

#define DCM_FUNC_START dcm_dbg("FUNCTION START")
#define DCM_FUNC_END dcm_dbg("FUNCTION END")

#define dcm_retm_if(expr, fmt, arg...) do { \
	if(expr) { \
		dcm_dbgE(fmt, ##arg); \
		dcm_dbgE("(%s) -> %s() return", #expr, __FUNCTION__); \
		return; \
	} \
} while (0)

#define dcm_retvm_if(expr, val, fmt, arg...) do { \
	if(expr) { \
		dcm_dbgE(fmt, ##arg); \
		dcm_dbgE("(%s) -> %s() return", #expr, __FUNCTION__); \
		return (val); \
	} \
} while (0)

#define DCM_CHECK_VAL(expr, val) 		dcm_retvm_if(!(expr), val, "Invalid parameter, return ERROR code!")
#define DCM_CHECK_NULL(expr) 			dcm_retvm_if(!(expr), NULL, "Invalid parameter, return NULL!")
#define DCM_CHECK_FALSE(expr) 			dcm_retvm_if(!(expr), FALSE, "Invalid parameter, return FALSE!")
#define DCM_CHECK(expr) 				dcm_retm_if(!(expr), "Invalid parameter, return!")


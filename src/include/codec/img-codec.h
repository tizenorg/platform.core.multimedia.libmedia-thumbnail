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

#ifndef _IMGCODEC_H_
#define _IMGCODEC_H_

#include "img-codec-common.h"
#include "img-codec-parser.h"
#include "img-codec-agif.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

unsigned int* ImgGetFirstFrameAGIFAtSize(const char *szFileName, unsigned int width, unsigned int height);

int ImgConvertRGB565ToRGB888(void *pBuf_rgb565, void **pBuf_rgb888, int width, int height);


AGifFrameInfo*	ImgCreateAGIFFrame(const char *szFileName, unsigned int width, unsigned int height, unsigned int bgColor, BOOL bLoop);

void				ImgDestroyAGIFFrame(AGifFrameInfo* pFrameData);

ImgFastCodecInfo	ImgGetNextAGIFFrame (AGifFrameInfo *gFrameData, BOOL bCenterAlign);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif		// _IMGCODEC_H_

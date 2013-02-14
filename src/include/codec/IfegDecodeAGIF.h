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

#ifndef _IFEG_DECODE_GIF_H_
#define _IFEG_DECODE_GIF_H_



#include "AGifFrameInfo.h"
#include "img-codec-common.h"



#define MODE 0

#define MAX_GIF_HEADER_SIZE 4096

#if MODE == 2
#define MAXBUFFMEMORY 330000
#endif

#define MAXWIDTH 4096
#define MAXHEIGHT 4096

#ifdef __cplusplus
extern "C" {
#endif


int FastImgGetNextFrameAGIF (AGifFrameInfo* pFrameData, BOOL bCenterAlign);
AGifFrameInfo* FastImgCreateAGIFFrameData(unsigned int width, unsigned int height, unsigned char *pEncodedData, unsigned int file_size, unsigned int ui_backcolor, BOOL bLoop);
void FastImgDestroyAGIFFrameData(AGifFrameInfo* pFrameData);

#ifdef __cplusplus
}
#endif


#endif /*_IFEG_DECODER_H_*/

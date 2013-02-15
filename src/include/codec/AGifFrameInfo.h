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

#ifndef _AGIFFRAMEINFO_H_
#define _AGIFFRAMEINFO_H_

typedef struct tagFrameInfo
{
	int height;
	int	width;
	unsigned int backcolor;
	unsigned int ui_backcolor;
	int imgCount;
	int inputSize;

	unsigned char *pEncodedData;
	void *pOutBits;					

	unsigned char *pPrevImg;		
	unsigned int *pGlobal_table;	
	unsigned short *pPrefix;		
	unsigned char *pDstack;	
	unsigned char *pSuffix;			

	int flag;
	int size;
	int useBuffer;
	int bLoop;
	int global_numcol;
	int resizedwidth;
	int resizedheight;
	int logi_wdt;
	int logi_hgt;
	int offset;
	int firstpos;

	unsigned short delay;

#ifdef INTERNAL_IMGCODEC
	unsigned int nRepeatCount;
	unsigned int nLoopCount;

	unsigned char bOneFrame;
#endif
}AGifFrameInfo;

#endif  //  _AGIFFRAMEINFO_H_

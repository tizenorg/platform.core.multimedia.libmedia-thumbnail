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

#ifndef _IMGCODEC_PARSER_H_
#define _IMGCODEC_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define		MINIMUM_HEADER_BYTES	8

#define		PNG_HEADER_LENGTH	8
#define		JPG_HEADER_LENGTH	2
#define		GIF_HEADER_LENGTH	3 
#define		TIFF_HEADER_LENGTH	2
#define		BMP_HEADER_LENGTH	2
#define		TIFF_IMAGE_WIDTH	0x100
#define		TIFF_IMAGE_HEIGHT	0x101

ImgCodecType ImgGetInfo(unsigned char* pEncodedData, unsigned long fileSize, ImgImageInfo* imgInfo);
ImgCodecType ImgGetInfoFile(const char*filePath, ImgImageInfo *imgInfo);
ImgCodecType ImgGetInfoHFile(HFile hFile, unsigned long fileSize, ImgImageInfo* imgInfo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


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
#define		WBMP_HEADER_LENGTH	2
#define		TIFF_IMAGE_WIDTH	0x100
#define		TIFF_IMAGE_HEIGHT	0x101

typedef enum
{
	IMG_CODEC_UNKNOWN_TYPE = -2,
	IMG_CODEC_BIG_PROGRESSIVE_JPEG = -1,
	IMG_CODEC_NONE	= 0,
	IMG_CODEC_GIF	= ( 1 << 0),
	IMG_CODEC_PNG	= ( 1 << 1),
	IMG_CODEC_WBMP	= ( 1 << 2),
	IMG_CODEC_JPEG	= ( 1 << 3),
	IMG_CODEC_BMP	= ( 1 << 4),
	IMG_CODEC_TIF	= ( 1 << 5),
	IMG_CODEC_AGIF	= ( 1 << 6),
	IMG_CODEC_PROGRESSIVE_JPEG = ( 1 << 7),
	IMG_CODEC_DRM	 = ( 1 << 8),
} ImgCodecType;

//ImgCodecType ImgGetInfoFile(const char*filePath, ImgImageInfo *imgInfo);
int ImgGetImageInfo(const char *filePath, ImgCodecType *type, unsigned int *width, unsigned int *height);
int ImgGetImageInfoForThumb(const char *filePath, ImgCodecType *type, unsigned int *width, unsigned int *height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


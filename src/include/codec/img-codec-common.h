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

#ifndef _IMGCODEC_COMMON_H_
#define _IMGCODEC_COMMON_H_

#include "img-codec-config.h"
#include "img-codec-osal.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//////////////////////////////////////////////////////////////

/* Maximum Image File Size Supported */
#define	IMG_MAX_IMAGE_FILE_SIZE			(10 * 1024 * 1024)

//////////////////////////////////////////////////////////////
#if 0
#define ImgDebug(type, fmt, arg...)	\
	do { \
		fprintf(stderr, "[Media-SVC]"fmt, ##arg);	\
	}while(0)


#define SysRequireEx(expr, retValue)	\
	if (!(expr)) {																			\
		fprintf(stderr, "[Media-SVC][%s] INVALID_PARAM (%d lines in %s)\n",__FUNCTION__,  __LINE__, __FILE__);  \
		return (retValue);																	\
	}
#define SysDebug(X)		ImgDebug X

#define SysAssert(expr)
#else
#define SysRequireEx(expr, retValue)
#define SysDebug(expr, retValue)
#define SysAssert(expr)
#endif

//////////////////////////////////////////////////////////////


typedef enum
{
	IMG_INFO_DECODING_FAIL 		=  0,
	IMG_INFO_DECODING_SUCCESS 	=  1,
	IMG_INFO_AGIF_LAST_FRAME 	=  2,
	IMG_INFO_TOO_BIG_FILE_SIZE 	= -1,
	IMG_INFO_TOO_LARGE_SCALE 	= -2,
	IMG_INFO_TOO_BIG_PROGRESSIVE= -3,
	IMG_INFO_UNKNOWN_FORMAT 	= -4,
	IMG_INFO_FILE_ERR				= -5,
	IMG_INFO_MEM_ALLOC_FAIL		= -6
}ImgFastCodecInfo;

typedef	enum inputFlag
{
	IMG_RGB_888_OUTPUT		= 0x0001,
	IMG_RGB_OUTPUT			= 0x0002,
	IMG_YUV_OUTPUT			= 0x0005
}ImgInputFlag;

typedef	enum resize
{
	/* During resizing aspect ratio will be maintained */
	IMG_RESIZE_MAINTAIN_ASPECT_RATIO	= 0x0001,
	IMG_RESIZE							= 0x0002
} ImgResizeInput;


typedef struct tagImgImageInfo
{
	unsigned int		width;
	unsigned int		height;
	unsigned int 		numberOfFrame;
	unsigned int		delay;	//deprecated.
	unsigned int		bOrientation;	//deprecated.
}ImgImageInfo;

typedef struct tagImgBitmap
{
	int		width;
	int		height;
	UCHAR	bitsPerPixel;  /* For TIF  it should be 8 or 4 or 1 always
						    * If coming directly from camera and
							* bitsperpixel is 16 then its assumed as 8
							* & color type as RGBC and compression
							* type as NO_COMP and encode it.
							*/

	void	*pBits;
    BOOL	bChromaKeyFlag;	/* Flag to indicate chroma key
							 * Default: FALSE
							 */
	ULONG	chromaKey;		/* This is the colour which needs to be (or is) transparent.
							 * This colour will be in 5, 5, 5 RGB format as below.
							 * First 16 MS Bits 0, R (5), G (5), 0 (1), B (5)
							 */
    UCHAR	disposal;		/* Disposal action to take during
							 *  display of current flag
							 *  Default: 0
							 */
	USHORT	delay;			/* Delay before display of
							 *  next frame. Default: 0
							 */
	BOOL	inputFlag;      /* User input required befflag used
							 * Default: FALSE
							 */

	BOOL	interlace;      /* Interlace indicator flag
							 *  Default: FALSE
							 */
    BYTE	*pLocalColorMap;/* Local color palette pointer
							 *  Default: NULL
							 */
							 /* For TIF :Should be given if  PALETTE color type.
							  */
    USHORT  localColorMapSize;
                             /* Local color palette size */
							 /* In TIF: Should be given if  PALETTE color type.
							  */
    void	*pAlphaChannel;	/* An alpha channel, representing transparency
							 * information on a per-pixel basis
							 */
	BYTE	colorType ;		/* Indicates the color type of image. It can be
							 * GRAY, COLOR, PALETTED, GRAY_WITH_ALPHA_CHANNEL or
							 * COLOR_WITH_ALPHA_CHANNEL
							 */
							 /* For TIF: IT could be TIF_COLORTYPE_BILEVEL,
							  * TIF_COLORTYPE_GRAY ,TIF_COLORTYPE_RGB,
							  * TIF_COLORTYPE_PALETTE or
							  * TIF_COLORTYPE_RGBPALETTE.
							  */
	BYTE	filter ;		/* Type of filter to apply. Could be one of NONE,
							 * SUB, UP, AVERAGE or PAETH
							 */
	BYTE	compressionMethod ; /* Type of compression in zLib to apply. Could be
								 * one of NONE, FIXED or DYANMIC
								 */
								 /*  For TIF : IT could be TIF_COMP_NOCOMP ,
								  * TIF_COMP_PACKBIT or TIF_COMP_HUFFMAN (only for bilevel)
  								  */

	UCHAR	colorConversion;	/* Indicates whether color conversion has to
								 * be done or not
								 */
								/* For TIF :This represents  white is zero or
								 * black is zero in case of bilevel & gray.
								 */
	UCHAR	downSampling;		/* Indicates whether down sampling needs to
								 * be done or not
								 */

	UCHAR		downSamplingMethod[3];
		/* Down sampling offsets for every component. Possible
		 * combinations are
		 *	1, 1, 1
		 *	1, 2, 1
		 *	1, 2, 2
		 */
} ImgBitmap;

typedef struct tagImgCodecColor
{
	UINT16	red;
	UINT16	green;
	UINT16	blue;
} ImgCodecColor;


typedef struct
{
    int				width, height, bpp;

    BOOL			tRNSFlag;
    ImgCodecColor	tRNSColor;

    BOOL			bKGDFlag;
    ImgCodecColor	bKGDColor;

    BOOL			AlphaChannelFlag;
    unsigned short	*pAlphaChannel;
} ImgTrnsInfo;

typedef struct tagImgBitmapElem ImgBitmapElem;

struct tagImgBitmapElem
{
	int					x;
	int					y;
	ImgBitmap			*pbmBitmap;
	ImgBitmapElem		*pNext;
};

typedef struct gifDecode	ImgGIFFrame ;

typedef struct tagImgImage
{
    int				dataSize;			/* Holds the total number of bytes required
										 * to hold the image
										 */
    int				width;
    int				height;

    USHORT			flag;				/*	This contains flags as defined
										 *	in ImgCodecFlagType
										 */
    ULONG			backGroundColor;	/* The backgroundColor specifies a default background
										 * color to present the image against.
										 * Note that viewers are not bound to honor this color.
										 * A viewer can choose to use a different background.
										 */
    BYTE			*pGlobalColorMap;	/*	Global color palette pointer
										 *	Default: NULL
										 */
    USHORT          globalColorMapSize; /* Local color palette size */
    ULONG			gamma;				/* The value is encoded as a 4-byte unsigned integer,
										 * representing gamma times 100000. For example, a
										 * gamma of 0.45 would be stored as the integer 45000.
										 */
	int				bitmapCount;		/* This will hold the number of bitmaps in the
										 * ImgImage structure
										 */
	BYTE			function ;			/* This will be set as TRUE if this structure is
										 * used for encoder
										 */

	ImgBitmapElem	*pHead;
	ImgBitmapElem	*pTail;


	ULONG			decodedSize ;	/* 1 => Same as in the encoded stream
									 * 0 => Resize to QQVGA if size is more
									 * than 176 x 144
									 */

	BOOL			memAllocEx ;	/* TRUE => MemAllocEx is used,
									 * FALSE => MemAlloc is used
									 */

	USHORT			loopCount ;		/* This will contain the number of times to repeat
									 * the animation
									 */
	BOOL			bLoopCount ;	/* If GIF LoopCount is present then this will be
									 * TRUE, otherwise it will be false
									 */

	ImgGIFFrame		*pGifFrame ;	/* Stores the intermediate GIF Frame */

	ImgInputFlag	inputFlag;
	ImgResizeInput	resizeMethod ;

	/* Flag to identify whether image is AGIF or not */
	BOOL			bAGIFImage ;
	unsigned char	*pEncodedData ;
	INT32			cFileSize ;
	BOOL			bFirstFrame ;
	BOOL			bReservedMemoryFlag;
	unsigned int	offset ;
} ImgImage;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif		// _IMGCODEC_COMMON_H_

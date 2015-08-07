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

#include "media-thumb-debug.h"
#include "img-codec-common.h"
#include "IfegDecodeAGIF.h"



#define MAX_CODES   4096

int __FastImgGetNextFrameAGIF_NoBuffer (AGifFrameInfo *pFrameData, BOOL bCenterAlign);
int __FastImgGetNextFrameAGIF_UseBuffer (AGifFrameInfo *pFrameData, BOOL bCenterAlign);

int image_left_pos_N = 0;	/* left position of image in Logical screeen */
int image_top_pos_N = 0;

AGifFrameInfo *FastImgCreateAGIFFrameData(unsigned int width, unsigned int height, unsigned char *pEncodedData, unsigned int file_size, unsigned int ui_backcolor, BOOL bLoop)
{
	unsigned int header_temp;
	int backcolor_index;
	unsigned int image_backcolor;
	unsigned int backcolor_parsing;
	int transparent = 0;
	int transIndex = 0;
	int inputPos = 0;

	AGifFrameInfo *pFrameData;
	if (0 == (pFrameData = IfegMemAlloc(sizeof(AGifFrameInfo)))) {
		return 0;
	}

	IfegMemset(pFrameData, 0, sizeof(AGifFrameInfo));

	inputPos += 13;

	pFrameData->logi_wdt = pEncodedData[6] | (pEncodedData[7]<<8);
	pFrameData->logi_hgt = pEncodedData[8] | (pEncodedData[9]<<8);
#ifdef INTERNAL_IMGCODEC
	thumb_dbg("logi_wdt:%d, logi_hgt:%d", pFrameData->logi_wdt, pFrameData->logi_hgt);
#else
	thumb_dbg("logi_wdt:%d, logi_hgt:%d", pFrameData->logi_wdt, pFrameData->logi_hgt);
	if ((pFrameData->logi_wdt > MAXWIDTH) || (pFrameData->logi_hgt > MAXHEIGHT)) {
		if (pFrameData) {
				IfegMemFree(pFrameData);
				pFrameData = 0;
		}
		return 0;
	}
#endif

	/* save nReapeatCount assigned by content */

	if ((pEncodedData[10]&0x80) == 0x80) {
		header_temp = 3 * (1<<((pEncodedData[10] & 0x07) + 1));
	} else {
		header_temp = 0;
	}

	if (file_size > 30+header_temp && pEncodedData[14+header_temp] == 0xFF) {
		pFrameData->nLoopCount = pEncodedData[30+header_temp]<<8 | pEncodedData[29+header_temp];
	} else if (file_size > 30+8+header_temp && pEncodedData[14+8+header_temp] == 0xFF) {
		pFrameData->nLoopCount = pEncodedData[30+8+header_temp]<<8 | pEncodedData[29+8+header_temp];
	} else {
		pFrameData->nLoopCount = -1;
	}

	pFrameData->nRepeatCount = 0;

	thumb_dbg("10st data :  0x%x , global color table num : %d", pEncodedData[10], header_temp);
	thumb_dbg("13: 0x%x ,14: 0x%x, 15: 0x%x, nRepeatCount : %d, nLoopCount : %d", pEncodedData[13+header_temp], pEncodedData[14+header_temp], pEncodedData[15+header_temp], pFrameData->nRepeatCount, pFrameData->nLoopCount);

	backcolor_index	= pEncodedData[11];

	if (pEncodedData[14+header_temp] == 0xF9) {
		transparent = pEncodedData[16+header_temp] & 0x01;
		transIndex = 	pEncodedData[19+header_temp];

		backcolor_parsing = (unsigned short)((pEncodedData[inputPos+backcolor_index*3] >> 3)<<11) | ((pEncodedData[inputPos+backcolor_index*3+1] >> 2)<<5) | (pEncodedData[inputPos+backcolor_index*3+2] >> 3);
	} else if (pEncodedData[14+19+header_temp] == 0xF9) {
		transparent = pEncodedData[35+header_temp] & 0x01;
		transIndex = pEncodedData[38+header_temp];

		backcolor_parsing = (unsigned short)((pEncodedData[inputPos+backcolor_index*3] >> 3)<<11) | ((pEncodedData[inputPos+backcolor_index*3+1] >> 2)<<5) | (pEncodedData[inputPos+backcolor_index*3+2] >> 3);
	} else if ((pEncodedData[10]&0x80) != 0x80) { /* global color table */
		backcolor_parsing = ui_backcolor;
	} else {
		backcolor_parsing = (unsigned short)((pEncodedData[inputPos+backcolor_index*3] >> 3)<<11) | ((pEncodedData[inputPos+backcolor_index*3+1] >> 2)<<5) | (pEncodedData[inputPos+backcolor_index*3+2] >> 3);
	}

	/* graphic extension block */
	if (pEncodedData[14+header_temp] == 0xF9 || pEncodedData[14+19+header_temp] == 0xF9) {

		if (transparent == 1 && backcolor_index == transIndex) {
			image_backcolor =  ui_backcolor;
		} else if (transparent == 1 && backcolor_index != transIndex) {
			image_backcolor = backcolor_parsing;
		} else {
			image_backcolor =  backcolor_parsing;		
		}
	} else {
		image_backcolor =  backcolor_parsing;	
	}

#if MODE == 0
	if (0 == (pFrameData->pPrevImg = IfegMemAlloc(sizeof(unsigned char)*width*height*2))) {
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}
	pFrameData->useBuffer = 0;
#endif

#if MODE == 1
	if (0 == (pFrameData->pPrevImg = IfegMemAlloc(sizeof(unsigned int)*pFrameData->logi_wdt*pFrameData->logi_hgt))) {
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}
	pFrameData->useBuffer = 1;
#endif


#if MODE == 2
	if (((pFrameData->logi_wdt <= width) && (pFrameData->logi_hgt <= height))) {
		if (0 ==  (pFrameData->pPrevImg = IfegMemAlloc(sizeof(unsigned char)*width*height*2))) {
			if (pFrameData) {
				IfegMemFree(pFrameData);
				pFrameData = 0;
			}
			return 0;
		}
		pFrameData->useBuffer = 0;
	} else {
		if (0 ==  (pFrameData->pPrevImg = IfegMemAlloc(sizeof(unsigned int)*pFrameData->logi_wdt*pFrameData->logi_hgt))) {
			if (pFrameData) {
				IfegMemFree(pFrameData);
				pFrameData = 0;
			}
			return 0;
		}
		pFrameData->useBuffer = 1;
	}
#endif

	if (0 == (pFrameData->pOutBits = IfegMemAlloc(sizeof(unsigned char)*width*height*2))) {
		if (pFrameData->pPrevImg) {
			IfegMemFree(pFrameData->pPrevImg);
			pFrameData->pPrevImg = 0;
		}
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}

	pFrameData->inputSize = file_size;
	pFrameData->pEncodedData = pEncodedData;

	if (0 == (pFrameData->pGlobal_table = IfegMemAlloc(sizeof(unsigned int)*256))) {
		if (pFrameData->pPrevImg) {
			IfegMemFree(pFrameData->pPrevImg);
			pFrameData->pPrevImg = 0;
		}

		if (pFrameData->pOutBits) {
			IfegMemFree(pFrameData->pOutBits);
			pFrameData->pOutBits = 0;
		}

		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}

	if (0 == (pFrameData->pPrefix = IfegMemAlloc(sizeof(unsigned short)*4097))) {
		if (pFrameData->pPrevImg) {
			IfegMemFree(pFrameData->pPrevImg);
			pFrameData->pPrevImg = 0;
		}
		if (pFrameData->pOutBits) {
			IfegMemFree(pFrameData->pOutBits);
			pFrameData->pOutBits = 0;
		}
		if (pFrameData->pGlobal_table) {
			IfegMemFree(pFrameData->pGlobal_table);
			pFrameData->pGlobal_table = 0;
		}
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}

	if (0 == (pFrameData->pDstack = IfegMemAlloc(sizeof(unsigned char)*4097))) {
		if (pFrameData->pPrevImg) {
			IfegMemFree(pFrameData->pPrevImg);
			pFrameData->pPrevImg = 0;
		}
		if (pFrameData->pOutBits) {
			IfegMemFree(pFrameData->pOutBits);
			pFrameData->pOutBits = 0;
		}
		if (pFrameData->pGlobal_table) {
			IfegMemFree(pFrameData->pGlobal_table);
			pFrameData->pGlobal_table = 0;
		}
		if (pFrameData->pPrefix) {
			IfegMemFree(pFrameData->pPrefix);
			pFrameData->pPrefix = 0;
		}
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}

	if (0 == (pFrameData->pSuffix = IfegMemAlloc(sizeof(unsigned char)*4097))) {
		if (pFrameData->pPrevImg) {
			IfegMemFree(pFrameData->pPrevImg);
			pFrameData->pPrevImg = 0;
		}
		if (pFrameData->pOutBits) {
			IfegMemFree(pFrameData->pOutBits);
			pFrameData->pOutBits = 0;
		}
		if (pFrameData->pGlobal_table) {
			IfegMemFree(pFrameData->pGlobal_table);
			pFrameData->pGlobal_table = 0;
		}
		if (pFrameData->pPrefix) {
			IfegMemFree(pFrameData->pPrefix);
			pFrameData->pPrefix = 0;
		}
		if (pFrameData->pDstack) {
			IfegMemFree(pFrameData->pDstack);
			pFrameData->pDstack = 0;
		}
		if (pFrameData) {
			IfegMemFree(pFrameData);
			pFrameData = 0;
		}
		return 0;
	}

	pFrameData->width = width;
	pFrameData->height = height;
	pFrameData->imgCount = -1;
	pFrameData->offset = 0;
	pFrameData->backcolor = image_backcolor;
	pFrameData->ui_backcolor = ui_backcolor;
	pFrameData->delay = 10;
	pFrameData->bLoop = bLoop;
	return pFrameData;
}

void FastImgDestroyAGIFFrameData(AGifFrameInfo *pFrameData)
{
	if (pFrameData == NULL) {
		return;
	}
	
	if (pFrameData->pPrevImg) {
		IfegMemFree(pFrameData->pPrevImg);
		pFrameData->pPrevImg = 0;
	}

	if (pFrameData->pEncodedData) {
		IfegMemFree(pFrameData->pEncodedData);
		pFrameData->pEncodedData = 0;
	}
	if (pFrameData->pOutBits) {
		IfegMemFree(pFrameData->pOutBits);
		pFrameData->pOutBits = 0;
	}

	if (pFrameData->pGlobal_table) {
		IfegMemFree(pFrameData->pGlobal_table);
		pFrameData->pGlobal_table = 0;
	}

	if (pFrameData->pPrefix) {
		IfegMemFree(pFrameData->pPrefix);
		pFrameData->pPrefix = 0;
	}

	if (pFrameData->pDstack) {
		IfegMemFree(pFrameData->pDstack);
		pFrameData->pDstack = 0;
	}

	if (pFrameData->pSuffix) {
		IfegMemFree(pFrameData->pSuffix);
		pFrameData->pSuffix = 0;
	}

	if (pFrameData) {
		IfegMemFree(pFrameData);
		pFrameData = 0;
	}
}

/* macro */
#define __get_next_code_first_nbits_left_0(pInputStream) \
{\
	{\
		if (navail_bytes == 0) {\
			navail_bytes = (pInputStream)[inputPos++];\
			if ((inputPos + navail_bytes) > filesize) {\
				if (decoderline) {\
					IfegMemFree(decoderline);\
					decoderline = 0;\
				} \
				if (pDecBuf) {\
					IfegMemFree(pDecBuf);\
					pDecBuf = 0;\
				} \
				return -1;\
			} \
		} \
		b1 = (pInputStream)[inputPos++];\
		ret = b1;\
		nbits_left = 8;\
		--navail_bytes;\
	} \
}

#define __get_next_code_first_nbits_left_0_nobuffer(pInputStream) \
{\
	{\
		if (navail_bytes == 0) {\
			navail_bytes = (pInputStream)[inputPos++];\
			if ((inputPos + navail_bytes) > filesize) {\
				if (decoderline) {\
					IfegMemFree(decoderline);\
					decoderline = 0;\
				} \
				if (pDecBuf) {\
					IfegMemFree(pDecBuf);\
					pDecBuf = 0;\
				} \
				if (done_prefix) {\
					IfegMemFree(done_prefix);\
					done_prefix = 0;\
				} \
				return -1;\
			} \
		} \
		b1 = (pInputStream)[inputPos++];\
		ret = b1;\
		nbits_left = 8;\
		--navail_bytes;\
	} \
}

#define __get_next_code_first_nbits_left_not_0(pInputStream) \
{\
	{\
		ret = b1 >> (8 - nbits_left); \
	} \
}

#define __get_next_code_first_while(pInputStream) \
{\
	while (curr_size > nbits_left) {\
		if (navail_bytes == 0) {\
			navail_bytes = (pInputStream)[inputPos++];\
			if ((inputPos + navail_bytes) > filesize) {\
				if (decoderline) {\
					IfegMemFree(decoderline);\
					decoderline = 0;\
				} \
				if (pDecBuf) {\
					IfegMemFree(pDecBuf);\
					pDecBuf = 0;\
				} \
				return -1;\
			} \
		} \
		b1 = (pInputStream)[inputPos++];\
		ret |= b1 << nbits_left;\
		nbits_left += 8;\
		--navail_bytes;\
	} \
	nbits_left -= curr_size;\
	ret &= (1<<curr_size)-1;\
	c = ret;\
}

#define __get_next_code_first_while_nobuffer(pInputStream) \
{\
	while (curr_size > nbits_left) {\
		if (navail_bytes == 0) {\
			navail_bytes = (pInputStream)[inputPos++];\
			if ((inputPos + navail_bytes) > filesize) {\
				if (decoderline) {\
					IfegMemFree(decoderline);\
					decoderline = 0;\
				} \
				if (pDecBuf) {\
					IfegMemFree(pDecBuf);\
					pDecBuf = 0;\
				} \
				if (done_prefix) {\
					IfegMemFree(done_prefix);\
					done_prefix = 0;\
				} \
				return -1;\
			} \
		} \
		b1 = (pInputStream)[inputPos++];\
		ret |= b1 << nbits_left;\
		nbits_left += 8;\
		--navail_bytes;\
	} \
	nbits_left -= curr_size;\
	ret &= (1<<curr_size)-1;\
	c = ret;\
}

#define __get_next_code_second_nbits_left_0(pInputStream) \
{\
	{\
		b1 = (pInputStream)[inputPos++];\
		ret = b1;\
		nbits_left = 8;\
		--navail_bytes;\
	} \
}

#define __get_next_code_second_nbits_left_not_0(pInputStream) \
{\
	{\
		ret = b1 >> (8 - nbits_left);\
	} \
}

#define __get_next_code_second_while(pInputStream) \
{\
	while (curr_size > nbits_left) {\
		b1 = (pInputStream)[inputPos++];\
		ret |= b1 << nbits_left;\
		nbits_left += 8;\
		--navail_bytes;\
	} \
	nbits_left -= curr_size;\
	ret &= (1 << curr_size)-1;\
	c = ret;\
}

int FastImgGetNextFrameAGIF(AGifFrameInfo *pFrameData, BOOL bCenterAlign)
{
	if (pFrameData->useBuffer) {
		return __FastImgGetNextFrameAGIF_UseBuffer(pFrameData, bCenterAlign);
	} else {
		return __FastImgGetNextFrameAGIF_NoBuffer(pFrameData, bCenterAlign);
	}
}

int __FastImgGetNextFrameAGIF_NoBuffer(AGifFrameInfo *pFrameData, BOOL bCenterAlign)
{
	unsigned int *pImage32, *pImage32_2;
	unsigned short *pImage16;
	unsigned char *sp;
	unsigned char *bufptr;
	unsigned int code, fc, oc, bufcnt;
	unsigned char buffer[16];
	unsigned char val1;
	unsigned char *buf;
	unsigned int *pDacbox;
	unsigned char *decoderline = 0;
	unsigned int size;

	unsigned int c = 0;
	unsigned int clear;
	unsigned int ending;
	unsigned int newcodes;
	unsigned int top_slot;
	unsigned int slot;

	int	numcolors;
	int	i, j, k;
	int	rowcount;
	int orgwdt_1, orgwdt, orghgt;
	int decwdt, dechgt;
	int len;
	int inter_step, interLaced;
	int transparent = 0;
	int startloc = 0;
	int transIndex = 0;
	int spCount = 0;
	int logi_wdt;		/* logical screen width */
	int logi_hgt;
	int image_backcolor;
	int ui_backcolor;
	int backcolor;
	int image_left_pos;	/* left position of image in Logical screeen */
	int image_top_pos;
	int disposal_method = 0;
	int margin_wdt1_2, margin_hgt1_2;
	int inputPos;

	unsigned int dacbox[256];

	/* Variable for Resize */
	unsigned int a_x, a_y;
	unsigned int b_x, b_y;
	unsigned int c_x, c_y;
	unsigned int d_x, d_y;
	int out_x = 0, out_y = 0;
	int d1, d2;
	int flag = 0;
	unsigned int end;

	unsigned char *pDecBuf = 0;

	int expected_width = pFrameData->width;
	int expected_height = pFrameData->height;
	int resized_width;
	int resized_height;

	/* macro */
	register unsigned int curr_size;
	register int navail_bytes = 0;
	register unsigned int nbits_left = 0;
	register unsigned int b1 = 0;
	register unsigned int ret;

	/* parameter */
	int filesize = pFrameData->inputSize;
	unsigned int *global_dacbox = pFrameData->pGlobal_table;
	register unsigned char *pInputStream = pFrameData->pEncodedData;
	void *pOutBits = pFrameData->pOutBits;
	unsigned short *prefix = pFrameData->pPrefix;
	unsigned char *dstack = pFrameData->pDstack;
	unsigned char *suffix = pFrameData->pSuffix;
	unsigned char *done_prefix = 0;

	inputPos = pFrameData->offset;

	done_prefix = IfegMemAlloc(sizeof(unsigned char)*(MAX_CODES+1));
	if (done_prefix == 0)
	{
		thumb_err("Failed to allocate memory for check buffer.");
		return -1;
	}
	IfegMemset(prefix, 0, sizeof(unsigned short)*(MAX_CODES+1));
	IfegMemset(dstack, 0, sizeof(unsigned char)*(MAX_CODES+1));
	IfegMemset(suffix, 0, sizeof(unsigned char)*(MAX_CODES+1));

	image_backcolor = pFrameData->backcolor;
	ui_backcolor = pFrameData->ui_backcolor;

	backcolor = image_backcolor;

	if (pFrameData->imgCount == -1) {
		/* Input stream index set to 0 */
		inputPos = 0;
		inter_step = 1;
		interLaced = 0; 
		transparent = 0; 

		/* read a GIF HEADER */
		IfegMemcpy(buffer, pInputStream, 13);
		inputPos += 13;

		/* wheather GIF file or not */
		if (buffer[0] != 'G' || buffer[1] != 'I' || buffer[2] != 'F' ||
			buffer[3] < '0' || buffer[3] > '9' ||
			buffer[4] < '0' || buffer[4] > '9' ||
			buffer[5] < 'A' || buffer[5] > 'z') {
			if (decoderline != 0) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf != 0) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			IfegMemFree(done_prefix);
			done_prefix = 0;
			return -1;
		}

		/* Regard the width/height of image block as the size of thumbnails. */
		pFrameData->logi_wdt = logi_wdt = expected_width;
		pFrameData->logi_hgt = logi_hgt = expected_height;

		/* ouput resized image size */
		if (logi_wdt <= expected_width && logi_hgt <= expected_height) {
			resized_width = logi_wdt;
			resized_height = logi_hgt;
			pFrameData->flag = flag = 1;
		} else {
			if ((logi_wdt/(float)expected_width) >= (logi_hgt/(float)expected_height)) {
				resized_height = logi_hgt * expected_width / logi_wdt;
				resized_width = expected_width;
			} else {
				resized_width = logi_wdt * expected_height / logi_hgt;
				resized_height = expected_height;
			}
		}
		if (!resized_width || !resized_height) {
			if (pDecBuf != 0) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			IfegMemFree(done_prefix);
			done_prefix = 0;
			return 0;
		}

		/* ouput resized image size */
		pFrameData->resizedwidth = resized_width;
		pFrameData->resizedheight = resized_height;

		/* Color Resolution */
		IfegMemset(global_dacbox, 0, 1024);
		numcolors = (buffer[10] & 0x7) + 1;
		if ((buffer[10] & 0x80) == 0) { /* Global color table */
			global_dacbox[0] = 0x000000;
			global_dacbox[1] = 0x3f3f3f;
			numcolors = 2;
		} else { /* Global color table */
			numcolors = 1 << numcolors;

			/* Make color table */
			for (i = 0 ; i < numcolors; i++) {
				global_dacbox[i] = ((pInputStream[inputPos++] >> 2)<<16);
				global_dacbox[i] |= ((pInputStream[inputPos++] >> 2)<<8);
				global_dacbox[i] |= (pInputStream[inputPos++] >> 2);
			}
		}

		/* Background Color */
		pImage16 = (unsigned short *)pFrameData->pPrevImg;
		for (i = 0; i < expected_width * expected_height; i++) {
			*pImage16++ = ui_backcolor;
		}

		if (numcolors > 16) {
			numcolors = 256;
		}
		if (numcolors > 2 && numcolors < 16) {
			numcolors = 16;
		}

		pFrameData->global_numcol = numcolors;
		pFrameData->imgCount++;
		pFrameData->firstpos = inputPos;
	} else {

		logi_wdt = pFrameData->logi_wdt;
		logi_hgt = pFrameData->logi_hgt;
		resized_width = pFrameData->resizedwidth;
		resized_height = pFrameData->resizedheight;

		flag = pFrameData->flag ;

		numcolors = pFrameData->global_numcol;
	}


	while (1) {
		if (inputPos > filesize) {
			if (decoderline) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			pFrameData->imgCount = 0;
			pFrameData->offset = pFrameData->firstpos;

			if (pFrameData->bLoop) {
				/* Background Color */
				pImage16 = (unsigned short *)pFrameData->pPrevImg;
				for (i = 0; i < expected_width * expected_height; i++) {
					*pImage16++ = ui_backcolor;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 1;
			} else {
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 0;
			}
		}

		switch (pInputStream[inputPos++]) {
		case 0x3b: /* End of the GIF dataset */
			if (decoderline) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			if (pFrameData->imgCount == 0) {
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return -1;
			}
			pFrameData->imgCount = 0;
			pFrameData->offset = pFrameData->firstpos;

#ifdef INTERNAL_IMGCODEC
			pFrameData->nRepeatCount++;
			thumb_dbg("(no_buffer) bLoop : %d, nRepeatCount : %d, nLoopCount : %d" , pFrameData->bLoop, pFrameData->nRepeatCount , pFrameData->nLoopCount);

			if (pFrameData->nLoopCount == -1) {
				break;
			} else if (pFrameData->bLoop || (pFrameData->nRepeatCount <= pFrameData->nLoopCount) || (pFrameData->nLoopCount == 0)) {
				/* Background Color */
				pImage16 = (unsigned short *)pFrameData->pPrevImg;

				for (i = 0; i < expected_width * expected_height; i++) {
					*pImage16++ = ui_backcolor;
				}

				inputPos = pFrameData->offset;
				continue;
			} else {
				/* if there is last frame and bLoop is FALSE, return 2. */
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 2;
			}
#else
			if (pFrameData->bLoop) {
				/* Background Color */
				pImage16 = (unsigned short *)pFrameData->pPrevImg;
				for (i = 0; i < expected_width * expected_height; i++) {
					*pImage16++ = backcolor;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 1;
			} else {
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 0;
			}
#endif
			break;
		case 0x21: /* Extension Block */
			switch (pInputStream[inputPos++]) {
			case 0xf9: /* Graphic control extension block */
				if (4 != pInputStream[inputPos++]) { /* data length : fixed 4 bytes */
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					IfegMemFree(done_prefix);
					done_prefix = 0;
					return -1;
				}

				disposal_method = ((pInputStream[inputPos] & 0x1c) >> 2);
				transparent = pInputStream[inputPos++] & 0x01;	/* does it use? 1:on 0:off */
				pFrameData->delay = (pInputStream[inputPos] | (pInputStream[inputPos+1] << 8))*10;
				inputPos += 2; /* Delay time (skip) */
				transIndex = pInputStream[inputPos++];
				inputPos++; /* block end */
				break;

			case 0x01: /* Plain Text block */
				while ((i = pInputStream[inputPos++]) > 0) {/* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}
						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						IfegMemFree(done_prefix);
						done_prefix = 0;
						return -1;
					}
				}
				break;

			case 0xfe: /* Comment Extension block */
				while ((i = pInputStream[inputPos++]) > 0) { /* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}
						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						IfegMemFree(done_prefix);
						done_prefix = 0;
						return -1;
					}
				}
				break;

			case 0xff: /* Appliation Extension block */
				while ((i = pInputStream[inputPos++]) > 0) {	/* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}

						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						IfegMemFree(done_prefix);
						done_prefix = 0;
						return -1;
					}
				}
				break;

			default:
				break;
			}
			break;

		case 0x2c: /* Start of an image object. Read the image description. */

			/* initialize */
			IfegMemcpy(pOutBits, pFrameData->pPrevImg, expected_width * expected_height * 2);
			pDacbox = global_dacbox;

			IfegMemcpy(buffer, pInputStream+inputPos, 9);
			inputPos += 9;

			image_left_pos = (buffer[0] | (buffer[1]<<8));
			image_top_pos = (buffer[2] | (buffer[3]<<8));
			orgwdt_1 = orgwdt = (buffer[4] | (buffer[5] << 8));
			orghgt = (buffer[6] | (buffer[7] << 8));

#ifdef INTERNAL_IMGCODEC
#else
			if ((orgwdt > MAXWIDTH) || (orghgt > MAXHEIGHT)) {
				if (decoderline != 0) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf != 0) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return -1;
			}
#endif

			/* Interlaced check */
			interLaced = buffer[8] & 0x40;
			if (interLaced) {
				startloc = -8;
			}

			inter_step = 1;

			if (decoderline) {
				IfegMemFree(decoderline);
			}
			decoderline = (unsigned char *)IfegMemAlloc(orgwdt);
			if (!decoderline) {
				if (decoderline != 0) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf != 0) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 0;
			}
			IfegMemset(decoderline, 0, orgwdt);

			decwdt = ((orgwdt * resized_width+logi_wdt-1) / logi_wdt);
			dechgt = ((orghgt * resized_height+logi_hgt-1) / logi_hgt);

			if (!decwdt || !dechgt) {
				if (decoderline != 0) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf != 0) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 0;
			}

			if (pDecBuf) {
				IfegMemFree(pDecBuf);
			}
			pDecBuf = (unsigned char *)IfegMemAlloc(decwdt * dechgt * 4);
			if (!pDecBuf) {
				if (decoderline != 0) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf != 0) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return 0;
			}
			IfegMemset(pDecBuf, 0, decwdt * dechgt * 4);

			/* assign out_888_image plane */
			pImage32 = (unsigned int *)(pDecBuf);

			/* Initialize */
			a_x = orgwdt>>2, a_y = orghgt>>2;
			b_x = ((orgwdt*3)>>2), b_y = orghgt>>2;
			c_x = orgwdt>>2, c_y = ((orghgt*3)>>2);
			d_x = ((orgwdt*3)>>2), d_y = ((orghgt*3)>>2);

			end = dechgt * orghgt;
			out_x = 0, out_y = -dechgt;

			/* Color Resolution */
			if ((buffer[8] & 0x80) == 0) { /* Logical color table  */
				/* use global table */
			} else { /* Logical color table */
				IfegMemset(dacbox, 0, 1024);

				numcolors = (buffer[8] & 0x7) + 1;
				numcolors = 1 << numcolors;

				/* Make color table */
				for (i = 0 ; i < numcolors; i++) {
					dacbox[i] = ((pInputStream[inputPos++] >> 2)<<16);
					dacbox[i] |= ((pInputStream[inputPos++] >> 2)<<8);
					dacbox[i] |= (pInputStream[inputPos++] >> 2);
				}
				pDacbox = dacbox;
			}

			if (numcolors > 16) {
				numcolors = 256;
			}
			if (numcolors > 2 && numcolors < 16) {
				numcolors = 16;
			}

			/****************************************************************************
			decoder(WDT, pInputStream, pBitmapElem->pBits);
			int decoder(int linewidth, UCHAR* pInputStream, UCHAR *pBitmapElem->pBits)
			****************************************************************************/

			size = pInputStream[inputPos++];
			if (size < 2 || 9 < size) {
				if (decoderline != 0) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf != 0) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;
				return -1;
			}

			rowcount = oc = fc = 0;
			buf = decoderline;
			sp = dstack;
			bufptr = buf;
			bufcnt = orgwdt_1;

			/************************
			init_exp(size);
			int init_exp(int size)
			************************/
			curr_size = size + 1;
			top_slot = 1 << curr_size;
			clear = 1 << size;
			ending = clear + 1;
			slot = newcodes = ending + 1;
			navail_bytes = nbits_left = 0;
			/************************/


			/* __get_next_code(pInputStream) */
			if (navail_bytes < 2) {
				if (nbits_left == 0) {
					__get_next_code_first_nbits_left_0_nobuffer(pInputStream)
				} else
					__get_next_code_first_nbits_left_not_0(pInputStream)

				__get_next_code_first_while_nobuffer(pInputStream)
			} else {
				if (nbits_left == 0)
					__get_next_code_second_nbits_left_0(pInputStream)
				else
					__get_next_code_second_nbits_left_not_0(pInputStream)

				__get_next_code_second_while(pInputStream)
			}

			if (c == ending) {
				break;
			}
			/**********************************************************/

			if (c == clear) {
				curr_size = size + 1;
				slot = newcodes;
				top_slot = 1 << curr_size;

				do {
					/* __get_next_code(pInputStream); */
					if (navail_bytes < 2) {
						if (nbits_left == 0)
							__get_next_code_first_nbits_left_0_nobuffer(pInputStream)
						else
							__get_next_code_first_nbits_left_not_0(pInputStream)

						__get_next_code_first_while_nobuffer(pInputStream)
					} else {
						if (nbits_left == 0)
							__get_next_code_second_nbits_left_0(pInputStream)
						else
							__get_next_code_second_nbits_left_not_0(pInputStream)

						__get_next_code_second_while(pInputStream)
					}

				} while (c == clear);

				if (c == ending) {
					break;
				}

				if (c >= slot) {
					c = 0;
				}

				oc = fc = c;
				*sp++ = (unsigned char)c;
			} else {
				if (c >= slot) {
					c = 0;
				}

				oc = fc = c;
				*sp++ = (unsigned char)c;
			}

			while (rowcount < orghgt) {
				if ((sp - dstack) > 0) {
					spCount = sp - dstack;
				}
				/* Now that we've pushed the decoded string (in reverse order)
				* onto the stack, lets pop it off and put it into our decode
				* buffer...  And when the decode buffer is full, write another
				* line...
				*/
				while (sp > dstack) {
					--sp;
					*bufptr++ = *sp;

					if (--bufcnt == 0) {

						/********************************************************************************
						if ((ret = put_line(rowcount++, bufptr - buf, WDT, buf, pBitmapElem->pBits)) < 0)
						********************************************************************************/
						rowcount++;
						len = bufptr - buf;
						if (len >= orgwdt) {
							len = orgwdt;
						}

						if (interLaced == 0) { /* interlaced image */
							if (!flag) {
								out_x = 0, out_y += dechgt;
							}
						} else {  /* interlaced image */
							if (inter_step == 1) {
								startloc += 8;
							} else if (inter_step == 2) {
								startloc += 8;
							} else if (inter_step == 3) {
								startloc += 4;
							} else if (inter_step == 4) {
								startloc += 2;
							}

							if (startloc >= orghgt) {
								inter_step++;
								if (inter_step == 2) {
									startloc = 4;
								} else if (inter_step == 3) {
									startloc = 2;
								} else if (inter_step == 4) {
									startloc = 1;
								}
							}

							/* gif to rgb 565 */
							if (flag) {
								pImage32 = (unsigned int *)(pDecBuf) + startloc * decwdt;
							} else {
								out_x = 0, out_y = startloc * dechgt;
								a_x = orgwdt>>2;
								b_x = ((orgwdt*3)>>2);
								c_x = orgwdt>>2;
								d_x = ((orgwdt*3)>>2);
								if (out_y <= (orghgt >> 2)) {
									a_y = orghgt>>2;
									b_y = orghgt>>2;
									c_y = ((orghgt*3)>>2);
									d_y = ((orghgt*3)>>2);
								} else {
									if (((out_y%orghgt) - (orghgt>>2)) > 0) {
										a_y = ((out_y/orghgt)+1) * orghgt + (orghgt>>2);
									} else if (((out_y%orghgt) - (orghgt>>2)) < 0 || !(out_y%orghgt)) {
										a_y = ((out_y/orghgt)) * orghgt + (orghgt>>2);
									} else {
										a_y = out_y;
									}

									if (((out_y % orghgt) - ((orghgt * 3) >> 2)) > 0) {
										c_y = ((out_y/orghgt)+1) * orghgt + ((orghgt*3)>>2);
									} else if (((out_y%orghgt) - ((orghgt*3)>>2)) < 0 || !(out_y%orghgt)) {
										c_y = ((out_y/orghgt)) * orghgt + ((orghgt * 3) >> 2);
									} else {
										c_y = out_y;
									}
									b_y = a_y, d_y = c_y;
								}
							}
						}

						if (transparent == 1) {
							if (flag) {
								for (i = 0; i < len; i++) {
									val1 = buf[i] & (numcolors-1);

									if (val1 == transIndex) {
										*pImage32++ = 0x4000000; /* Set *pImage32 MSB 1 */
									} else {
										*pImage32++ = pDacbox[val1]<<2;
									}
								}
							} else {
								if (c_y < end) {
									d1 = a_y - out_y;
									d2 = c_y - out_y;
									if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
										pImage32 = (unsigned int *)(pDecBuf) + (a_y/orghgt) * decwdt;
										pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/orghgt) * decwdt;

										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = a_x - out_x;
											d2 = b_x - out_x;

											if (0 <= d1 && d1 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32 + (a_x/orgwdt)) += 0x1000000;
													*(pImage32_2 + (c_x/orgwdt)) += 0x1000000;
													a_x += orgwdt, c_x += orgwdt;
												} else {
													*(pImage32 + (a_x/orgwdt)) += pDacbox[val1];
													*(pImage32_2 + (c_x/orgwdt)) += pDacbox[val1];
													a_x += orgwdt, c_x += orgwdt;
												}
											}
											if (0 <= d2  && d2 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32 + (b_x/orgwdt)) += 0x1000000;
													*(pImage32_2 + (d_x/orgwdt)) += 0x1000000;
													b_x += orgwdt, d_x += orgwdt;
												} else {
													*(pImage32 + (b_x/orgwdt)) += pDacbox[val1];
													*(pImage32_2 + (d_x/orgwdt)) += pDacbox[val1];
													b_x += orgwdt, d_x += orgwdt;
												}
											}
											out_x += decwdt;
										}
										if (!interLaced) {
											a_x = orgwdt>>2, a_y += orghgt;
											b_x = ((orgwdt*3)>>2), b_y += orghgt;
											c_x = orgwdt>>2, c_y += orghgt;
											d_x = ((orgwdt*3)>>2), d_y += orghgt;
										}
									} else if (0 <= d1 && d1 < dechgt) {
										pImage32 = (unsigned int *)(pDecBuf) + (a_y/orghgt) * decwdt;

										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = a_x - out_x;
											d2 = b_x - out_x;
											if (0 <= d1 && d1 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32 + (a_x/orgwdt)) += 0x1000000;
													a_x += orgwdt;
												} else {
													*(pImage32 + (a_x/orgwdt)) += pDacbox[val1];
													a_x += orgwdt;
												}
											} if (0 <= d2 && d2 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32 + (b_x/orgwdt)) += 0x1000000;
													b_x += orgwdt;
												} else {
													*(pImage32 + (b_x/orgwdt)) += pDacbox[val1];
													b_x += orgwdt;
												}
											}
											out_x += decwdt;
										}
										if (!interLaced) {
											a_x = orgwdt>>2, a_y += orghgt;
											b_x = ((orgwdt*3)>>2), b_y += orghgt;
										}
									} else if (0 <= d2 && d2 < dechgt) {
										pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/orghgt) * decwdt;
										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = c_x - out_x;
											d2 = d_x - out_x;
											if (0 <= d1 && d1 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32_2 + (c_x/orgwdt)) += 0x1000000;
													c_x += orgwdt;
												} else {
													*(pImage32_2 + (c_x/orgwdt)) += pDacbox[val1];
													c_x += orgwdt;
												}
											}
											if (0 <= d2  && d2 < decwdt) {
												if (val1 == transIndex) {
													*(pImage32_2 + (d_x/orgwdt)) += 0x1000000;
													d_x += orgwdt;
												} else {
													*(pImage32_2 + (d_x/orgwdt)) += pDacbox[val1];
													d_x += orgwdt;
												}
											}
											out_x += decwdt;
										}
										if (!interLaced) {
											c_x = orgwdt>>2, c_y += orghgt;
											d_x = ((orgwdt*3)>>2), d_y += orghgt;
										}
									}
								}
							}
						} else {
							if (flag) {
								for (i = 0; i < len; i++) {
									val1 = buf[i] & (numcolors-1);
									*pImage32++ = pDacbox[val1]<<2;
								}
							} else {
								if (c_y < end) {
									d1 = a_y - out_y;
									d2 = c_y - out_y;
									if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
										pImage32 = (unsigned int *)(pDecBuf) + (a_y/orghgt) * decwdt;
										pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/orghgt) * decwdt;
										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = a_x - out_x;
											d2 = b_x - out_x;
											if (0 <= d1 && d1 < decwdt) {
												*(pImage32 + (a_x/orgwdt)) += pDacbox[val1];
												*(pImage32_2 + (c_x/orgwdt)) += pDacbox[val1];
												a_x += orgwdt, c_x += orgwdt;
											}
											if (0 <= d2  && d2 < decwdt) {
												*(pImage32 + (b_x/orgwdt)) += pDacbox[val1];
												*(pImage32_2 + (d_x/orgwdt)) += pDacbox[val1];
												b_x += orgwdt, d_x += orgwdt;
											}
											out_x += decwdt;
										}
										if (!interLaced) {
											a_x = orgwdt>>2, a_y += orghgt;
											b_x = ((orgwdt*3)>>2), b_y += orghgt;
											c_x = orgwdt>>2, c_y += orghgt;
											d_x = ((orgwdt*3)>>2), d_y += orghgt;
										}
									} else if (0 <= d1 && d1 < dechgt) {
										pImage32 = (unsigned int *)(pDecBuf) + (a_y/orghgt) * decwdt;
										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = a_x - out_x;
											d2 = b_x - out_x;
											if (0 <= d1 && d1 < decwdt) {
												*(pImage32 + (a_x/orgwdt)) += pDacbox[val1];
												a_x += orgwdt;
											}
											if (0 <= d2  && d2 < decwdt) {
												*(pImage32 + (b_x/orgwdt)) += pDacbox[val1];
												b_x += orgwdt;
											}
											out_x += decwdt;
										}
										if (!interLaced) {
											a_x = orgwdt>>2, a_y += orghgt;
											b_x = ((orgwdt*3)>>2), b_y += orghgt;
										}
									} else if (0 <= d2 && d2 < dechgt) {
										pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/orghgt) * decwdt;
										for (i = 0; i < orgwdt; i++) {
											val1 = buf[i] & (numcolors-1);

											d1 = c_x - out_x;
											d2 = d_x - out_x;
											if (0 <= d1 && d1 < decwdt) {
												*(pImage32_2 + (c_x/orgwdt)) += pDacbox[val1];
												c_x += orgwdt;
											}
											if (0 <= d2 && d2 < decwdt) {
												*(pImage32_2 + (d_x/orgwdt)) += pDacbox[val1];
												d_x += orgwdt;
											}
											out_x += decwdt;
										}

										if (!interLaced) {
											c_x = orgwdt>>2, c_y += orghgt;
											d_x = ((orgwdt*3)>>2), d_y += orghgt;
										}
									}
								}
							}
						}
								bufptr = buf;
								bufcnt = orgwdt_1;
					}
				}

				if (rowcount == orghgt) {
					break;
				}

				/* __get_next_code(pInputStream) */
				if (navail_bytes < 2) {
					if (nbits_left == 0)
						__get_next_code_first_nbits_left_0_nobuffer(pInputStream)
					else
						__get_next_code_first_nbits_left_not_0(pInputStream)
					__get_next_code_first_while_nobuffer(pInputStream)
				} else {
					if (nbits_left == 0)
						__get_next_code_second_nbits_left_0(pInputStream)
					else
						__get_next_code_second_nbits_left_not_0(pInputStream)

					__get_next_code_second_while(pInputStream)
				}

				if (c == ending) {
					break;
				}
				/*************************************************************/

				if (c == clear) {
					curr_size = size + 1;
					slot = newcodes;
					top_slot = 1 << curr_size;

					do {
						/* __get_next_code(pInputStream); */
						if (navail_bytes < 2) {
							if (nbits_left == 0)
								__get_next_code_first_nbits_left_0_nobuffer(pInputStream)
							else
								__get_next_code_first_nbits_left_not_0(pInputStream)
							__get_next_code_first_while_nobuffer(pInputStream)
						} else {
							if (nbits_left == 0)
								__get_next_code_second_nbits_left_0(pInputStream)
							else
								__get_next_code_second_nbits_left_not_0(pInputStream)

							__get_next_code_second_while(pInputStream)
						}
					} while (c == clear);

					if (c == ending) {
						break;
					}

					if (c >= slot) {
						c = 0;
					}

						oc = fc = c;
						*sp++ = (unsigned char)c;
					} else {
						code = c;

						if (code >= slot) {
							code = oc;
							*sp++ = (unsigned char)fc;
						}

						IfegMemset(done_prefix, 0, sizeof(unsigned char)*(MAX_CODES+1));
						while (code >= newcodes) {
							*sp++ = suffix[code];
							if ((code == prefix[code]) || (done_prefix[code] == 1))
							{
								thumb_err("Circular entry in table.");
								if (decoderline != 0) {
									IfegMemFree(decoderline);
									decoderline = 0;
								}
								if (pDecBuf != 0) {
									IfegMemFree(pDecBuf);
									pDecBuf = 0;
								}
								IfegMemFree(done_prefix);
								done_prefix = 0;
								return 0;
							}
							done_prefix[code] = 1;
							code = prefix[code];
						}

						*sp++ = (unsigned char)code;
						spCount++;
						if (slot < top_slot) {
							fc = code;
							suffix[slot] = (unsigned char)fc;
							prefix[slot++] = oc;
							oc = c;
						}
						if (slot >= top_slot) {
							if (curr_size < 12) {
								top_slot <<= 1;
								++curr_size;
							}
						}
					}
				}
				/*************************************************************/

				if (bCenterAlign) {
					margin_wdt1_2 = (expected_width - resized_width)>>1;
					margin_hgt1_2 = (expected_height - resized_height)>>1;
				} else {
					margin_wdt1_2 = 0;
					margin_hgt1_2 = 0;
				}

				pImage32 = (unsigned int *)(pDecBuf);

				/* Only make a first image frame as a thumbnail */
				image_left_pos = 0; 
				image_top_pos = 0;
				len = decwdt;

				if (orgwdt > logi_wdt) {
					decwdt = resized_width;
				}
				if (orghgt > logi_hgt) {
					dechgt = resized_height;
				}

				if ((image_left_pos + decwdt) > resized_width) {
					decwdt = resized_width - image_left_pos;
					if (decwdt < 0) {
						decwdt = 0;
					}
				}
				if ((image_top_pos+dechgt) > resized_height) {
					dechgt = resized_height - image_top_pos;
					if (dechgt < 0) {
						dechgt = 0;
					}
				}

				if (pFrameData->imgCount == 0) {
					for (i = 0, k = margin_hgt1_2; i < resized_height; i++) {
						pImage16 = (unsigned short *)((unsigned char *)pOutBits + ((margin_wdt1_2 + expected_width * k) << 1));
						for (j = 0; j < resized_width; j++) {
							*pImage16++ = backcolor;
						}
						k++;
					}
					if (transparent == 1) {
					for (i = 0, k = image_top_pos+margin_hgt1_2; i < dechgt; i++) {
						pImage16 = (unsigned short *)((unsigned char *)pOutBits + ((image_left_pos+margin_wdt1_2 + expected_width * k) << 1));
						for (j = 0; j < decwdt; j++) {
							*pImage16++ = ui_backcolor;					
						}
						k++; 
					}
					}
				}
				
				for (i = 0, k = image_top_pos+margin_hgt1_2; i < dechgt; i++) {
					pImage16 = (unsigned short *)((unsigned char*)pOutBits + ((image_left_pos+margin_wdt1_2 + (expected_width) * k) << 1));
					pImage32 = (unsigned int *)(pDecBuf) + (i * len);

					for (j = 0; j < decwdt; j++) {
						if ((*pImage32>>24) == 0) {
							*(pImage16++) = ((*pImage32 & 0xf80000) >> 8) | ((*pImage32 & 0xfc00) >> 5) | ((*pImage32 & 0xf8) >> 3);
						} else if ((*pImage32 >> 24) == 4) {
							pImage16++;
						} else if ((*pImage32 >> 24) == 3) {
							*pImage16 =
								(((*pImage32 & 0xf80000) >> 8)+(((((*pImage16) & 0xf800) * 3) >> 2) & 0xf800)) |
								(((*pImage32 & 0xfc00) >> 5)+(((((*pImage16) & 0x07c0) * 3) >> 2) & 0x07c0)) |
								(((*pImage32 & 0xf8) >> 3)+((((*pImage16) & 0x001f) * 3) >> 2));
							pImage16++;
						} else if ((*pImage32 >> 24) == 1) {
							*pImage16 =
								(((*pImage32 & 0xf80000) >> 8)+((((*pImage16) & 0xf800) >> 2) & 0xf800)) |
								(((*pImage32 & 0xfc00) >> 5)+((((*pImage16) & 0x07c0) >> 2) & 0x07c0)) |
								(((*pImage32 & 0xf8) >> 3)+((((*pImage16) & 0x001f) >> 2)));
							pImage16++;
						} else {
							*pImage16 =
								(((*pImage32 & 0xf80000) >> 8)+((((*pImage16) & 0xf800) >> 1) & 0xf800)) |
								(((*pImage32 & 0xfc00) >> 5)+((((*pImage16) & 0x07c0) >> 1) & 0x07c0)) |
								(((*pImage32 & 0xf8) >> 3)+((((*pImage16) & 0x001f) >> 1)));
							pImage16++;
						}
						pImage32++;
					}
					k++;
				}

				switch (disposal_method) {
					/* No disposal specified. The decoder is not required to take any action */
				case 0:

					/* Do not dispose. The graphic is to be left in place. */
				case 1:
					IfegMemcpy(pFrameData->pPrevImg, pOutBits, expected_width * expected_height * 2);
					break;

					/* Restore to background color. The area used by the graphic must be restored to the background color. */
				case 2:
					IfegMemcpy(pFrameData->pPrevImg, pOutBits, expected_width * expected_height * 2);

					if (transparent == 1) {
					for (i = 0, k = image_top_pos+margin_hgt1_2; i < dechgt; i++) {
						pImage16 = (unsigned short *)((pFrameData->pPrevImg) + ((image_left_pos+margin_wdt1_2 + expected_width * k) << 1));
						for (j = 0; j < decwdt; j++) {
							*pImage16++ = ui_backcolor;					
						}
						k++; 
					}
					} else {
					for (i = 0, k = image_top_pos+margin_hgt1_2; i < dechgt; i++) {
						pImage16 = (unsigned short *)((pFrameData->pPrevImg) + ((image_left_pos+margin_wdt1_2 + expected_width * k) << 1));
						for (j = 0; j < decwdt; j++) {
							*pImage16++ = image_backcolor;					
						}
						k++; 
					}
					}
					break;

					/* Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic.*/
				case 3:
					break;

				default:
					IfegMemcpy(pFrameData->pPrevImg, pOutBits, expected_width * expected_height * 2);
					/* same with case 1 */
					break;
				}

				if (decoderline) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}
				IfegMemFree(done_prefix);
				done_prefix = 0;

				pFrameData->offset = inputPos;
				pFrameData->imgCount++;

				return 1;

				break;

				default:
					break;

		}
	}
}


/* use buffer */
int __FastImgGetNextFrameAGIF_UseBuffer(AGifFrameInfo *pFrameData, BOOL bCenterAlign)
{
	unsigned int *pImage32, *pImage32_2, *backGround;
	unsigned short *pImage16;
	unsigned char	*sp;
	unsigned char	*bufptr;
	unsigned int	code, fc, oc, bufcnt;
	unsigned char			buffer[16];
	unsigned char			val1;
	unsigned char			*buf;
	unsigned int			*pDacbox;
	unsigned char			*decoderline = 0;
	unsigned int size;
	unsigned int c = 0;

	unsigned int clear;
	unsigned int ending;
	unsigned int newcodes;
	unsigned int top_slot;
	unsigned int slot;

	int	numcolors;
	int	i, j, k;
	int	rowcount;
	int orgwdt_1, orgwdt, orghgt;
	int decwdt, dechgt;
	int len;
	int inter_step, interLaced, intercount;
	int transparent = 0;
	int startloc = 0;
	int backgroundIndex = 0;
	int transIndex = 0;
	int spCount = 0;
	int logi_wdt;		/* logical screen width */
	int logi_hgt;
	int ui_backcolor565;
	int backcolor565;

	int backcolor888;

	int image_left_pos;	/* left position of image in Logical screeen */
	int image_top_pos;
	int disposal_method = 0;
	int margin_wdt1_2, margin_hgt1_2;

	int inputPos;


	unsigned int dacbox[256];

	/* Variable for Resize */

	unsigned int a_x, a_y;
	unsigned int b_x, b_y;
	unsigned int c_x, c_y;
	unsigned int d_x, d_y;
	int out_x = 0, out_y = 0;
	int d1, d2;
	int flag = 0;
	unsigned int end;

	unsigned char *pDecBuf = 0;

	int expected_width = pFrameData->width;
	int expected_height = pFrameData->height;
	int resized_width;
	int resized_height;

	/* macro */
	register unsigned int curr_size;
	register int navail_bytes = 0;
	register unsigned int nbits_left = 0;
	register unsigned int b1 = 0;
	register unsigned int ret;

	/* parameter */
	unsigned int *global_dacbox = pFrameData->pGlobal_table;
	unsigned char *pInputStream = pFrameData->pEncodedData;
	void *pOutBits = pFrameData->pOutBits;
	unsigned short *prefix = pFrameData->pPrefix;
	unsigned char *dstack = pFrameData->pDstack;
	unsigned char *suffix = pFrameData->pSuffix;
	int filesize = pFrameData->inputSize;

	inputPos = pFrameData->offset;

	IfegMemset(prefix, 0, sizeof(unsigned short)*(MAX_CODES+1));
	IfegMemset(dstack, 0, sizeof(unsigned char)*(MAX_CODES+1));
	IfegMemset(suffix, 0, sizeof(unsigned char)*(MAX_CODES+1));

	ui_backcolor565 = pFrameData->ui_backcolor;

	backcolor565 = pFrameData->backcolor;
	backcolor888 =
		((backcolor565&0xf800) << 6)|
		((backcolor565&0x7e0) << 3)|
		((backcolor565&0x1f) << 1);

	backGround = (unsigned int *)pFrameData->pPrevImg;

	intercount = 0;

	if (pFrameData->imgCount == -1) {
		/* Input stream index set to 0 */
		inputPos = 0;
		inter_step = 1;
		interLaced = 0; 
		transparent = 0;

		/* read a GIF HEADER */
		IfegMemcpy(buffer, pInputStream, 13);
		inputPos += 13;

		/* wheather GIF file or not */
		if (buffer[0] != 'G' || buffer[1] != 'I' || buffer[2] != 'F' ||
			buffer[3] < '0' || buffer[3] > '9' ||
			buffer[4] < '0' || buffer[4] > '9' ||
			buffer[5] < 'A' || buffer[5] > 'z') {
			if (pDecBuf != 0) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			return -1;
		}

		/* get Logical width, height */
		pFrameData->logi_wdt = logi_wdt = buffer[6] | (buffer[7] << 8);
		pFrameData->logi_hgt = logi_hgt = buffer[8] | (buffer[9] << 8);

		/* ouput resized image size */
		if (logi_wdt <= expected_width && logi_hgt <= expected_height) {
			resized_width = logi_wdt;
			resized_height = logi_hgt;
			pFrameData->flag = flag = 1;
		} else {
			if ((logi_wdt/(float)expected_width) >= (logi_hgt/(float)expected_height)) {
				resized_height = logi_hgt * expected_width / logi_wdt;
				resized_width = expected_width;
			} else {
				resized_width = logi_wdt * expected_height / logi_hgt;
				resized_height = expected_height;
			}
		}
		if (!resized_width || !resized_height) {
			if (decoderline != 0) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf != 0) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			return 0;
		}

		/* ouput resized image size */
		pFrameData->resizedwidth = resized_width;
		pFrameData->resizedheight = resized_height;

		/* Color Resolution */
		IfegMemset(global_dacbox, 0, 1024);
		numcolors = (buffer[10] & 0x7) + 1;
		if ((buffer[10] & 0x80) == 0) {  /* Global color table */
			global_dacbox[0] = 0x000000;
			global_dacbox[1] = 0x3f3f3f;
			numcolors = 2;
		} else { /* Global color table */
			numcolors = 1 << numcolors;

			/* Make color table */
			for (i = 0; i < numcolors; i++) {
				global_dacbox[i] = ((pInputStream[inputPos++] >> 2) << 16);
				global_dacbox[i] |= ((pInputStream[inputPos++] >> 2) << 8);
				global_dacbox[i] |= (pInputStream[inputPos++] >> 2);
			}
		}

		backgroundIndex = buffer[11];
		/* Background Color */
		backcolor888 = global_dacbox[backgroundIndex];
		backcolor565 = pFrameData->backcolor = (((backcolor888 & 0xff0000) >> 17) << 11) | (((backcolor888 & 0x00ff00) >> 8) << 5) | (((backcolor888 & 0xff) >> 1));

		pImage16 = pOutBits;

		ui_backcolor565 = pFrameData->ui_backcolor ;
		for (i = 0; i < expected_width * expected_height; i++) {
			*pImage16++ = ui_backcolor565;
		}

		pImage32 = (unsigned int *)pFrameData->pPrevImg;

		if (numcolors > 16)	{
			numcolors = 256;
		}
		if (numcolors > 2 && numcolors < 16) {
			numcolors = 16;
		}
		pFrameData->global_numcol = numcolors;
		pFrameData->imgCount++;
		pFrameData->firstpos = inputPos;
	} else {
		logi_wdt = pFrameData->logi_wdt;
		logi_hgt = pFrameData->logi_hgt;
		resized_width = pFrameData->resizedwidth;
		resized_height = pFrameData->resizedheight;
		flag = pFrameData->flag ;

		numcolors = pFrameData->global_numcol;
	}

	/* still gif image (image_cnt = 1) */
	while (1) {
		if (inputPos > filesize) {

			if (decoderline) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			pFrameData->imgCount = 0;
			pFrameData->offset = pFrameData->firstpos;

			if (pFrameData->bLoop) {
				return 1;
			} else {
				return 0;
			}
		}

		switch (pInputStream[inputPos++]) {
		case 0x3b:/* End of the GIF dataset */
			if (decoderline) {
				IfegMemFree(decoderline);
				decoderline = 0;
			}
			if (pDecBuf) {
				IfegMemFree(pDecBuf);
				pDecBuf = 0;
			}
			if (pFrameData->imgCount == 0) {
				return -1;
			}
			pFrameData->imgCount = 0;
			pFrameData->offset = pFrameData->firstpos;
#ifdef INTERNAL_IMGCODEC
			pFrameData->nRepeatCount++;

			if (pFrameData->nLoopCount == -1) {
				break;
			} else if (pFrameData->bLoop || (pFrameData->nRepeatCount <= pFrameData->nLoopCount) || (pFrameData->nLoopCount == 0)) {
				inputPos = pFrameData->offset;
				continue;
			} else {
				return 2;
			}
#else
			if (pFrameData->bLoop) {
				return 1;
			} else {
				return 0;
			}

#endif
			break;
		case 0x21: /* Extension Block */
			switch (pInputStream[inputPos++]) {
			case 0xf9: /* Graphic control extension block */
				if (4 != pInputStream[inputPos++]) { /* data length : fixed 4 bytes */
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return -1;
				}

				disposal_method = ((pInputStream[inputPos] & 0x1c) >> 2);
				transparent = pInputStream[inputPos++] & 0x01;	/* does it use? 1:on 0:off */
				pFrameData->delay = (pInputStream[inputPos] | (pInputStream[inputPos+1] << 8))*10;
				inputPos += 2; /* Delay time (skip) */
				transIndex = pInputStream[inputPos++];
				inputPos++; /* block end */
				
				if (backgroundIndex == transIndex && transparent == 1) {
					backcolor888 = 0x1000000;
					backcolor565 = pFrameData->ui_backcolor;
				}

				break;
			case 0x01: /* Plain Text block */
				while ((i = pInputStream[inputPos++]) > 0) {	/* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}
						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						return -1;
					}
				}
				break;
			case 0xfe: /* Comment Extension block */
				while ((i = pInputStream[inputPos++]) > 0) {	/* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}
						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						return -1;
					}
				}
				break;
			case 0xff: /* Appliation Extension block */

				while ((i = pInputStream[inputPos++]) > 0) {	/* get the data length */
					inputPos += i;
					if (inputPos > filesize) {
						if (decoderline) {
							IfegMemFree(decoderline);
							decoderline = 0;
						}
						if (pDecBuf) {
							IfegMemFree(pDecBuf);
							pDecBuf = 0;
						}
						return -1;
					}
				}
				break;
			default:
				break;
			}

			break;

			case 0x2c: /* Start of an image object. Read the image description. */

				/* initialize */
				pDacbox = global_dacbox;

				IfegMemcpy(buffer, pInputStream+inputPos, 9);
				inputPos += 9;

				if (pFrameData->imgCount == 0) {
					image_left_pos_N = (buffer[0] | (buffer[1] << 8)); 
					image_top_pos_N = (buffer[2] | (buffer[3] << 8));	

					image_left_pos = image_left_pos_N;
					image_top_pos = image_top_pos_N;
				}
				image_left_pos = (buffer[0] | (buffer[1] << 8));
				image_top_pos = (buffer[2] | (buffer[3] << 8));
				orgwdt_1 = orgwdt = (buffer[4] | (buffer[5] << 8));
				orghgt = (buffer[6] | (buffer[7] << 8));


#ifdef INTERNAL_IMGCODEC
#else
				if ((orgwdt > MAXWIDTH) || (orghgt > MAXHEIGHT)) {
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return -1;
				}
#endif

				/* Initialize */
				a_x = logi_wdt >> 2, a_y = logi_hgt >> 2;
				b_x = ((logi_wdt*3) >> 2), b_y = logi_hgt >> 2;
				c_x = logi_wdt >> 2, c_y = ((logi_hgt*3) >> 2);
				d_x = ((logi_wdt*3) >> 2), d_y = ((logi_hgt*3) >> 2);

				/* Interlaced check */
				interLaced = buffer[8] & 0x40;
				if (interLaced) {
					startloc = -8+image_top_pos;
				}

				inter_step = 1;

				if (decoderline) {
					IfegMemFree(decoderline);
				}
				decoderline = (unsigned char *)IfegMemAlloc(orgwdt);
				if (!decoderline) {
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return 0;
				}
				IfegMemset(decoderline, 0, orgwdt);

				decwdt = resized_width;
				dechgt = resized_height;

				if (!decwdt || !dechgt) {
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return 0;
				}

				if (pDecBuf) {
					IfegMemFree(pDecBuf);
				}
				pDecBuf = (unsigned char *)IfegMemAlloc(decwdt * dechgt * 4);
				if (!pDecBuf) {
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return 0;
				}

				IfegMemset(pDecBuf, 0, decwdt * dechgt * 4);

				/* assign out_888_image plane */
				pImage32 = (unsigned int *)(pDecBuf);

				end = dechgt * logi_hgt;
				out_x = 0, out_y = -dechgt;

				/* Color Resolution */
				if ((buffer[8] & 0x80) == 0) { /* Logical color table  */
					;/* use global table */
				} else { /* Logical color table */
					IfegMemset(dacbox, 0, 1024);

					numcolors = (buffer[8] & 0x7) + 1;
					numcolors = 1 << numcolors;

					/* Make color table */
					for (i = 0; i < numcolors; i++) {
						dacbox[i] = ((pInputStream[inputPos++] >> 2) << 16);
						dacbox[i] |= ((pInputStream[inputPos++] >> 2) << 8);
						dacbox[i] |= (pInputStream[inputPos++] >> 2);
					}
					pDacbox = dacbox;
				}

				if (numcolors > 16) {
					numcolors = 256;
				}
				if (numcolors > 2 && numcolors < 16) {
					numcolors = 16;
				}

				if (logi_wdt < (image_left_pos + orgwdt)) {
					if (image_left_pos > logi_wdt) {
						image_left_pos = logi_wdt;
					}
					orgwdt = logi_wdt - image_left_pos;
					flag = 0;
				}
				if (logi_hgt < (image_top_pos + orghgt)) {
					flag = 0;
				}

				if (pFrameData->imgCount == 0) {
					if (disposal_method == 3) {
						pImage32 = (unsigned int *)pFrameData->pPrevImg;
						for (i = 0; i < logi_wdt * logi_hgt; i++) {
							*pImage32++ = 0x1000000;
						}	
					} else {
						pImage32 = (unsigned int *)pFrameData->pPrevImg;
						for (i = 0; i < logi_wdt * logi_hgt; i++) {
							*pImage32++ = backcolor888;
						}	
					}
					if (transparent == 1) {
						/* Background Color */
						pImage32 = (unsigned int *)pFrameData->pPrevImg;
						pImage32 += image_top_pos * logi_wdt + image_left_pos;
						if (logi_hgt < (image_top_pos + orghgt)) {
							for (i = 0; i < logi_hgt - image_top_pos; i++) {
								for (j = 0; j < orgwdt; j++) {
									pImage32[j] = 0x1000000;  /* set ui color */
								}
								pImage32 += logi_wdt;
							}
						} else {
							for (i = 0; i < orghgt; i++) {
								for (j = 0; j < orgwdt; j++) {
									pImage32[j] = 0x1000000;  /* set ui color */
								}
								pImage32 += logi_wdt;
							}
						}
					}
				}

				/* assign out_888_image plane */
				pImage32 = (unsigned int *)(pDecBuf);

				for (rowcount = 0; rowcount < image_top_pos; rowcount++) {
					if (flag) {
						for (i = 0; i < logi_wdt; i++) {
							*pImage32++ = backGround[i] << 2;
						}
					} else {
						out_x = 0, out_y += dechgt;
						if (c_y < end) {
							d1 = a_y - out_y;
							d2 = c_y - out_y;
							if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
								pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
								pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = a_x - out_x;
									d2 = b_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32 + (a_x/logi_wdt)) += backGround[i];
										*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
										a_x += logi_wdt, c_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32 + (b_x/logi_wdt)) += backGround[i];
										*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
										b_x += logi_wdt, d_x += logi_wdt;
									}
									out_x += decwdt;
								}
								a_x = logi_wdt >> 2, a_y += logi_hgt;
								b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
								c_x = logi_wdt >> 2, c_y += logi_hgt;
								d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
							} else if (0 <= d1 && d1 < dechgt) {
								pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = a_x - out_x;
									d2 = b_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32 + (a_x/logi_wdt)) += backGround[i];
										a_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32 + (b_x/logi_wdt)) += backGround[i];
										b_x += logi_wdt;
									}
									out_x += decwdt;
								}
								a_x = logi_wdt >> 2, a_y += logi_hgt;
								b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
							} else if (0 <= d2 && d2 < dechgt) {
								pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = c_x - out_x;
									d2 = d_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
										c_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
										d_x += logi_wdt;
									}
									out_x += decwdt;
								}
								c_x = logi_wdt >> 2, c_y += logi_hgt;
								d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
							}
						}
					}
					backGround += logi_wdt;
				}


				/***************************************************************************
				decoder(WDT, pInputStream, pBitmapElem->pBits);	//this does the grunt work		
				int decoder(int linewidth, UCHAR* pInputStream, UCHAR *pBitmapElem->pBits)			
				***************************************************************************/

				size = pInputStream[inputPos++];
				if (size < 2 || 9 < size) {
					if (decoderline != 0) {
						IfegMemFree(decoderline);
						decoderline = 0;
					}
					if (pDecBuf != 0) {
						IfegMemFree(pDecBuf);
						pDecBuf = 0;
					}
					return -1;
				}
				rowcount = oc = fc = 0;
				buf = decoderline;
				sp = dstack;
				bufptr = buf;
				bufcnt = orgwdt_1;

				/************************
				init_exp(size);		
				int init_exp(int size)
				************************/
				curr_size = size + 1;
				top_slot = 1 << curr_size;
				clear = 1 << size;
				ending = clear + 1;
				slot = newcodes = ending + 1;
				navail_bytes = nbits_left = 0;
				/************************/


				/* __get_next_code(pInputStream) */
				if (navail_bytes < 2) {
					if (nbits_left == 0)
						__get_next_code_first_nbits_left_0(pInputStream)
					else
						__get_next_code_first_nbits_left_not_0(pInputStream)
					__get_next_code_first_while(pInputStream)
				} else {
					if (nbits_left == 0) 
						__get_next_code_second_nbits_left_0(pInputStream)
					else
						__get_next_code_second_nbits_left_not_0(pInputStream)

					__get_next_code_second_while(pInputStream)
				}
				if (c == ending) {
					break;
				}

				if (c == clear) {
					curr_size = size + 1;
					slot = newcodes;
					top_slot = 1 << curr_size;

					do {
						/* __get_next_code(pInputStream); */
						if (navail_bytes < 2) {
							if (nbits_left == 0)
								__get_next_code_first_nbits_left_0(pInputStream)
							else
								__get_next_code_first_nbits_left_not_0(pInputStream)
							__get_next_code_first_while(pInputStream)
						} else {
							if (nbits_left == 0) 
								__get_next_code_second_nbits_left_0(pInputStream)
							else
								__get_next_code_second_nbits_left_not_0(pInputStream)

							__get_next_code_second_while(pInputStream)
						}		
					} while (c == clear);

					if (c == ending) {
						break;
					}

					if (c >= slot) {
						c = 0;
					}

					oc = fc = c;
					*sp++ = (unsigned char)c;
				} else {
					if (c >= slot) {
						c = 0;
					}

					oc = fc = c;
					*sp++ = (unsigned char)c;
				}

				switch (disposal_method) {

					/* Restore to previous. The decoder is required to restore the area overwritten by the graphic with what was there prior to rendering the graphic. */
				case 3:
					while (rowcount < orghgt) {
						if ((sp - dstack) > 0) {
							spCount = sp - dstack;
						}
						/* Now that we've pushed the decoded string (in reverse order)
						* onto the stack, lets pop it off and put it into our decode
						* buffer...  And when the decode buffer is full, write another
						* line...
						*/
						while (sp > dstack) {
							--sp;
							*bufptr++ = *sp;

							if (--bufcnt == 0) {
								/**********************************************************************************
								if ((ret = put_line(rowcount++, bufptr - buf, WDT, buf, pBitmapElem->pBits)) < 0)
								**********************************************************************************/
								rowcount++;
								len = bufptr - buf;
								if (len >= orgwdt) {
									len = orgwdt;
								}

								if (interLaced == 0) { /* interlaced image */
									if (!flag) {
										out_x = 0, out_y += dechgt;
									}
								} else { /* interlaced image */
									if (inter_step == 1) {
										startloc += 8;
										intercount++;
									} else if (inter_step == 2) {
										startloc += 8;
										intercount++;
									} else if (inter_step == 3) {
										startloc += 4;
										intercount++;
									} else if (inter_step == 4) {
										startloc += 2;
										intercount++;
									}

									if (startloc >= orghgt+image_top_pos) {
										inter_step++;
										if (inter_step == 2) {
											startloc = 4+image_top_pos;
										} else if (inter_step == 3) {
											startloc = 2+image_top_pos;
										} else if (inter_step == 4) {
											startloc = 1+image_top_pos;
										}
									}

									backGround = (unsigned int *)(pFrameData->pPrevImg + ((startloc * logi_wdt) << 2));
									/* gif to rgb 565 */
									if (flag) {
										pImage32 = (unsigned int *)(pDecBuf) + startloc * decwdt;
									} else {
										out_x = 0, out_y = startloc * dechgt;
										a_x = logi_wdt >> 2;
										b_x = ((logi_wdt*3) >> 2);
										c_x = logi_wdt >> 2;
										d_x = ((logi_wdt*3) >> 2);
										if (out_y <= (logi_hgt >> 2)) {
											a_y = logi_hgt >> 2;
											b_y = logi_hgt >> 2;
											c_y = ((logi_hgt*3) >> 2);
											d_y = ((logi_hgt*3) >> 2);
										} else {
											if (((out_y % logi_hgt) - (logi_hgt >> 2)) > 0) {
												a_y = ((out_y/logi_hgt)+1) * logi_hgt + (logi_hgt >> 2);
											} else if (((out_y % logi_hgt) - (logi_hgt >> 2)) < 0 || !(out_y%logi_hgt)) {
												a_y = ((out_y/logi_hgt)) * logi_hgt + (logi_hgt>>2);
											} else {
												a_y = out_y;
											}

											if (((out_y % logi_hgt) - ((logi_hgt*3) >> 2)) > 0) {
												c_y = ((out_y/logi_hgt+1) * logi_hgt + ((logi_hgt*3) >> 2));
											} else if (((out_y % logi_hgt) - ((logi_hgt*3) >> 2)) < 0 || !(out_y % logi_hgt)) {
												c_y = ((out_y/logi_hgt)) * logi_hgt + ((logi_hgt*3) >> 2);
											} else {
												c_y = out_y;
											}
											b_y = a_y, d_y = c_y;
										}
									}
								}

								if (transparent == 1) {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i]<<2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors-1);

											if (val1 == transIndex) {
												*pImage32++ = backGround[i+image_left_pos] << 2;
											} else {
												*pImage32++ = pDacbox[val1] << 2;
											}
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;

												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt, c_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt, c_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt, d_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt, d_x += logi_wdt;
														}
													}
													out_x += decwdt;
												}

												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt>>2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt;
														}
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															c_x += logi_wdt;
														} else {
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															c_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															d_x += logi_wdt;
														} else {
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															d_x += logi_wdt;
														}
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								} else {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i] << 2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors - 1);
											*pImage32++ = pDacbox[val1] << 2;
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;

												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								}

								backGround += logi_wdt;
								bufptr = buf;
								bufcnt = orgwdt_1;
							}
						}

						if (rowcount == orghgt) {
							break;
						}
						/* __get_next_code(pInputStream) */
						if (navail_bytes < 2) {
							if (nbits_left == 0)
								__get_next_code_first_nbits_left_0(pInputStream)
							else
								__get_next_code_first_nbits_left_not_0(pInputStream)
							__get_next_code_first_while(pInputStream)
						} else {
							if (nbits_left == 0) 
								__get_next_code_second_nbits_left_0(pInputStream)
							else
								__get_next_code_second_nbits_left_not_0(pInputStream)

							__get_next_code_second_while(pInputStream)
						}		

							if (c == ending) {
								break;
							}

							if (c == clear) {
								curr_size = size + 1;
								slot = newcodes;
								top_slot = 1 << curr_size;

								do {
									/* __get_next_code(pInputStream); */
									if (navail_bytes < 2) {
										if (nbits_left == 0)
											__get_next_code_first_nbits_left_0(pInputStream)
										else
											__get_next_code_first_nbits_left_not_0(pInputStream)
										__get_next_code_first_while(pInputStream)
									} else {
										if (nbits_left == 0) 
											__get_next_code_second_nbits_left_0(pInputStream)
										else
											__get_next_code_second_nbits_left_not_0(pInputStream)

										__get_next_code_second_while(pInputStream)
									}		
								} while (c == clear);

								if (c == ending) {
									break;
								}

								if (c >= slot) {
									c = 0;
								}

								oc = fc = c;

								*sp++ = (unsigned char)c;
							} else {
								code = c;

								if (code >= slot) {
									code = oc;
									*sp++ = (unsigned char)fc;
								}

								while (code >= newcodes) {
									*sp++ = suffix[code];
									code = prefix[code];
								}

								*sp++ = (unsigned char)code;
								spCount++;
								if (slot < top_slot) {
									fc = code;
									suffix[slot] = (unsigned char)fc;
									prefix[slot++] = oc;
									oc = c;
								}
								if (slot >= top_slot) {
									if (curr_size < 12) {
										top_slot <<= 1;
										++curr_size;
									}
								}
							}
					}
					break;

					/* Restore to background color. The area used by the graphic must be restored to the background color. */
				case 2:
					while (rowcount < orghgt) {
						if ((sp - dstack) > 0) {
							spCount = sp - dstack;
						}
						while (sp > dstack) {
							--sp;
							*bufptr++ = *sp;

							if (--bufcnt == 0) {
								/********************************************************************************
								if ((ret = put_line(rowcount++, bufptr - buf, WDT, buf, pBitmapElem->pBits)) < 0)
								********************************************************************************/
								rowcount++;
								len = bufptr - buf;
								if (len >= orgwdt) {
									len = orgwdt;
								}

								if (interLaced == 0) { /* interlaced image */
									if (!flag) {
										out_x = 0, out_y += dechgt;
									}
								} else { /* interlaced image */
									if (inter_step == 1) {
										startloc += 8;
									} else if (inter_step == 2) {
										startloc += 8;
									} else if (inter_step == 3) {
										startloc += 4;
									} else if (inter_step == 4) {
										startloc += 2;
									}

									if (startloc >= orghgt + image_top_pos) {
										inter_step++;
										if (inter_step == 2) {
											startloc = 4 + image_top_pos;
										} else if (inter_step == 3) {
											startloc = 2+image_top_pos;
										} else if (inter_step == 4) {
											startloc = 1+image_top_pos;
										}
									}
									backGround = (unsigned int *)(pFrameData->pPrevImg + ((startloc * logi_wdt) << 2));
									/* gif to rgb 565 */
									if (flag) {
										pImage32 = (unsigned int *)(pDecBuf) + startloc * decwdt;
									} else {
										out_x = 0, out_y = startloc * dechgt;
										a_x = logi_wdt >> 2;
										b_x = ((logi_wdt*3) >> 2);
										c_x = logi_wdt>>2;
										d_x = ((logi_wdt*3) >> 2);
										if (out_y <= (logi_hgt >> 2)) {
											a_y = logi_hgt>>2;
											b_y = logi_hgt>>2;
											c_y = ((logi_hgt*3)>>2);
											d_y = ((logi_hgt*3)>>2);
										} else {
											if (((out_y % logi_hgt) - (logi_hgt >> 2)) > 0) {
												a_y = ((out_y/logi_hgt)+1) * logi_hgt + (logi_hgt >> 2);
											} else if (((out_y%logi_hgt) - (logi_hgt >> 2)) < 0 || !(out_y%logi_hgt)) {
												a_y = ((out_y/logi_hgt)) * logi_hgt + (logi_hgt >> 2);
											} else {
												a_y = out_y;
											}

											if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) > 0) {
												c_y = ((out_y/logi_hgt+1) * logi_hgt + ((logi_hgt*3) >> 2));
											} else if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) < 0 || !(out_y%logi_hgt)) {
												c_y = ((out_y/logi_hgt)) * logi_hgt + ((logi_hgt*3) >> 2);
											} else {
												c_y = out_y;
											}
											b_y = a_y, d_y = c_y;
										}
									}
								}

								if (transparent == 1) {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i] << 2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors-1);

											if (val1 == transIndex) {
												*pImage32++ = backGround[i+image_left_pos] << 2; 
												backGround[i+image_left_pos] = 0x1000000;
											} else {
												*pImage32++ = pDacbox[val1] << 2;
												backGround[i+image_left_pos] = 0x1000000;
											}
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;

												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt, c_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt, c_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt, d_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt, d_x += logi_wdt;
														}
													}
													backGround[i+image_left_pos] = 0x1000000;
													out_x += decwdt;
												}

												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt;
														}
													}
													backGround[i+image_left_pos] = 0x1000000;
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {									
												d1 = a_x - out_x;
												d2 = b_x - out_x;
												if (0 <= d1 && d1 < decwdt) {
													*(pImage32 + (a_x/logi_wdt)) += backGround[i];
													a_x += logi_wdt;
												}
												if (0 <= d2  && d2 < decwdt) {
													*(pImage32 + (b_x/logi_wdt)) += backGround[i];
													b_x += logi_wdt;
												}
												out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															c_x += logi_wdt;
														} else {
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															c_x += logi_wdt;
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															d_x += logi_wdt;
														} else {
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															d_x += logi_wdt;
														}
													}
													backGround[i+image_left_pos] = 0x1000000;
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								} else {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i]<<2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors-1);
											*pImage32++ = pDacbox[val1] << 2;
											backGround[i+image_left_pos] = backcolor888;
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt, c_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt, d_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;

												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														c_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														d_x += logi_wdt;
														backGround[i+image_left_pos] = backcolor888;
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								}

								backGround += logi_wdt;
								bufptr = buf;
								bufcnt = orgwdt_1;
							}
						}

						if (rowcount == orghgt) {
							break;
						}
						/* __get_next_code(pInputStream) */
						if (navail_bytes < 2) {
							if (nbits_left == 0)
								__get_next_code_first_nbits_left_0(pInputStream)
							else
								__get_next_code_first_nbits_left_not_0(pInputStream)
							__get_next_code_first_while(pInputStream)
						} else {
							if (nbits_left == 0) 
								__get_next_code_second_nbits_left_0(pInputStream)
							else
								__get_next_code_second_nbits_left_not_0(pInputStream)

							__get_next_code_second_while(pInputStream)
						}		
							if (c == ending) {
								break;
							}

							if (c == clear) {
								curr_size = size + 1;
								slot = newcodes;
								top_slot = 1 << curr_size;

								do {
									/* __get_next_code(pInputStream); */
									if (navail_bytes < 2) {
										if (nbits_left == 0)
											__get_next_code_first_nbits_left_0(pInputStream)
										else
											__get_next_code_first_nbits_left_not_0(pInputStream)
										__get_next_code_first_while(pInputStream)
									} else {
										if (nbits_left == 0)
											__get_next_code_second_nbits_left_0(pInputStream)
										else
											__get_next_code_second_nbits_left_not_0(pInputStream)

										__get_next_code_second_while(pInputStream)
									}		
								} while (c == clear);

								if (c == ending) {
									break;
								}

								if (c >= slot) {
									c = 0;
								}

								oc = fc = c;

								*sp++ = (unsigned char)c;
							} else {
								code = c;

								if (code >= slot) {
									code = oc;
									*sp++ = (unsigned char)fc;
								}

								while (code >= newcodes) {
									*sp++ = suffix[code];
									code = prefix[code];
								}

								*sp++ = (unsigned char)code;
								spCount++;
								if (slot < top_slot) {
									fc = code;
									suffix[slot] = (unsigned char)fc;
									prefix[slot++] = oc;
									oc = c;
								}
								if (slot >= top_slot) {
									if (curr_size < 12) {
										top_slot <<= 1;
										++curr_size;
									}
								}
							}

					}

					break;


					/* No disposal specified. The decoder is not required to take any action */
				case 0:

					/* Do not dispose. The graphic is to be left in place. */
				case 1:

				default:
					while (rowcount < orghgt) {
						if ((sp - dstack) > 0) {
							spCount = sp - dstack;
						}
						/* Now that we've pushed the decoded string (in reverse order)
						* onto the stack, lets pop it off and put it into our decode
						* buffer...  And when the decode buffer is full, write another
						* line...
						*/
						while (sp > dstack) {
							--sp;
							*bufptr++ = *sp;

							if (--bufcnt == 0) {
								/**********************************************************************************
								if ((ret = put_line(rowcount++, bufptr - buf, WDT, buf, pBitmapElem->pBits)) < 0)
								**********************************************************************************/
								rowcount++;
								len = bufptr - buf;
								if (len >= orgwdt) {
									len = orgwdt;
								}

								if (interLaced == 0) { /* interlaced image  */
									if (!flag) {
										out_x = 0, out_y += dechgt;
									}
								} else { /* interlaced image */
									if (inter_step == 1) {
										startloc += 8;
										intercount++;
									} else if (inter_step == 2) {
										startloc += 8;
										intercount++;
									} else if (inter_step == 3) {
										startloc += 4;
										intercount++;
									} else if (inter_step == 4) {
										startloc += 2;
										intercount++;
									}

									if (startloc >= orghgt+image_top_pos) {
										inter_step++;
										if (inter_step == 2) {
											startloc = 4+image_top_pos;
										} else if (inter_step == 3) {
											startloc = 2+image_top_pos;
										} else if (inter_step == 4) {
											startloc = 1+image_top_pos;
										}
									}

									backGround = (unsigned int *)(pFrameData->pPrevImg + ((startloc * logi_wdt) << 2));
									/* gif to rgb 565 */
									if (flag) {
										pImage32 = (unsigned int *)(pDecBuf) + startloc * decwdt;
									} else {
										out_x = 0, out_y = startloc * dechgt;
										a_x = logi_wdt >> 2;
										b_x = ((logi_wdt*3) >> 2);
										c_x = logi_wdt >> 2;
										d_x = ((logi_wdt*3) >> 2);
										if (out_y <= (logi_hgt >> 2)) {
											a_y = logi_hgt >> 2;
											b_y = logi_hgt >> 2;
											c_y = ((logi_hgt*3) >> 2);
											d_y = ((logi_hgt*3) >> 2);
										} else {
											if (((out_y%logi_hgt) - (logi_hgt >> 2)) > 0) {
												a_y = ((out_y/logi_hgt)+1) * logi_hgt + (logi_hgt >> 2);
											} else if (((out_y%logi_hgt) - (logi_hgt >> 2)) < 0 || !(out_y%logi_hgt)) {
												a_y = ((out_y/logi_hgt)) * logi_hgt + (logi_hgt >> 2);
											} else {
												a_y = out_y;
											}

											if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) > 0) {
												c_y = ((out_y/logi_hgt+1) * logi_hgt + ((logi_hgt*3) >> 2));
											} else if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) < 0 || !(out_y%logi_hgt)) {
												c_y = ((out_y/logi_hgt)) * logi_hgt + ((logi_hgt*3) >> 2);
											} else {
												c_y = out_y;
											}
											b_y = a_y, d_y = c_y;
										}
									}
								}

								if (transparent == 1) {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i] << 2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors-1);

											if (val1 == transIndex) {
												*pImage32++ = backGround[i+image_left_pos] << 2; /* Set *pImage32 MSB 1 */
											} else {
												*pImage32++ = pDacbox[val1] << 2;
												backGround[i+image_left_pos] = pDacbox[val1];
											}
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;

												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt, c_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt, c_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt, d_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt, d_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													out_x += decwdt;
												}

												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;
												}

												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (a_x/logi_wdt)) += backGround[i+image_left_pos];
															a_x += logi_wdt;
														} else {
															*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
															a_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32 + (b_x/logi_wdt)) += backGround[i+image_left_pos];
															b_x += logi_wdt;
														} else {
															*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
															b_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {			
												d1 = a_x - out_x;
												d2 = b_x - out_x;
												if (0 <= d1 && d1 < decwdt) {
													*(pImage32 + (a_x/logi_wdt)) += backGround[i];
													a_x += logi_wdt;
												}
												if (0 <= d2  && d2 < decwdt) {
													*(pImage32 + (b_x/logi_wdt)) += backGround[i];
													b_x += logi_wdt;
												}
												out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (c_x/logi_wdt)) += backGround[i+image_left_pos];
															c_x += logi_wdt;
														} else {
															*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
															c_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													if (0 <= d2  && d2 < decwdt) {
														if (val1 == transIndex) {
															*(pImage32_2 + (d_x/logi_wdt)) += backGround[i+image_left_pos];
															d_x += logi_wdt;
														} else {
															*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
															d_x += logi_wdt;
															backGround[i+image_left_pos] = pDacbox[val1];
														}
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								} else {
									if (flag) {
										for (i = 0; i < image_left_pos; i++) {
											*pImage32++ = backGround[i] << 2;
										}
										for (i = 0; i < len; i++) {
											val1 = buf[i] & (numcolors-1);
											*pImage32++ = pDacbox[val1] << 2;
											backGround[i+image_left_pos] = pDacbox[val1];
										}
										for (i = orgwdt+image_left_pos; i < logi_wdt; i++) {
											*pImage32++ = backGround[i] << 2;
										}
									} else {
										if (c_y < end) {
											d1 = a_y - out_y;
											d2 = c_y - out_y;
											if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt, c_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt, d_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt, c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt, d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											} else if (0 <= d1 && d1 < dechgt) {
												pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;

												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += pDacbox[val1];
														a_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += pDacbox[val1];
														b_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = a_x - out_x;
													d2 = b_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32 + (a_x/logi_wdt)) += backGround[i];
														a_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32 + (b_x/logi_wdt)) += backGround[i];
														b_x += logi_wdt;
													}
													out_x += decwdt;
												}
												if (!interLaced) {
													a_x = logi_wdt >> 2, a_y += logi_hgt;
													b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
												}
											} else if (0 <= d2 && d2 < dechgt) {
												pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
												for (i = 0; i < image_left_pos; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;
												}
												for (i = 0; i < orgwdt; i++) {
													val1 = buf[i] & (numcolors-1);

													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += pDacbox[val1];
														c_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += pDacbox[val1];
														d_x += logi_wdt;
														backGround[i+image_left_pos] = pDacbox[val1];
													}
													out_x += decwdt;
												}
												for (i = image_left_pos+orgwdt; i < logi_wdt; i++) {
													d1 = c_x - out_x;
													d2 = d_x - out_x;
													if (0 <= d1 && d1 < decwdt) {
														*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
														c_x += logi_wdt;
													}
													if (0 <= d2  && d2 < decwdt) {
														*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
														d_x += logi_wdt;
													}
													out_x += decwdt;

												}
												if (!interLaced) {
													c_x = logi_wdt >> 2, c_y += logi_hgt;
													d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
												}
											}
										}
									}
								}

								backGround += logi_wdt;
								bufptr = buf;
								bufcnt = orgwdt_1;
							}
						}
						if (rowcount == orghgt) {
							break;
						}
						/* __get_next_code(pInputStream) */
						if (navail_bytes < 2) {
							if (nbits_left == 0)
								__get_next_code_first_nbits_left_0(pInputStream)
							else
								__get_next_code_first_nbits_left_not_0(pInputStream)
							__get_next_code_first_while(pInputStream)
						} else {
							if (nbits_left == 0) 
								__get_next_code_second_nbits_left_0(pInputStream)
							else
								__get_next_code_second_nbits_left_not_0(pInputStream)

							__get_next_code_second_while(pInputStream)
						}		
							if (c == ending) {
								break;
							}

							if (c == clear) {
								curr_size = size + 1;
								slot = newcodes;
								top_slot = 1 << curr_size;

								do {
									/* __get_next_code(pInputStream); */
									if (navail_bytes < 2) {
										if (nbits_left == 0)
											__get_next_code_first_nbits_left_0(pInputStream)
										else
											__get_next_code_first_nbits_left_not_0(pInputStream)
										__get_next_code_first_while(pInputStream)
									} else {
										if (nbits_left == 0) 
											__get_next_code_second_nbits_left_0(pInputStream)
										else
											__get_next_code_second_nbits_left_not_0(pInputStream)

										__get_next_code_second_while(pInputStream)
									}		
								} while (c == clear);

								if (c == ending) {
									break;
								}

								if (c >= slot) {
									c = 0;
								}

								oc = fc = c;

								*sp++ = (unsigned char)c;
							} else {
								code = c;

								if (code >= slot) {
									code = oc;
									*sp++ = (unsigned char)fc;
								}

								while (code >= newcodes) {
									*sp++ = suffix[code];
									code = prefix[code];
								}

								*sp++ = (unsigned char)code;
								spCount++;
								if (slot < top_slot) {
									fc = code;
									suffix[slot] = (unsigned char)fc;
									prefix[slot++] = oc;
									oc = c;
								}
								if (slot >= top_slot) {
									if (curr_size < 12) {
										top_slot <<= 1;
										++curr_size;
									}
								}
							}
					}

					break;
			}

			if (interLaced) {
				startloc = orghgt+image_top_pos;
				pImage32 = (unsigned int *)(pDecBuf) + startloc * decwdt;
				if (!flag) {
					out_x = 0, out_y = startloc * dechgt;
					a_x = logi_wdt >> 2;
					b_x = ((logi_wdt*3) >> 2);
					c_x = logi_wdt >> 2;
					d_x = ((logi_wdt*3) >> 2);
					if (out_y <= (logi_hgt >> 2)) {
						a_y = logi_hgt >> 2;
						b_y = logi_hgt >> 2;
						c_y = ((logi_hgt*3) >> 2);
						d_y = ((logi_hgt*3) >> 2);
					} else {
						if (((out_y%logi_hgt) - (logi_hgt >> 2)) > 0) {
							a_y = ((out_y/logi_hgt)+1) * logi_hgt + (logi_hgt >> 2);
						} else if (((out_y%logi_hgt) - (logi_hgt >> 2)) < 0 || !(out_y%logi_hgt)) {
							a_y = ((out_y/logi_hgt)) * logi_hgt + (logi_hgt >> 2);
						} else {
							a_y = out_y;
						}

						if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) > 0) {
							c_y = ((out_y/logi_hgt+1) * logi_hgt + ((logi_hgt*3) >> 2));
						} else if (((out_y%logi_hgt) - ((logi_hgt*3) >> 2)) < 0 || !(out_y%logi_hgt)) {
							c_y = ((out_y/logi_hgt)) * logi_hgt + ((logi_hgt*3) >> 2);
						} else {
							c_y = out_y;
						}
						b_y = a_y, d_y = c_y;
					}
				}
				out_y -= dechgt;
			}

				backGround = (unsigned int *)(pFrameData->pPrevImg + (((image_top_pos+orghgt) * logi_wdt) << 2));
				for (rowcount = image_top_pos+orghgt; rowcount < logi_hgt; rowcount++) {
					if (flag) {
						for (i = 0; i < logi_wdt; i++) {
							*pImage32++ = backGround[i] << 2;
						}
					} else {
						out_x = 0, out_y += dechgt;
						if (c_y < end) {
							d1 = a_y - out_y;
							d2 = c_y - out_y;
							if ((0 <= d1 && d1 < dechgt) && 0 <= d2 && d2 < dechgt) {
								pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
								pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = a_x - out_x;
									d2 = b_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32 + (a_x/logi_wdt)) += backGround[i];
										*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
										a_x += logi_wdt, c_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32 + (b_x/logi_wdt)) += backGround[i];
										*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
										b_x += logi_wdt, d_x += logi_wdt;
									}
									out_x += decwdt;
								}
								a_x = logi_wdt >> 2, a_y += logi_hgt;
								b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
								c_x = logi_wdt >> 2, c_y += logi_hgt;
								d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
							} else if (0 <= d1 && d1 < dechgt) {
								pImage32 = (unsigned int *)(pDecBuf) + (a_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = a_x - out_x;
									d2 = b_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32 + (a_x/logi_wdt)) += backGround[i];
										a_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32 + (b_x/logi_wdt)) += backGround[i];
										b_x += logi_wdt;
									}
									out_x += decwdt;
								}
								a_x = logi_wdt >> 2, a_y += logi_hgt;
								b_x = ((logi_wdt*3) >> 2), b_y += logi_hgt;
							} else if (0 <= d2 && d2 < dechgt) {
								pImage32_2 = (unsigned int *)(pDecBuf) + (c_y/logi_hgt) * decwdt;
								for (i = 0; i < logi_wdt; i++) {
									d1 = c_x - out_x;
									d2 = d_x - out_x;
									if (0 <= d1 && d1 < decwdt) {
										*(pImage32_2 + (c_x/logi_wdt)) += backGround[i];
										c_x += logi_wdt;
									}
									if (0 <= d2  && d2 < decwdt) {
										*(pImage32_2 + (d_x/logi_wdt)) += backGround[i];
										d_x += logi_wdt;
									}
									out_x += decwdt;
								}
								c_x = logi_wdt >> 2, c_y += logi_hgt;
								d_x = ((logi_wdt*3) >> 2), d_y += logi_hgt;
							}
						}
					}
					backGround += logi_wdt;
				}
				

				if (bCenterAlign) {
					margin_wdt1_2 = (expected_width - resized_width) >> 1;
					margin_hgt1_2 = (expected_height - resized_height) >> 1;
				} else {
					margin_wdt1_2 = 0;
					margin_hgt1_2 = 0;
				}


				/* 565 Conversion  */
				pImage32 = (unsigned int *)(pDecBuf);
	
				for (i = 0, k = margin_hgt1_2; i < dechgt; i++) {
					pImage16 = (unsigned short *)((unsigned char *)pOutBits + ((margin_wdt1_2 + (expected_width) * k) << 1));
					for (j = 0; j < decwdt; j++) {
						if ((*pImage32>>24) == 0) {
							*(pImage16++) = ((*pImage32 & 0xf80000) >> 8) | ((*pImage32 & 0xfc00) >> 5) | ((*pImage32 & 0xf8) >> 3);
						} else if ((*pImage32 >> 24) == 1) {
							*(pImage16++) = (((*pImage32 & 0xf80000) >> 6) / 3)  | (((*pImage32 & 0xfc00) >> 3)/3) | (((*pImage32 & 0xf8) >> 1)/3);
						} else if ((*pImage32 >> 24) == 2) {
							*(pImage16++) = ((*pImage32 & 0xf80000) >> 7) | ((*pImage32 & 0xfc00) >> 4) | ((*pImage32 & 0xf8) >> 2);
						} else {
							*(pImage16++) = ui_backcolor565;
						}
						pImage32++;
					}
					k++;
				}

				if (decoderline) {
					IfegMemFree(decoderline);
					decoderline = 0;
				}
				if (pDecBuf) {
					IfegMemFree(pDecBuf);
					pDecBuf = 0;
				}

				pFrameData->offset = inputPos;
				pFrameData->imgCount++;

				return 1;

				break;

				default:
					break;

		}
	}
}

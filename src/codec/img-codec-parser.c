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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_PERFORMANCE_CHECK_)
#include <time.h>
#include <sys/time.h>
#endif

#include "media-thumb-debug.h"
#include "img-codec-common.h"
#include "img-codec-parser.h"

#define		FILE_READ_SIZE		4096
typedef struct _stream {
	HFile fd;
	unsigned int buffpos;
	unsigned int filepos;
	unsigned int filesize;
	unsigned int buffend;
	unsigned int debugpos;
	unsigned char *buffer;
} IFEGSTREAMCTRL;

ImgCodecType _ImgGetInfoStreaming(HFile hFile, unsigned long fileSize,
				  ImgImageInfo *imgInfo);
BOOL process_EXIF(unsigned char *ExifSection, unsigned int length,
		  unsigned int *pOrientataion);

ImgCodecType ImgGetInfoHFile(HFile hFile, unsigned long fileSize,
			     ImgImageInfo *imgInfo)
{
	ImgCodecType result = 0;
	SysAssert(hFile);
	SysAssert(fileSize);

	if (imgInfo == NULL) {
		result = _ImgGetInfoStreaming(hFile, fileSize, NULL);
	} else {
		result = _ImgGetInfoStreaming(hFile, fileSize, imgInfo);
	}

	DrmSeekFile(hFile, SEEK_SET, 0);

	return result;

}

ImgCodecType ImgGetInfoFile(const char *filePath, ImgImageInfo * imgInfo)
{
	HFile hFile;
	FmFileAttribute fileAttrib;
	ImgCodecType result;
	SysAssert(filePath);
	hFile = DrmOpenFile(filePath);

	if (hFile == (HFile) INVALID_HOBJ) {
		return IMG_CODEC_NONE;
	}
	DrmGetFileAttributes(filePath, &fileAttrib);

	if ((fileAttrib.fileSize == 0)) {
		DrmCloseFile(hFile);
		return IMG_CODEC_NONE;
	}

	result = ImgGetInfoHFile(hFile, fileAttrib.fileSize, imgInfo);

	DrmCloseFile(hFile);

	return result;

}

static unsigned char gIfegPNGHeader[] = { 
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned char gIfegJPEGHeader[] = { 0xFF, 0xD8 };
static unsigned char gIfegGIFHeader[] = { "GIF" };
static unsigned char gIfegBMPHeader[] = { 0x42, 0x4D };

static int _CheckBuffer(IFEGSTREAMCTRL *pIfegstreamctrl, unsigned int size)
{
	unsigned long fileread;

	if ((size + pIfegstreamctrl->buffpos) > pIfegstreamctrl->buffend) {
		if (pIfegstreamctrl->filepos == pIfegstreamctrl->filesize) {
			return 0;
		}

		if (pIfegstreamctrl->buffpos == 0) {
			if (DrmReadFile
			    (pIfegstreamctrl->fd, pIfegstreamctrl->buffer,
			     FILE_READ_SIZE, &fileread) == FALSE) {
				return 0;
			}
			if (fileread == 0) {
				return 0;
			}
			pIfegstreamctrl->buffend = fileread;
			pIfegstreamctrl->filepos += fileread;
			pIfegstreamctrl->buffpos = 0;
		} else {

			if (size >= 2048
			    || pIfegstreamctrl->buffend -
			    pIfegstreamctrl->buffpos > FILE_READ_SIZE) {
				return 0;
			}
			AcMemcpy(pIfegstreamctrl->buffer,
				 &pIfegstreamctrl->buffer[pIfegstreamctrl->
							  buffpos],
				 pIfegstreamctrl->buffend -
				 pIfegstreamctrl->buffpos);
			if (DrmReadFile
			    (pIfegstreamctrl->fd,
			     &pIfegstreamctrl->buffer[pIfegstreamctrl->buffend -
						      pIfegstreamctrl->buffpos],
			     pIfegstreamctrl->buffpos, &fileread) == FALSE) {
				return 0;
			}
			if (fileread == 0) {
				return 0;
			}
			pIfegstreamctrl->buffend =
			    pIfegstreamctrl->buffend -
			    pIfegstreamctrl->buffpos + fileread;
			pIfegstreamctrl->buffpos = 0;
			pIfegstreamctrl->filepos += fileread;
		}
		return 1;
	}
	return 2;
}

static unsigned int _IfegReadUINT(unsigned char *pBuffer)
{
	return (((*pBuffer) << 24) | ((*(pBuffer + 1)) << 16) |
		((*(pBuffer + 2)) << 8) | (*(pBuffer + 3)));
}

ImgCodecType ImgGetInfo(unsigned char *pEncodedData, unsigned long fileSize,
			ImgImageInfo *imgInfo)
{
	unsigned int width = 0, height = 0;

	/* Initialize values */
	if (imgInfo) {
		imgInfo->width = 0;
		imgInfo->height = 0;
		imgInfo->numberOfFrame = 1;
		imgInfo->bOrientation = 0;
	}

	SysAssert(pEncodedData);
	SysAssert(fileSize);

	/***********************  PNG  *************************/
	if (AcMemcmp(pEncodedData, gIfegPNGHeader, PNG_HEADER_LENGTH) == 0) {
		unsigned char tmp;

		if (fileSize < 40) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in PNG");
			return IMG_CODEC_UNKNOWN_TYPE;
		}
		/* Get Image Width */
		width = _IfegReadUINT((pEncodedData + 16));

		/* Get Image Height */
		height = _IfegReadUINT((pEncodedData + 20));

		/* Read Interlace byte */
		tmp = *(pEncodedData + 28);
		/* If image is interlaced then multiple should be 2 */
		if (tmp) {
			thumb_dbg("Interlaced PNG Image.");
		}

		thumb_dbg("type: IMG_CODEC_PNG");

		if (imgInfo) {
			imgInfo->width = width;
			imgInfo->height = height;
		}
		return IMG_CODEC_PNG;
	}
	/***********************  BMP  *************************/
	else if (AcMemcmp(pEncodedData, gIfegBMPHeader, BMP_HEADER_LENGTH) == 0) {
		/* Parse BMP File and get image width and image height */
		if (fileSize < 26) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in BMP");
			return IMG_CODEC_UNKNOWN_TYPE;
		}
		if (imgInfo) {
			imgInfo->width =
			    pEncodedData[18] | (pEncodedData[19] << 8) |
			    (pEncodedData[20] << 16) | (pEncodedData[21] << 24);
			imgInfo->height =
			    pEncodedData[22] | (pEncodedData[23] << 8) |
			    (pEncodedData[24] << 16) | (pEncodedData[25] << 24);
		}

		thumb_dbg("type : IMG_CODEC_BMP");
		return IMG_CODEC_BMP;

	}
	/***********************  GIF  *************************/
	else if (AcMemcmp(pEncodedData, gIfegGIFHeader, GIF_HEADER_LENGTH) == 0) {
		int length;
		int inputPos = 0;
		int temp;
		int finished;
		int imagecount = 0;

		if ((unsigned int)inputPos + 13 > fileSize) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return IMG_CODEC_UNKNOWN_TYPE;
		}

		if (pEncodedData[0] != 'G' || pEncodedData[1] != 'I'
		    || pEncodedData[2] != 'F' || pEncodedData[3] < '0'
		    || pEncodedData[3] > '9' || pEncodedData[4] < '0'
		    || pEncodedData[4] > '9' || pEncodedData[5] < 'A'
		    || pEncodedData[5] > 'z') {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return IMG_CODEC_UNKNOWN_TYPE;
		}
		/* get Logical width, height */
		if (imgInfo) {
			imgInfo->width =
			    pEncodedData[6] | (pEncodedData[7] << 8);
			imgInfo->height =
			    pEncodedData[8] | (pEncodedData[9] << 8);
		}

		if ((pEncodedData[10] & 0x80) != 0)	/* Global color table */ {
			temp = (pEncodedData[10] & 0x7) + 1;
			length = (1 << temp) * 3;
			inputPos += length;
			if ((unsigned int)inputPos > fileSize) {
				thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
				return IMG_CODEC_UNKNOWN_TYPE;
			}
		}

		inputPos += 13;
		finished = 0;

		/* still gif image (image_cnt = 1) */
		while (!finished) {
			if ((unsigned int)inputPos > fileSize)
				break;

			switch (pEncodedData[inputPos++]) {
			case 0x3b:	/* End of the GIF dataset */
				finished = 1;
				break;

			case 0x21:	/* Extension Block */
				switch (pEncodedData[inputPos++]) {
				case 0xf9:	/* Graphic control extension block */
					if (4 != pEncodedData[inputPos++]) {	/* data length : fixed 4 bytes */ 
						thumb_warn
						    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
						return IMG_CODEC_UNKNOWN_TYPE;
					}
					inputPos += 4;
					inputPos++;	/* block end */
					break;

				case 0x01:	/* Plain Text block */
				case 0xfe:	/* Comment Extension block */
				case 0xff:	/* Appliation Extension block */
					while ((length = pEncodedData[inputPos++]) > 0) {	/* get the data length */
						inputPos += (length);
						if ((unsigned int)inputPos >
						    fileSize) {
							thumb_warn
							    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
							return
							    IMG_CODEC_UNKNOWN_TYPE;
						}
					}
					break;

				default:
					break;
				}

				break;

			case 0x2c:	/* Start of an image object. Read the image description. */

				if ((unsigned int)inputPos + 9 > fileSize) {
					thumb_warn
					    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
					return IMG_CODEC_UNKNOWN_TYPE;
				}
				/* Color Resolution */
				if ((pEncodedData[inputPos + 8] & 0x80) != 0) {	/* Logical color table */
					temp =
					    (pEncodedData[inputPos + 8] & 0x7) +
					    1;
					length = (1 << temp) * 3;
					inputPos += length;
					if ((unsigned int)inputPos > fileSize) {
						thumb_warn
						    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
						return IMG_CODEC_UNKNOWN_TYPE;
					}
				}

				inputPos += 9;

				temp = pEncodedData[inputPos++];
				if (temp < 2 || 9 < temp) {
					thumb_warn
					    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
					return IMG_CODEC_UNKNOWN_TYPE;
				}

				do {
					length = pEncodedData[inputPos++];
					inputPos += length;
					if ((unsigned int)inputPos > fileSize) {
						thumb_warn
						    ("IMG_CODEC_UNKNOWN_TYPE in GIF");
						return IMG_CODEC_UNKNOWN_TYPE;
					}
				} while (length);

				if (!imagecount)
					imagecount++;
				else {
					if (imgInfo)
						imgInfo->numberOfFrame = 2;
					return IMG_CODEC_AGIF;
				}
				break;

			default:
				finished = 0;
				break;

			}	/* end of  switch (pEncodedData[inputPos++]) */
		}	/* end of  while (pImage->bitmapCount > image_cnt && !finished) */

		return IMG_CODEC_GIF;
	}

	thumb_warn("IMG_CODEC_UNKNOWN_TYPE");
	return IMG_CODEC_UNKNOWN_TYPE;
}

ImgCodecType _ImgGetInfoStreaming(HFile hFile, unsigned long fileSize,
				  ImgImageInfo *imgInfo)
{
	unsigned int fileleft;
	unsigned long fileread;
	unsigned char EncodedDataBuffer[4096];

	unsigned int *pNumberOfFrames = NULL;
	unsigned int *pWidth = NULL;
	unsigned int *pHeight = NULL;
	unsigned int *pOrientation = NULL;

	if (imgInfo == NULL) {
		pNumberOfFrames = NULL;
		pWidth = NULL;
		pHeight = NULL;
		pOrientation = NULL;
	} else {
		pWidth = &(imgInfo->width);
		pHeight = &(imgInfo->height);
		pOrientation = &(imgInfo->bOrientation);
		pNumberOfFrames = &(imgInfo->numberOfFrame);

		*pWidth = 0;
		*pHeight = 0;
		*pOrientation = 0;
	}

	AcMemset(EncodedDataBuffer, 0, 4096);

	/* Initialize values */
	if (pNumberOfFrames) {
		*pNumberOfFrames = 1;
	}

	SysAssert((const char *)&fileSize);

	if (DrmReadFile(hFile, EncodedDataBuffer, 8, &fileread) == FALSE) {
		thumb_err("DrmReadFile was failed");
		return IMG_CODEC_NONE;
	}

	if (fileread < MINIMUM_HEADER_BYTES) {
		thumb_warn("IMG_CODEC_UNKNOWN_TYPE");
		return IMG_CODEC_UNKNOWN_TYPE;
	}
	/***********************  PNG  *************************/
	if (AcMemcmp(EncodedDataBuffer, gIfegPNGHeader, PNG_HEADER_LENGTH) == 0) {
		unsigned char tmp;

		if (DrmReadFile(hFile, EncodedDataBuffer, 32, &fileread) ==
		    FALSE) {
			thumb_err("DrmReadFile was failed");
			return IMG_CODEC_NONE;
		}
		if (fileread < 32) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in PNG");
			return IMG_CODEC_UNKNOWN_TYPE;
		}
		/* Get Image Width */
		if (pWidth) {
			*pWidth = _IfegReadUINT((EncodedDataBuffer + 8));
		}

		/* Get Image Height */
		if (pHeight) {
			*pHeight = _IfegReadUINT((EncodedDataBuffer + 12));
		}

		/* Read Interlace byte */
		tmp = *(EncodedDataBuffer + 20);
		/* If image is interlaced then multiple should be 2 */
		if (tmp) {
			thumb_dbg("Interlaced PNG Image.\n");
		}
		thumb_dbg("IMG_CODEC_PNG\n");
		return IMG_CODEC_PNG;
	}
	/***********************  BMP  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegBMPHeader, BMP_HEADER_LENGTH)
		 == 0) {
		/* Parse BMP File and get image width and image height */
		if (DrmReadFile(hFile, &EncodedDataBuffer[8], 18, &fileread) ==
		    FALSE) {
			thumb_err("DrmReadFile was failed");
			return IMG_CODEC_NONE;
		}
		if (fileread < 18) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in BMP");
			return IMG_CODEC_UNKNOWN_TYPE;
		}
		if (pWidth == NULL || pHeight == NULL
		    || pNumberOfFrames == NULL) {
			return IMG_CODEC_BMP;
		}
		if (pWidth) {
			*pWidth =
			    EncodedDataBuffer[18] | (EncodedDataBuffer[19] << 8)
			    | (EncodedDataBuffer[20] << 16) |
			    (EncodedDataBuffer[21] << 24);
		}
		if (pHeight) {
			*pHeight =
			    EncodedDataBuffer[22] | (EncodedDataBuffer[23] << 8)
			    | (EncodedDataBuffer[24] << 16) |
			    (EncodedDataBuffer[25] << 24);
		}

		thumb_dbg("IMG_CODEC_BMP");
		return IMG_CODEC_BMP;
	}
	/***********************  GIF  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegGIFHeader, GIF_HEADER_LENGTH)
		 == 0) {
		unsigned int tablelength = 0;
		unsigned int imagecount = 0;
		int finished = 0;
		unsigned char temp;
		unsigned int length;
		IFEGSTREAMCTRL ifegstreamctrl;

		if (13 > fileSize) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return IMG_CODEC_UNKNOWN_TYPE;
		}

		if (DrmReadFile(hFile, &EncodedDataBuffer[8], 5, &fileread) ==
		    FALSE) {
			thumb_err("DrmReadFile was failed");

			return IMG_CODEC_NONE;
		}
		if (fileread < 5) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return IMG_CODEC_UNKNOWN_TYPE;
		}

		if (EncodedDataBuffer[0] != 'G' || EncodedDataBuffer[1] != 'I'
		    || EncodedDataBuffer[2] != 'F' || EncodedDataBuffer[3] < '0'
		    || EncodedDataBuffer[3] > '9' || EncodedDataBuffer[4] < '0'
		    || EncodedDataBuffer[4] > '9' || EncodedDataBuffer[5] < 'A'
		    || EncodedDataBuffer[5] > 'z') {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return IMG_CODEC_UNKNOWN_TYPE;
		}

		if (!(pWidth && pHeight)) {
			return IMG_CODEC_UNKNOWN_TYPE;
		} else {
			*pWidth =
			    EncodedDataBuffer[6] | (EncodedDataBuffer[7] << 8);
			*pHeight =
			    EncodedDataBuffer[8] | (EncodedDataBuffer[9] << 8);
		}

		thumb_dbg("Logical width : %d, Height : %d", *pWidth,
			     *pHeight);

		if ((EncodedDataBuffer[10] & 0x80) != 0) {	/* Global color table */
			temp = (EncodedDataBuffer[10] & 0x7) + 1;
			tablelength = (1 << temp) * 3;

			if ((tablelength * sizeof(char)) >
			    sizeof(EncodedDataBuffer)) {
				thumb_warn
				    ("_ImgGetInfoStreaming :table length is more than buffer length");
				return IMG_CODEC_UNKNOWN_TYPE;
			}

			if (13 + tablelength > fileSize) {
				thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
				return IMG_CODEC_UNKNOWN_TYPE;
			}
			/* coverity[ -tainted_data_argument : EncodedDataBuffer ] */
			if (DrmReadFile
			    (hFile, EncodedDataBuffer, tablelength,
			     &fileread) == FALSE) {
				thumb_err("DrmReadFile was failed");

				return IMG_CODEC_NONE;
			}
			if (fileread < tablelength) {
				thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
				return IMG_CODEC_UNKNOWN_TYPE;
			}
		}

		fileleft = fileSize - 13 - tablelength;

		ifegstreamctrl.fd = hFile;
		ifegstreamctrl.filesize = fileleft;
		ifegstreamctrl.filepos = 0;
		ifegstreamctrl.buffpos = 0;
		ifegstreamctrl.buffend = 0;
		ifegstreamctrl.buffer = EncodedDataBuffer;

		while (!finished) {
			if (ifegstreamctrl.buffpos > ifegstreamctrl.buffend)
				break;
			if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
				thumb_warn("_CheckBuffer was failed");
				return IMG_CODEC_UNKNOWN_TYPE;
			}

			switch (EncodedDataBuffer[ifegstreamctrl.buffpos++]) {

			case 0x3b:	/* End of the GIF dataset */
				finished = 1;
				break;

			case 0x21:	/* Extension Block */
				if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
					thumb_warn("_CheckBuffer was failed");
					return IMG_CODEC_UNKNOWN_TYPE;
				}

				switch (EncodedDataBuffer
					[ifegstreamctrl.buffpos++]) {

				case 0xf9:	/* Graphic control extension block */
					if (_CheckBuffer(&ifegstreamctrl, 6) ==
					    0) {
						thumb_warn
						    ("_CheckBuffer was failed");
						return IMG_CODEC_UNKNOWN_TYPE;
					}

					if (4 != EncodedDataBuffer[ifegstreamctrl.buffpos++]) {	/* data length : fixed 4 bytes */
						return 0;
					}
					ifegstreamctrl.buffpos += 4;
					ifegstreamctrl.buffpos++;	/* block end */
					break;

				case 0x01:	/* Plain Text block */
				case 0xfe:	/* Comment Extension block */
				case 0xff:	/* Appliation Extension block */
					if (_CheckBuffer(&ifegstreamctrl, 1) ==
					    0) {
						thumb_warn
						    ("_CheckBuffer was failed");
						return IMG_CODEC_UNKNOWN_TYPE;
					}

					if (ifegstreamctrl.buffpos >
					    sizeof(EncodedDataBuffer)) {
						thumb_warn
						    ("buffer position exceeds buffer max length ");
						return IMG_CODEC_UNKNOWN_TYPE;
					}

					while ((ifegstreamctrl.buffpos < sizeof(EncodedDataBuffer)) && ((length = EncodedDataBuffer[ifegstreamctrl.buffpos++]) > 0)) {	/* get the data length */
						if (_CheckBuffer
						    (&ifegstreamctrl,
						     length) == 0) {
							thumb_warn
							    ("_CheckBuffer was failed");
							return
							    IMG_CODEC_UNKNOWN_TYPE;
						}

						/* Check integer overflow */
						if (ifegstreamctrl.buffpos > 0xffffffff - length) {
							thumb_err("Prevent integer overflow..");
							return IMG_CODEC_UNKNOWN_TYPE;
						}

						ifegstreamctrl.buffpos +=
						    (length);
						/* File End Check */
					}
					break;

				default:
					break;
				}

				break;

			case 0x2c:	/* Start of an image object. Read the image description. */

				if (_CheckBuffer(&ifegstreamctrl, 9) == 0) {
					thumb_warn("_CheckBuffer was failed");
					return IMG_CODEC_UNKNOWN_TYPE;
				}
#if 1
				if (imagecount == 0) {
					/* Regard the width/height of the first image block as the size of thumbnails. */
					int img_block_w, img_block_h,
					    img_block_left, img_block_top;
					img_block_left =
					    EncodedDataBuffer[ifegstreamctrl.
							      buffpos] |
					    (EncodedDataBuffer
					     [ifegstreamctrl.buffpos + 1] << 8);
					img_block_top =
					    EncodedDataBuffer[ifegstreamctrl.
							      buffpos +
							      2] |
					    (EncodedDataBuffer
					     [ifegstreamctrl.buffpos + 3] << 8);

					img_block_w =
					    EncodedDataBuffer[ifegstreamctrl.
							      buffpos +
							      4] |
					    (EncodedDataBuffer
					     [ifegstreamctrl.buffpos + 5] << 8);
					img_block_h =
					    EncodedDataBuffer[ifegstreamctrl.
							      buffpos +
							      6] |
					    (EncodedDataBuffer
					     [ifegstreamctrl.buffpos + 7] << 8);
					thumb_dbg
					    ("Image block width : %d, Height : %d, left:%d, top:%d\n",
					     img_block_w, img_block_h,
					     img_block_left, img_block_top);

					*pWidth = img_block_w;
					*pHeight = img_block_h;
				}
#endif
				/* Color Resolution */
				if ((EncodedDataBuffer[ifegstreamctrl.buffpos + 8] & 0x80) != 0) {	/* Logical color table */
					temp =
					    (EncodedDataBuffer
					     [ifegstreamctrl.buffpos +
					      8] & 0x7) + 1;
					length = (1 << temp) * 3;
					if (_CheckBuffer
					    (&ifegstreamctrl,
					     length + 9) == 0) {
						thumb_warn
						    ("_CheckBuffer was failed");
						return IMG_CODEC_UNKNOWN_TYPE;
					}

					ifegstreamctrl.buffpos += length;
					/* File End Check */
				}

				ifegstreamctrl.buffpos += 9;

				if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
					thumb_warn("_CheckBuffer was failed");
					return IMG_CODEC_UNKNOWN_TYPE;
				}

				temp =
				    EncodedDataBuffer[ifegstreamctrl.buffpos++];
				if (temp < 2 || 9 < temp) {
					return IMG_CODEC_UNKNOWN_TYPE;
				}

				do {
					if (_CheckBuffer(&ifegstreamctrl, 1) ==
					    0) {
						thumb_warn
						    ("_CheckBuffer was failed");
						return IMG_CODEC_UNKNOWN_TYPE;
					}

					length =
					    EncodedDataBuffer[ifegstreamctrl.
							      buffpos++];
					if ((length + ifegstreamctrl.buffpos) > ifegstreamctrl.buffend) {
						length =
						    length +
						    ifegstreamctrl.buffpos -
						    ifegstreamctrl.buffend;
						if (DrmSeekFile
						    (ifegstreamctrl.fd,
						     SEEK_CUR,
						     length) == FALSE) {
							if (imagecount) {
								if (pNumberOfFrames)
									*pNumberOfFrames
									    = 2;
								thumb_dbg
								    ("IMG_CODEC_AGIF");
								return
								    IMG_CODEC_AGIF;
							}
							return
							    IMG_CODEC_UNKNOWN_TYPE;
						}
						ifegstreamctrl.filepos +=
						    length;
						ifegstreamctrl.buffpos = 0;
						ifegstreamctrl.buffend = 0;
					} else {
						ifegstreamctrl.buffpos +=
						    length;
					}

					/* File End Check */
				} while (length);
				if (!imagecount)
					imagecount++;
				else {
					if (pNumberOfFrames)
						*pNumberOfFrames = 2;
					thumb_dbg("IMG_CODEC_AGIF");
					return IMG_CODEC_AGIF;
				}
				break;

			default:
				finished = 0;
				break;

			}
		}
		if (pNumberOfFrames) {
			*pNumberOfFrames = 1;
		}
		thumb_dbg("IMG_CODEC_GIF");
		return IMG_CODEC_GIF;
	}

	/***********************  Jpeg  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegJPEGHeader, JPG_HEADER_LENGTH)
		 == 0) {
#if 1
		/*
		   IFEGSTREAMCTRL ifegstreamctrl;
		   if( DrmReadFile(hFile, &EncodedDataBuffer[8], FILE_READ_SIZE-8, &fileread) == FALSE )
		   {
		   thumb_err( "DrmReadFile was failed");

		   return IMG_CODEC_NONE;
		   }

		   ifegstreamctrl.fd = hFile;
		   ifegstreamctrl.filesize = fileSize;
		   ifegstreamctrl.filepos = fileread+8;
		   ifegstreamctrl.buffpos = 2;
		   ifegstreamctrl.buffend = 8+fileread;
		   ifegstreamctrl.buffer = EncodedDataBuffer;
		 */

#else	/* Get w / h from jpeg header */

#ifdef _PERFORMANCE_CHECK_
		long start = 0L, end = 0L;
		start = mediainfo_get_debug_time();
#endif
		unsigned char *img_buf = NULL;
		img_buf = (unsigned char *)malloc(fileSize);

		rewind(hFile);
		if (DrmReadFile(hFile, img_buf, fileSize, &fileread) == FALSE) {
			thumb_err("DrmReadFile was failed");

			return IMG_CODEC_NONE;
		}

		unsigned short block_length = img_buf[4] * 256 + img_buf[5];
		thumb_dbg("block length : %d", block_length);
		int i = 4;

		while (i < fileSize) {
			i += block_length;
			if (i >= fileSize) {
				thumb_warn
				    ("Failed to get w / h from jpeg at index [%d]",
				     i);
				break;
			}

			if (img_buf[i] != 0xFF) {
				thumb_warn
				    ("Failed to get w / h from jpeg at index [%d]",
				     i);
				break;
			}

			if (img_buf[i + 1] == 0xC0) {
				*pWidth = img_buf[i + 5] * 256 + img_buf[i + 6];
				*pHeight =
				    img_buf[i + 7] * 256 + img_buf[i + 8];
				break;
			} else {
				i += 2;
				block_length =
				    img_buf[i] * 256 + img_buf[i + 1];
				thumb_dbg("new block length : %d",
					     block_length);
			}
		}
		thumb_dbg("Jpeg w: %d, h: %d", *pWidth, *pHeight);

		if (img_buf)
			free(img_buf);

#if defined(_PERFORMANCE_CHECK_) && defined(_USE_LOG_FILE_)
		end = mediainfo_get_debug_time();
		double get_size =
		    ((double)(end - start) / (double)CLOCKS_PER_SEC);
		thumb_dbg("get_size from jpeg header : %f", get_size);
		mediainfo_init_file_debug();
		mediainfo_file_dbg("get_size from jpeg header : %f", get_size);
		mediainfo_close_file_debug();
#endif

#endif				/* End of Get w / h from jpeg header */

		thumb_dbg("IMG_CODEC_JPEG");
		return IMG_CODEC_JPEG;
	}
	return IMG_CODEC_UNKNOWN_TYPE;
}

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

#include <media-util-err.h>
#include "media-thumb-debug.h"
#include "img-codec-common.h"
#include "img-codec-parser.h"

#define		MINIMUM_HEADER_BYTES	8

#define		PNG_HEADER_LENGTH	8
#define		JPG_HEADER_LENGTH	2
#define		GIF_HEADER_LENGTH	3
#define		TIFF_HEADER_LENGTH	2
#define		BMP_HEADER_LENGTH	2
#define		WBMP_HEADER_LENGTH	2
#define		TIFF_IMAGE_WIDTH	0x100
#define		TIFF_IMAGE_HEIGHT	0x101

#define 	JPG_HEADER_TYPE_LENGTH 2
#define 	JPG_BLOCK_SIZE_LENGTH 2
#define 	JPG_IMAGE_SIZE_LENGTH 8

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

static unsigned char gIfegPNGHeader[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned char gIfegJPEGHeader[] = { 0xFF, 0xD8 };
static unsigned char gIfegGIFHeader[] = { "GIF" };
static unsigned char gIfegBMPHeader[] = { 0x42, 0x4D };
static unsigned char gIfegWBMPHeader[] = { 0x00, 0x00 };

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

static int _ImgGetImageInfo(HFile hFile, unsigned long fileSize, char *fileExt, ImgCodecType *type, unsigned int *width, unsigned int *height, bool fast_mode)
{
	unsigned int fileleft;
	unsigned long fileread;
	unsigned char EncodedDataBuffer[4096];

	unsigned int *pWidth = NULL;
	unsigned int *pHeight = NULL;
	int ret = MS_MEDIA_ERR_NONE;

	if (type == NULL || width == NULL ||height == NULL ) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	} else {
		pWidth = width;
		pHeight = height;

		*type = IMG_CODEC_UNKNOWN_TYPE;
		*pWidth = 0;
		*pHeight = 0;
	}

	AcMemset(EncodedDataBuffer, 0, 4096);

	SysAssert((const char *)&fileSize);

	if (DrmReadFile(hFile, EncodedDataBuffer, 8, &fileread) == FALSE) {
		thumb_err("DrmReadFile was failed");
		return MS_MEDIA_ERR_FILE_READ_FAIL;
	}
	if (fileread < MINIMUM_HEADER_BYTES) {
		thumb_warn("IMG_CODEC_UNKNOWN_TYPE");
		return ret;
	}

	if (AcMemcmp(EncodedDataBuffer, gIfegJPEGHeader, JPG_HEADER_LENGTH) == 0) {
		if (fast_mode == FALSE) {
			unsigned char header_type[JPG_HEADER_TYPE_LENGTH];
			unsigned char block_size[JPG_BLOCK_SIZE_LENGTH];
			unsigned char image_size[JPG_IMAGE_SIZE_LENGTH];

			rewind(hFile);

			unsigned short block_length = EncodedDataBuffer[4] * 256 + EncodedDataBuffer[5];
			thumb_dbg("block length : %d", block_length);
			unsigned int i = 4;

			if (DrmSeekFile(hFile, SEEK_CUR, block_length+4) == FALSE) {
				thumb_err("DrmSeekFile was failed");
				return MS_MEDIA_ERR_FILE_READ_FAIL;
			}

			while (i < fileSize) {
				i += block_length;
				if (i >= fileSize) {
					thumb_warn("Failed to get w / h from jpeg at index [%d]", i);
					break;
				}

				AcMemset(header_type, 0, JPG_HEADER_TYPE_LENGTH);
				if (DrmReadFile(hFile, header_type, (ULONG)JPG_HEADER_TYPE_LENGTH, &fileread) == FALSE) {
					thumb_err("DrmReadFile was failed");
					return MS_MEDIA_ERR_FILE_READ_FAIL;
				}

				if (header_type[0] != 0xFF) {
					thumb_warn("Failed to get w / h from jpeg at index [%d]", i);
					break;
				}

				if (header_type[1] == 0xC0 || header_type[1] == 0xC2) {
					AcMemset(image_size, 0, JPG_IMAGE_SIZE_LENGTH);
					if (DrmReadFile(hFile, image_size, (ULONG)JPG_IMAGE_SIZE_LENGTH, &fileread) == FALSE) {
						thumb_err("DrmReadFile was failed");
						return MS_MEDIA_ERR_FILE_READ_FAIL;
					}

					*pWidth = image_size[5] * 256 + image_size[6];
					*pHeight = image_size[3] * 256 + image_size[4];
					break;
				} else {
					i += 2;
					AcMemset(block_size, 0, JPG_BLOCK_SIZE_LENGTH);
					if (DrmReadFile(hFile, block_size, (ULONG)JPG_BLOCK_SIZE_LENGTH, &fileread) == FALSE) {
						thumb_err("DrmReadFile was failed");
						return MS_MEDIA_ERR_FILE_READ_FAIL;
					}

					block_length = block_size[0] * 256 + block_size[1];
					thumb_dbg("new block length : %d", block_length);
					if (DrmSeekFile(hFile, SEEK_CUR, block_length-JPG_BLOCK_SIZE_LENGTH) == FALSE) {
						thumb_err("DrmSeekFile was failed");
						return MS_MEDIA_ERR_FILE_READ_FAIL;
					}
				}
			}
			thumb_dbg("Jpeg w: %d, h: %d", *pWidth, *pHeight);
		}
		thumb_dbg("IMG_CODEC_JPEG");
		*type = IMG_CODEC_JPEG;
	}
	/***********************  PNG  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegPNGHeader, PNG_HEADER_LENGTH) == 0) {
		unsigned char tmp;

		if (DrmReadFile(hFile, EncodedDataBuffer, 32, &fileread) ==
			FALSE) {
			thumb_err("DrmReadFile was failed");
			return MS_MEDIA_ERR_FILE_READ_FAIL;
		}
		if (fileread < 32) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in PNG");
			return ret;
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
			thumb_dbg("Interlaced PNG Image.");
		}
		thumb_dbg("IMG_CODEC_PNG");
		*type = IMG_CODEC_PNG;
	}
	/***********************  BMP  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegBMPHeader, BMP_HEADER_LENGTH) == 0) {
		/* Parse BMP File and get image width and image height */
		if (DrmReadFile(hFile, &EncodedDataBuffer[8], 18, &fileread) == FALSE) {
			thumb_err("DrmReadFile was failed");
			return MS_MEDIA_ERR_FILE_READ_FAIL;
		}
		if (fileread < 18) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in BMP");
			return ret;
		}
		if (pWidth) {
			*pWidth =
				EncodedDataBuffer[18] | (EncodedDataBuffer[19] << 8)
				| (EncodedDataBuffer[20] << 16) |
				(EncodedDataBuffer[21] << 24);
		}
		if (pHeight) {
			// add the reference function abs(). may have negative height values in bmp header.
			*pHeight = abs(EncodedDataBuffer[22] | (EncodedDataBuffer[23] << 8) | 
						  (EncodedDataBuffer[24] << 16) | (EncodedDataBuffer[25] << 24));
		}

		thumb_dbg("IMG_CODEC_BMP");
		*type = IMG_CODEC_BMP;
	}
	/***********************  GIF  *************************/
	else if (AcMemcmp(EncodedDataBuffer, gIfegGIFHeader, GIF_HEADER_LENGTH) == 0) {
		unsigned int tablelength = 0;
		unsigned int imagecount = 0;
		int finished = 0;
		unsigned char temp;
		unsigned int length;
		IFEGSTREAMCTRL ifegstreamctrl;

		if (13 > fileSize) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return ret;
		}

		if (DrmReadFile(hFile, &EncodedDataBuffer[8], 5, &fileread) == FALSE) {
			thumb_err("DrmReadFile was failed");
			return MS_MEDIA_ERR_FILE_READ_FAIL;
		}
		if (fileread < 5) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return ret;
		}

		if (EncodedDataBuffer[0] != 'G' || EncodedDataBuffer[1] != 'I'
			|| EncodedDataBuffer[2] != 'F' || EncodedDataBuffer[3] < '0'
			|| EncodedDataBuffer[3] > '9' || EncodedDataBuffer[4] < '0'
			|| EncodedDataBuffer[4] > '9' || EncodedDataBuffer[5] < 'A'
			|| EncodedDataBuffer[5] > 'z') {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
			return ret;
		}

		*pWidth = EncodedDataBuffer[6] | (EncodedDataBuffer[7] << 8);
		*pHeight = EncodedDataBuffer[8] | (EncodedDataBuffer[9] << 8);

		thumb_dbg("Logical width : %d, Height : %d", *pWidth, *pHeight);

		if ((EncodedDataBuffer[10] & 0x80) != 0) {	/* Global color table */
			temp = (EncodedDataBuffer[10] & 0x7) + 1;
			tablelength = (1 << temp) * 3;

			if ((tablelength * sizeof(char)) > sizeof(EncodedDataBuffer)) {
				thumb_warn("_ImgGetInfoStreaming :table length is more than buffer length");
				return ret;
			}

			if (13 + tablelength > fileSize) {
				thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
				return ret;
			}
			/* coverity[ -tainted_data_argument : EncodedDataBuffer ] */
			if (DrmReadFile(hFile, EncodedDataBuffer, tablelength, &fileread) == FALSE) {
				thumb_err("DrmReadFile was failed");
				return MS_MEDIA_ERR_FILE_READ_FAIL;
			}
			if (fileread < tablelength) {
				thumb_warn("IMG_CODEC_UNKNOWN_TYPE in GIF");
				return ret;
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
				return ret;
			}

			switch (EncodedDataBuffer[ifegstreamctrl.buffpos++]) {

			case 0x3b:	/* End of the GIF dataset */
				finished = 1;
				break;

			case 0x21:	/* Extension Block */
				if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
					thumb_warn("_CheckBuffer was failed");
					return ret;
				}

				switch (EncodedDataBuffer[ifegstreamctrl.buffpos++]) {

				case 0xf9:	/* Graphic control extension block */
					if (_CheckBuffer(&ifegstreamctrl, 6) == 0) {
						thumb_warn("_CheckBuffer was failed");
						return ret;
					}

					if (4 != EncodedDataBuffer[ifegstreamctrl.buffpos++]) { /* data length : fixed 4 bytes */
						*type = 0;
					}
					ifegstreamctrl.buffpos += 4;
					ifegstreamctrl.buffpos++;	/* block end */
					break;

				case 0x01:	/* Plain Text block */
				case 0xfe:	/* Comment Extension block */
				case 0xff:	/* Appliation Extension block */
					if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
						thumb_warn("_CheckBuffer was failed");
						return ret;
					}

					if (ifegstreamctrl.buffpos > sizeof(EncodedDataBuffer)) {
						thumb_warn("buffer position exceeds buffer max length ");
						return ret;
					}

					while ((ifegstreamctrl.buffpos < sizeof(EncodedDataBuffer))
						&& ((length = EncodedDataBuffer[ifegstreamctrl.buffpos++]) > 0)) {	/* get the data length */
						if (_CheckBuffer(&ifegstreamctrl, length) == 0) {
							thumb_warn("_CheckBuffer was failed");
							return ret;
						}

						/* Check integer overflow */
						if (ifegstreamctrl.buffpos > 0xffffffff - length) {
							thumb_err("Prevent integer overflow..");
							return ret;
						}

						ifegstreamctrl.buffpos += (length);
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
					return ret;
				}
#if 1
				if (imagecount == 0) {
					/* Regard the width/height of the first image block as the size of thumbnails. */
					int img_block_w, img_block_h;

					img_block_w = EncodedDataBuffer[ifegstreamctrl.buffpos + 4] |(EncodedDataBuffer[ifegstreamctrl.buffpos + 5] << 8);
					img_block_h = EncodedDataBuffer[ifegstreamctrl.buffpos + 6] |(EncodedDataBuffer[ifegstreamctrl.buffpos + 7] << 8);
					thumb_dbg ("Image block width : %d, Height : %d, left:%d, top:%d", img_block_w, img_block_h);

					*pWidth = img_block_w;
					*pHeight = img_block_h;
				}
#endif
				/* Color Resolution */
				if ((EncodedDataBuffer[ifegstreamctrl.buffpos + 8] & 0x80) != 0) {	/* Logical color table */
					temp = (EncodedDataBuffer[ifegstreamctrl.buffpos + 8] & 0x7) + 1;
					length = (1 << temp) * 3;
					if (_CheckBuffer(&ifegstreamctrl, length + 9) == 0) {
						thumb_warn("_CheckBuffer was failed");
						return ret;
					}

					ifegstreamctrl.buffpos += length;
					/* File End Check */
				}

				ifegstreamctrl.buffpos += 9;

				if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
					thumb_warn("_CheckBuffer was failed");
					return ret;
				}

				temp = EncodedDataBuffer[ifegstreamctrl.buffpos++];
				if (temp < 2 || 9 < temp) {
					return ret;
				}

				do {
					if (_CheckBuffer(&ifegstreamctrl, 1) == 0) {
						thumb_warn("_CheckBuffer was failed");
						return ret;
					}

					length =EncodedDataBuffer[ifegstreamctrl.buffpos++];
					if ((length + ifegstreamctrl.buffpos) > ifegstreamctrl.buffend) {
						length =
							length +
							ifegstreamctrl.buffpos -
							ifegstreamctrl.buffend;
						if (DrmSeekFile(ifegstreamctrl.fd, SEEK_CUR, length) == FALSE) {
							if (imagecount) {
								thumb_dbg("IMG_CODEC_AGIF");
								*type = IMG_CODEC_AGIF;
								return ret;
							}
							return MS_MEDIA_ERR_FILE_READ_FAIL;
						}
						ifegstreamctrl.filepos += length;
						ifegstreamctrl.buffpos = 0;
						ifegstreamctrl.buffend = 0;
					} else {
						ifegstreamctrl.buffpos +=length;
					}

					/* File End Check */
				} while (length);
				if (!imagecount)
					imagecount++;
				else {
					thumb_dbg("IMG_CODEC_AGIF");
					*type = IMG_CODEC_AGIF;
					return ret;
				}
				break;

			default:
				finished = 0;
				break;

			}
		}
		thumb_dbg("IMG_CODEC_GIF");
		*type = IMG_CODEC_GIF;
	}
	/***********************  WBMP  *************************/
	else if ((AcMemcmp(EncodedDataBuffer, gIfegWBMPHeader, WBMP_HEADER_LENGTH) == 0)
		&& (strcasecmp(fileExt, "wbmp") == 0)) {
		/* Parse BMP File and get image width and image height */
/*		if (DrmReadFile(hFile, &EncodedDataBuffer[2], 2, &fileread) == FALSE) {
			thumb_err("DrmReadFile was failed");
			return MS_MEDIA_ERR_FILE_READ_FAIL;
		}
		if (fileread < 2) {
			thumb_warn("IMG_CODEC_UNKNOWN_TYPE in WBMP");
			return ret;
		}*/
		if (pWidth) {
			*pWidth = EncodedDataBuffer[2];
		}
		if (pHeight) {
			*pHeight = EncodedDataBuffer[3];
		}
		thumb_dbg("WBMP w: %d, h: %d", *pWidth, *pHeight);

		thumb_dbg("IMG_CODEC_WBMP");
		*type = IMG_CODEC_WBMP;
	}
	return ret;
}

static int _ImgGetFileExt(const char *file_path, char *file_ext, int max_len)
{
	int i = 0;

	for (i = (int)strlen(file_path); i >= 0; i--) {
		if ((file_path[i] == '.') && (i < (int)strlen(file_path))) {
			strncpy(file_ext, &file_path[i + 1], max_len);
			return 0;
		}

		/* meet the dir. no ext */
		if (file_path[i] == '/') {
			return -1;
		}
	}

	return -1;
}

int ImgGetImageInfo(const char *filePath, ImgCodecType *type, unsigned int *width, unsigned int *height)
{
	HFile hFile;
	FmFileAttribute fileAttrib;
	char file_ext[10] = {0,};
	int err, ret = 0;

	SysAssert(filePath);
	hFile = DrmOpenFile(filePath);

	if (hFile == (HFile) INVALID_HOBJ) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	DrmGetFileAttributes(filePath, &fileAttrib);

	if ((fileAttrib.fileSize == 0)) {
		DrmCloseFile(hFile);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	err = _ImgGetFileExt(filePath, file_ext, sizeof(file_ext));
	if (err < 0) {
		thumb_warn("_media_thumb_get_file_ext failed");
	}

	ret = _ImgGetImageInfo(hFile, fileAttrib.fileSize, file_ext, type, width, height, FALSE);

	DrmSeekFile(hFile, SEEK_SET, 0);

	DrmCloseFile(hFile);

	return ret;

}

int ImgGetImageInfoForThumb(const char *filePath, ImgCodecType *type, unsigned int *width, unsigned int *height)
{
	HFile hFile;
	FmFileAttribute fileAttrib;
	char file_ext[10] = {0,};
	int err, ret = 0;

	SysAssert(filePath);
	hFile = DrmOpenFile(filePath);

	if (hFile == (HFile) INVALID_HOBJ) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	DrmGetFileAttributes(filePath, &fileAttrib);

	if ((fileAttrib.fileSize == 0)) {
		DrmCloseFile(hFile);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	err = _ImgGetFileExt(filePath, file_ext, sizeof(file_ext));
	if (err < 0) {
		thumb_warn("_media_thumb_get_file_ext failed");
	};

	ret = _ImgGetImageInfo(hFile, fileAttrib.fileSize, file_ext, type, width, height, TRUE);

	DrmSeekFile(hFile, SEEK_SET, 0);

	DrmCloseFile(hFile);

	return ret;

}

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

#ifndef _IMGCODEC_OSAL_H_
#define _IMGCODEC_OSAL_H_
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef BOOL

#define BOOL	bool//unsigned short
#endif

#ifndef UCHAR

#define UCHAR		unsigned char
#endif

#ifndef BYTE

#define BYTE			unsigned char
#endif

#ifndef USHORT

#define USHORT		unsigned short 
#endif

#ifndef UINT16

#define UINT16		unsigned short 
#endif

#ifndef UINT32

#define UINT32		unsigned short 
#endif

#ifndef UINT

#define UINT			unsigned int
#endif

#ifndef INT32

#define INT32		unsigned int 
#endif

#ifndef ULONG

#define ULONG		unsigned long 
#endif

#ifndef TRUE
#define TRUE		true
#endif

#ifndef FALSE
#define	FALSE 		false
#endif

#ifndef INVALID_HOBJ
//#define INVALID_HOBJ	(-1)
#define INVALID_HOBJ	NULL
#endif


typedef void*				HFile;

#define	AcMemalloc 		IfegMemAlloc	
#define 	AcMemfree		IfegMemFree
#define	AcMemcmp		IfegMemcmp
#define 	AcMemset		IfegMemset
#define 	AcMemcpy		IfegMemcpy

typedef struct
{
	ULONG		fileSize;				// File size
	ULONG*		startAddr;			// Only used at LFS
	ULONG		attribute;			// file attribute like directory or file, hidden, readonly, system, ...
	int			iVol;					// positioned volume
	ULONG		allocatedSize;		// real allocated size of file & directory in sub System
} FmFileAttribute;

void *	IfegMemAlloc(unsigned int size);
void 	IfegMemFree(void* pMem);
void *	IfegMemcpy( void *dest, const void *src, unsigned int count );
void *	IfegMemset( void *dest, int c, unsigned int count );
int 		IfegMemcmp(const void* pMem1, const void* pMem2, size_t length);

ULONG 	IfegGetAvailableMemSize(void);

HFile 	DrmOpenFile(const char* szPathName);
BOOL 	DrmReadFile(HFile hFile, void* pBuffer, ULONG bufLen, ULONG* pReadLen);
long 	DrmTellFile(HFile hFile);
BOOL 	DrmSeekFile(HFile hFile, long position, long offset);
BOOL 	DrmGetFileAttributes(const char* szPathName, FmFileAttribute* pFileAttr);
BOOL 	DrmCloseFile(HFile hFile);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif		// _IMGCODEC_OSAL_H_



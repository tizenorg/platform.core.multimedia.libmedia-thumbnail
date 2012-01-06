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

#include "img-codec-osal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <drm-service.h>

void *IfegMemAlloc(unsigned int size)
{
	void *pmem;
	pmem = malloc(size);
	return pmem;
}

void IfegMemFree(void *pMem)
{
	free(pMem);
	pMem = 0;
}

void *IfegMemcpy(void *dest, const void *src, unsigned int count)
{
	return memcpy(dest, src, count);
}

void *IfegMemset(void *dest, int c, unsigned int count)
{
	return memset(dest, c, count);
}

ULONG IfegGetAvailableMemSize(void)
{
	return 1;
}

int IfegMemcmp(const void *pMem1, const void *pMem2, size_t length)
{
	return memcmp(pMem1, pMem2, length);
}

DRM_FILE_HANDLE hDrmFile = NULL;
static BOOL _is_real_drm = FALSE;

HFile DrmOpenFile(const char *szPathName)
{
	if (drm_svc_is_drm_file(szPathName) == DRM_TRUE) {
		_is_real_drm = TRUE;
	} else {
		_is_real_drm = FALSE;
	}

	if (!_is_real_drm) {
		FILE *fp = fopen(szPathName, "rb");

		if (fp == NULL) {
			return (HFile) INVALID_HOBJ;
		}

		return fp;

	} else {
		int ret =
		    drm_svc_open_file(szPathName, DRM_PERMISSION_DISPLAY,
				      &hDrmFile);

		if (ret != DRM_RESULT_SUCCESS) {
			return (HFile) INVALID_HOBJ;
		}
		return hDrmFile;
	}
}

BOOL DrmReadFile(HFile hFile, void *pBuffer, ULONG bufLen, ULONG * pReadLen)
{
	size_t readCnt = -1;

	if (!_is_real_drm) {
		readCnt = fread(pBuffer, sizeof(char), bufLen, hFile);
		*pReadLen = (ULONG) readCnt;
	} else {
		drm_svc_read_file((DRM_FILE_HANDLE) hFile, pBuffer, bufLen,
				  &readCnt);
		*pReadLen = (ULONG) readCnt;
	}
	return TRUE;
}

long DrmTellFile(HFile hFile)
{
	if (!_is_real_drm) {
		return ftell(hFile);
	} else {
		return drm_svc_tell_file((DRM_FILE_HANDLE) hFile);
	}
}

BOOL DrmSeekFile(HFile hFile, long position, long offset)
{

	if (position < 0) {
		return FALSE;
	}
	if (!_is_real_drm) {
		fseek(hFile, offset, position);
	} else {
		drm_svc_seek_file((DRM_FILE_HANDLE) hFile, offset, position);
	}

	return TRUE;
}

BOOL DrmGetFileAttributes(const char *szPathName, FmFileAttribute * pFileAttr)
{
	FILE *f = NULL;

	f = fopen(szPathName, "r");

	if (f == NULL) {
		return FALSE;
	}

	fseek(f, 0, SEEK_END);
	pFileAttr->fileSize = ftell(f);
	fclose(f);

	return TRUE;
}

BOOL DrmCloseFile(HFile hFile)
{
	if (!_is_real_drm) {
		fclose(hFile);
	} else {
		drm_svc_close_file((DRM_FILE_HANDLE) hFile);
	}

	return TRUE;
}

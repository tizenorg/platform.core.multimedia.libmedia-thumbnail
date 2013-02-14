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
#include "img-codec-osal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <drm-service.h>
#include <drm_client.h>

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

static BOOL _is_real_drm = FALSE;

HFile DrmOpenFile(const char *szPathName)
{
	int ret = 0;
	drm_bool_type_e drm_type;

	ret = drm_is_drm_file(szPathName, &drm_type);
	if (ret < 0) {
		thumb_err("drm_is_drm_file falied : %d", ret);
		drm_type = DRM_FALSE;
	}

	if (drm_type == DRM_TRUE) {
		_is_real_drm = TRUE;
	} else {
		_is_real_drm = FALSE;
	}

	if (!_is_real_drm) {
		FILE *fp = fopen(szPathName, "rb");

		if (fp == NULL) {
			return (HFile) INVALID_HOBJ;
			thumb_err("file open error: %s", szPathName);
		}

		return fp;

	} else {
		return (HFile) INVALID_HOBJ;
	}
}

BOOL DrmReadFile(HFile hFile, void *pBuffer, ULONG bufLen, ULONG * pReadLen)
{
	size_t readCnt = -1;

	if (!_is_real_drm) {
		readCnt = fread(pBuffer, sizeof(char), bufLen, hFile);
		*pReadLen = (ULONG) readCnt;
	} else {
		return FALSE;
	}
	return TRUE;
}

long DrmTellFile(HFile hFile)
{
	if (!_is_real_drm) {
		return ftell(hFile);
	} else {
		return -1;
	}
}

BOOL DrmSeekFile(HFile hFile, long position, long offset)
{
	int ret = 0;

	if (position < 0) {
		return FALSE;
	}
	if (!_is_real_drm) {
		ret = fseek(hFile, offset, position);
		if (ret < 0) {
			thumb_err("fseek failed : %s", strerror(errno));
			return FALSE;
		}
	} else {
		return FALSE;
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
		return FALSE;
	}

	return TRUE;
}

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


#include "media-thumb-error.h"
#include "media-thumb-debug.h"
#include "media-thumb-types.h"
#include "media-thumb-internal.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifndef _MEDIA_THUMB_IPC_H_
#define _MEDIA_THUMB_IPC_H_

#define THUMB_DAEMON_PORT 10000
//#define MAX_PATH_SIZE 4096
#define MAX_PATH_SIZE 2048

#define TIMEOUT_SEC		10
#define MAX_TRIES		3

enum {
	THUMB_REQUEST_DB_INSERT,
	THUMB_REQUEST_SAVE_FILE,
	THUMB_REQUEST_ALL_MEDIA,
	THUMB_RESPONSE
};

enum {
	THUMB_SUCCESS,
	THUMB_FAIL
};

typedef struct _thumbMsg{
	int msg_type;
	media_thumb_type thumb_type;
	int status;
	int pid;
	int thumb_size;
	int thumb_width;
	int thumb_height;
	int origin_width;
	int origin_height;
	char org_path[MAX_PATH_SIZE];
	char dst_path[MAX_PATH_SIZE];
} thumbMsg;

int
_media_thumb_request(int msg_type,
					media_thumb_type thumb_type,
					const char *origin_path,
					char *thumb_path,
					int max_length,
					media_thumb_info *thumb_info);

int
_media_thumb_request_async(int msg_type,
					media_thumb_type thumb_type,
					const char *origin_path,
					thumbUserData *userData);

int
_media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg);

#endif /*_MEDIA_THUMB_IPC_H_*/

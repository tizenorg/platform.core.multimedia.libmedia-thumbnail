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


#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "media-thumb-internal.h"
#include "media-util-ipc.h"
#include "media-server-ipc.h"

#ifndef _MEDIA_THUMB_IPC_H_
#define _MEDIA_THUMB_IPC_H_

#define MAX_PATH_SIZE 4096
#define MAX_TRIES		3

enum {
	THUMB_REQUEST_DB_INSERT,
	THUMB_REQUEST_SAVE_FILE,
	THUMB_REQUEST_ALL_MEDIA,
	THUMB_REQUEST_CANCEL_MEDIA,
	THUMB_REQUEST_CANCEL_ALL,
	THUMB_REQUEST_KILL_SERVER,
	THUMB_RESPONSE,
	THUMB_REQUEST_RAW_DATA,
	THUMB_REQUEST_CANCEL_RAW_DATA,
	THUMB_REQUEST_CANCEL_ALL_RAW_DATA,
	THUMB_RESPONSE_RAW_DATA,
};

enum {
	THUMB_SUCCESS,
	THUMB_FAIL
};

int _media_thumb_recv_msg(int sock, int header_size, thumbMsg *msg);

int _media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size);

int _media_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size);

int _media_thumb_request(int msg_type,
					const char *origin_path,
					char *thumb_path,
					int max_length,
					media_thumb_info *thumb_info,
					uid_t uid);

int _media_thumb_request_async(int msg_type,
					unsigned int request_id,
					const char *origin_path,
					thumbUserData *userData,
					uid_t uid);

int _media_thumb_request_raw_data_async(int msg_type,
					int request_id,
					const char *origin_path,
					int width,
					int height,
					thumbRawUserData *userData,
					uid_t uid);

int _media_thumb_request_cancel_all(bool isRaw);

#endif /*_MEDIA_THUMB_IPC_H_*/

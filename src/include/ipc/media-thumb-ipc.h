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

#ifdef _USE_MEDIA_UTIL_
#include "media-util-ipc.h"
#include "media-server-ipc.h"
#endif

#ifdef _USE_UDS_SOCKET_
#include <sys/un.h>
#else
#include <sys/socket.h>
#endif

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#ifndef _MEDIA_THUMB_IPC_H_
#define _MEDIA_THUMB_IPC_H_

#ifndef _USE_MEDIA_UTIL_
#define THUMB_DAEMON_PORT 10000
#endif
#define MAX_PATH_SIZE 4096

#ifndef _USE_MEDIA_UTIL_
#define TIMEOUT_SEC		10
#endif

#define MAX_TRIES		3

enum {
	THUMB_REQUEST_DB_INSERT,
	THUMB_REQUEST_SAVE_FILE,
	THUMB_REQUEST_ALL_MEDIA,
	THUMB_REQUEST_CANCEL_MEDIA,
	THUMB_REQUEST_CANCEL_ALL,
	THUMB_REQUEST_KILL_SERVER,
	THUMB_RESPONSE
};

enum {
	THUMB_SUCCESS,
	THUMB_FAIL
};

#ifndef _USE_MEDIA_UTIL_
enum {
	CLIENT_SOCKET,
	SERVER_SOCKET
};

typedef struct _thumbMsg{
	int msg_type;
	media_thumb_type thumb_type;
	int status;
	int pid;
	uid_t uid;
	int thumb_size;
	int thumb_width;
	int thumb_height;
	int origin_width;
	int origin_height;
	int origin_path_size;
	int dest_path_size;
	char org_path[MAX_PATH_SIZE];
	char dst_path[MAX_PATH_SIZE];
} thumbMsg;
#endif

int
_media_thumb_create_socket(int sock_type, int *sock);

int
_media_thumb_create_udp_socket(int *sock);

int
_media_thumb_recv_msg(int sock, int header_size, thumbMsg *msg);

#ifdef _USE_UDS_SOCKET_
int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size);
#else
int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_in *from_addr, unsigned int *from_size);
#endif

int
_media_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size);

int
_media_thumb_request(int msg_type,
					media_thumb_type thumb_type,
					const char *origin_path,
					char *thumb_path,
					int max_length,
					media_thumb_info *thumb_info,
					uid_t uid);

int
_media_thumb_request_async(int msg_type,
					media_thumb_type thumb_type,
					const char *origin_path,
					thumbUserData *userData,
					uid_t uid);

int
_media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg, uid_t uid);

#endif /*_MEDIA_THUMB_IPC_H_*/

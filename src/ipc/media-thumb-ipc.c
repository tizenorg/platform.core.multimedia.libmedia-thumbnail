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

#include "media-thumbnail.h"
#include "media-thumb-ipc.h"
#include "media-thumb-util.h"
#include "media-thumb-db.h"
#include "media-thumb-debug.h"
#include <glib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>

#define THUMB_SOCK_BLOCK_SIZE 512
#define THUMB_IPC_PATH "/var/run/media-server/media_ipc_thumbcreator.socket"

static GQueue *g_request_queue = NULL;
static GQueue *g_manage_queue = NULL;
static GQueue *g_request_raw_queue = NULL;
static GQueue *g_manage_raw_queue = NULL;


typedef struct {
	GIOChannel *channel;
	int msg_type;
	bool isCanceled;
	int source_id;
	uid_t uid;
	char *path;
	thumbUserData *userData;
} thumbReq;

typedef struct {
	GIOChannel *channel;
	int msg_type;
	bool isCanceled;
	int request_id;
	int source_id;
	int width;
	int height;
	uid_t uid;
	char *path;
	thumbRawUserData *userData;
} thumbRawReq;

int _media_thumb_send_request();
int _media_thumb_raw_data_send_request();


int _media_thumb_get_error()
{
	if (errno == EWOULDBLOCK) {
		thumb_err("Timeout. Can't try any more");
		return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
	} else {
		thumb_stderror("recvfrom failed");
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}
}

void __media_thumb_shutdown_channel(bool only_shutdown)
{
	int len = -1;
	thumbReq *req = (thumbReq *)g_queue_peek_head(g_request_queue);

	len = g_queue_get_length(g_manage_queue);

	if (req != NULL) {
		if (!only_shutdown) {
			if (len == 0 && req->isCanceled == true) {
				thumb_dbg("The manage queue will be released");
			} else {
				thumb_dbg("There is remain item in the manage queue");
				return;
			}
		}
		GSource *source_id = g_main_context_find_source_by_id(g_main_context_get_thread_default(), req->source_id);
		if (source_id != NULL) {
			g_source_destroy(source_id);
		} else {
			thumb_err("G_SOURCE_ID is NULL");
		}

		g_io_channel_shutdown(req->channel, TRUE, NULL);
		g_io_channel_unref(req->channel);
		g_queue_pop_head(g_request_queue);

		SAFE_FREE(req->path);
		SAFE_FREE(req->userData);
		SAFE_FREE(req);

		if(g_manage_queue && len == 0) {
			g_queue_free(g_manage_queue);
			g_manage_queue = NULL;
		}
	}
}

int __media_thumb_pop_req_queue(const char *path)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {
		__media_thumb_shutdown_channel(true);
	}

	/* Check manage queue */
	if(g_manage_queue) {
		req_len = g_queue_get_length(g_manage_queue);

		if(req_len > 0)
			_media_thumb_send_request();
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_thumb_check_req_queue_for_cancel(const char *path)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {
		thumbReq *req = NULL;
		req = (thumbReq *)g_queue_peek_head(g_request_queue);

		if (req != NULL && strncmp(path, req->path, strlen(path)) == 0) {
			req->isCanceled = true;
			__media_thumb_shutdown_channel(false);

			return MS_MEDIA_ERR_NONE;
		}
	}

	return MS_MEDIA_ERR_INTERNAL;
}

int __media_thumb_pop_manage_queue(const char *path)
{
	int req_len = 0, i;
	bool flag = false;

	req_len = g_queue_get_length(g_manage_queue);

	if (req_len < 0) {
//		thumb_dbg("There is no request in the queue");
	} else {
		for (i = 0; i < req_len; i++) {
			thumbReq *req = NULL;
			req = (thumbReq *)g_queue_peek_nth(g_manage_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				g_queue_pop_nth(g_manage_queue, i);

				SAFE_FREE(req->path);
				SAFE_FREE(req->userData);
				SAFE_FREE(req);
				flag = true;
				break;
			}
		}
		if (!flag) {
			return __media_thumb_check_req_queue_for_cancel(path);
		} else {
			__media_thumb_shutdown_channel(false);

			return MS_MEDIA_ERR_NONE;
		}
	}

	return MS_MEDIA_ERR_NONE;
}


int __media_thumb_pop_raw_data_req_queue(int request_id, bool shutdown_channel)
{
	int req_len = 0, i;

	req_len = g_queue_get_length(g_request_raw_queue);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			thumbRawReq *req = NULL;
			req = (thumbRawReq *)g_queue_peek_nth(g_request_raw_queue, i);
			if (req == NULL) continue;

			if (request_id == req->request_id) {
				if (shutdown_channel) {
					GSource *source_id = g_main_context_find_source_by_id(g_main_context_get_thread_default(), req->source_id);
					if (source_id != NULL) {
						g_source_destroy(source_id);
					} else {
						thumb_err("G_SOURCE_ID is NULL");
					}

					g_io_channel_shutdown(req->channel, TRUE, NULL);
					g_io_channel_unref(req->channel);
				}
				g_queue_pop_nth(g_request_raw_queue, i);

				SAFE_FREE(req->userData);
				SAFE_FREE(req);

				break;
			}
		}
		if (i == req_len) {
//			thumb_dbg("There's no %s in the queue", path);
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_thumb_check_raw_data_req_queue_for_cancel(int request_id)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_raw_queue);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {
		thumbRawReq *req = NULL;
		req = (thumbRawReq *)g_queue_peek_head(g_request_raw_queue);

		if (req != NULL && request_id == req->request_id) {
			req->isCanceled = true;
			return MS_MEDIA_ERR_NONE;
		}
	}

	return MS_MEDIA_ERR_INTERNAL;
}

int __media_thumb_pop_raw_data_manage_queue(int request_id)
{
	int req_len = 0, i;
	int flag = false;

	req_len = g_queue_get_length(g_manage_raw_queue);

	if (req_len < 0) {
//		thumb_dbg("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			thumbRawReq *req = NULL;
			req = (thumbRawReq *)g_queue_peek_nth(g_manage_raw_queue, i);
			if (req == NULL) continue;

			if (request_id == req->request_id) {
				g_queue_pop_nth(g_manage_raw_queue, i);

				SAFE_FREE(req->userData);
				SAFE_FREE(req);
				flag = true;
				break;
			}
		}
		if (!flag) {
			return __media_thumb_check_raw_data_req_queue_for_cancel(request_id);
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_thumb_check_req_queue(const char *path)
{
	int req_len = 0, i;

	req_len = g_queue_get_length(g_request_queue);

//	thumb_dbg("Queue length : %d", req_len);
//	thumb_dbg("Queue path : %s", path);

	if (req_len <= 0) {
//		thumb_dbg("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			thumbReq *req = NULL;
			req = (thumbReq *)g_queue_peek_nth(g_request_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				//thumb_dbg("Same Request - %s", path);
				return MS_MEDIA_ERR_INVALID_PARAMETER;

				break;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

bool __media_thumb_check_cancel(void)
{
	thumbReq *req = NULL;
	req = (thumbReq *)g_queue_peek_head(g_request_queue);

	if (req == NULL) {
		return false;
	} else {
		if (req->isCanceled)
			return false;
		else
			return true;
	}
}

bool __media_thumb_check_cancel_for_raw(void)
{
	thumbRawReq *req = NULL;
	req = (thumbRawReq *)g_queue_peek_head(g_request_raw_queue);

	if (req == NULL) {
		return false;
	} else {
		if (req->isCanceled)
			return false;
		else
			return true;
	}
}

int
_media_thumb_recv_msg(int sock, int header_size, thumbMsg *msg)
{
	int recv_msg_len = 0;
	int remain_size = 0;
	int block_size = THUMB_SOCK_BLOCK_SIZE;
	int recv_block = 0;
	unsigned char *buf = NULL;
	unsigned char *block_buf = NULL;

	THUMB_MALLOC(buf, header_size);
	THUMB_MALLOC(block_buf, THUMB_SOCK_BLOCK_SIZE);
	if (buf == NULL || block_buf == NULL) {
		thumb_err("memory allocation failed");
		SAFE_FREE(buf);
		SAFE_FREE(block_buf);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recv(sock, buf, header_size, 0)) <= 0) {
		thumb_stderror("recv failed");
		SAFE_FREE(buf);
		SAFE_FREE(block_buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);
	//thumb_dbg("origin_path_size : %d, dest_path_size : %d, thumb_size : %d", msg->origin_path_size, msg->dest_path_size, msg->thumb_size);

	SAFE_FREE(buf);
	if (msg->origin_path_size < 0 || msg->dest_path_size < 0 || msg->thumb_size < 0) {
		thumb_err("recv data is wrong");
		SAFE_FREE(block_buf);
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	remain_size = msg->origin_path_size + msg->dest_path_size + msg->thumb_size;
	THUMB_MALLOC(buf, remain_size);
	if (buf == NULL) {
		SAFE_FREE(block_buf);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	while (remain_size > 0) {
		if (remain_size < THUMB_SOCK_BLOCK_SIZE) {
			block_size = remain_size;
		}
		if ((recv_msg_len = recv(sock, block_buf, block_size, 0)) < 0) {
			thumb_stderror("recv failed");
			SAFE_FREE(buf);
			SAFE_FREE(block_buf);
			return _media_thumb_get_error();
		}
		memcpy(buf+recv_block, block_buf, block_size);
		recv_block += block_size;
		remain_size -= block_size;
	}

	strncpy(msg->org_path, (char *)buf, msg->origin_path_size);
	strncpy(msg->dst_path, (char *)buf + msg->origin_path_size, msg->dest_path_size);

	SAFE_FREE(msg->thumb_data);
	if (msg->thumb_size > 0) {
		THUMB_MALLOC(msg->thumb_data, msg->thumb_size);
		if (msg->thumb_data != NULL) {
			memcpy(msg->thumb_data, buf + msg->origin_path_size + msg->dest_path_size, msg->thumb_size);
		} else {
			SAFE_FREE(buf);
			SAFE_FREE(block_buf);

			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
	}

	SAFE_FREE(buf);
	SAFE_FREE(block_buf);

	return MS_MEDIA_ERR_NONE;
}

int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size)
{
	int recv_msg_len = 0;
	unsigned int from_addr_size = sizeof(struct sockaddr_un);
	unsigned char *buf = NULL;

	THUMB_MALLOC(buf, sizeof(thumbMsg));
	if (buf == NULL) {
		thumb_err("memory allocation failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recvfrom(sock, buf, sizeof(thumbMsg), 0, (struct sockaddr *)from_addr, &from_addr_size)) < 0) {
		thumb_stderror("recvform failed");
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);

	if (msg->origin_path_size <= 0 || msg->origin_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->origin_path_size);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	strncpy(msg->org_path, (char*)buf + header_size, msg->origin_path_size);

	if (msg->dest_path_size <= 0 || msg->dest_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->dest_path_size);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	strncpy(msg->dst_path, (char*)buf + header_size + msg->origin_path_size, msg->dest_path_size);

	SAFE_FREE(buf);
	*from_size = from_addr_size;

	return MS_MEDIA_ERR_NONE;
}

int
_media_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size)
{
	if (req_msg == NULL || buf == NULL) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	int org_path_len = 0;
	int dst_path_len = 0;
	int thumb_data_len = 0;
	int size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) -(MAX_FILEPATH_LEN * 2) - sizeof(unsigned char *);
	org_path_len = req_msg->origin_path_size;
	dst_path_len = req_msg->dest_path_size;
	thumb_data_len = req_msg->thumb_size;

	//thumb_dbg("Basic Size : %d, org_path : %s[%d], dst_path : %s[%d], thumb_data_len : %d", header_size, req_msg->org_path, org_path_len, req_msg->dst_path, dst_path_len, thumb_data_len);

	size = header_size + org_path_len + dst_path_len + thumb_data_len;
	THUMB_MALLOC(*buf, size);
	if (*buf == NULL) {
		*buf_size = 0;
		return 0;
	}
	memcpy(*buf, req_msg, header_size);
	memcpy((*buf)+header_size, req_msg->org_path, org_path_len);
	memcpy((*buf)+header_size + org_path_len, req_msg->dst_path, dst_path_len);
	if (thumb_data_len > 0)
		memcpy((*buf)+header_size + org_path_len + dst_path_len, req_msg->thumb_data, thumb_data_len);

	*buf_size = size;

	return MS_MEDIA_ERR_NONE;
}

int
_media_thumb_request(int msg_type, const char *origin_path, char *thumb_path, int max_length, media_thumb_info *thumb_info, uid_t uid)
{
	int sock = -1;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	int recv_str_len = 0;
	int err = MS_MEDIA_ERR_NONE;
	int pid;
	sock_info.port = MS_THUMB_CREATOR_PORT;

	err = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("ms_ipc_create_client_socket failed");
		return err;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, THUMB_IPC_PATH, strlen(THUMB_IPC_PATH));

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_stderror("connect");
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	thumbMsg req_msg;
	thumbMsg recv_msg;

	memset((void *)&req_msg, 0, sizeof(thumbMsg));
	memset((void *)&recv_msg, 0, sizeof(thumbMsg));

	/* Get PID of client*/
	pid = getpid();
	req_msg.pid = pid;

	/* Set requset message */
	req_msg.msg_type = msg_type;
	req_msg.uid = uid;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(req_msg.dst_path, thumb_path, sizeof(req_msg.dst_path));
		req_msg.dst_path[strlen(req_msg.dst_path)] = '\0';
	}

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = strlen(req_msg.dst_path) + 1;
	req_msg.thumb_size = 0;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2 - sizeof(unsigned char *);
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d", errno);
		SAFE_FREE(buf);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	thumb_dbg("Sending msg to thumbnail daemon is successful");

	SAFE_FREE(buf);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		ms_ipc_delete_client_socket(&sock_info);
		return err;
	}

	recv_str_len = strlen(recv_msg.org_path);
	thumb_dbg_slog("recv %s(%d) from thumb daemon is successful", recv_msg.org_path, recv_str_len);

	ms_ipc_delete_client_socket(&sock_info);

	if (recv_str_len > max_length) {
		thumb_err("user buffer is too small. Output's length is %d", recv_str_len);
		SAFE_FREE(recv_msg.thumb_data);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		SAFE_FREE(recv_msg.thumb_data);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (msg_type != THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, recv_msg.dst_path, max_length);
	}

	thumb_info->origin_width = recv_msg.origin_width;
	thumb_info->origin_height = recv_msg.origin_height;

	SAFE_FREE(recv_msg.thumb_data);

	return MS_MEDIA_ERR_NONE;
}

gboolean _media_thumb_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	thumbMsg recv_msg;
	int header_size = 0;
	int sock = 0;
	int err = MS_MEDIA_ERR_NONE;

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	sock = g_io_channel_unix_get_fd(src);

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2 - sizeof(unsigned char *);

	thumb_err("_media_thumb_write_socket socket : %d", sock);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		if (recv_msg.origin_path_size > 0) {
			__media_thumb_pop_req_queue(recv_msg.org_path);
		} else {
			thumb_err("origin path size is wrong.");
		}

		return FALSE;
	}

	//thumb_dbg("Completed..%s", recv_msg.org_path);

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		err = MS_MEDIA_ERR_INTERNAL;
	}

	if (__media_thumb_check_cancel()) {
		if (data) {
			thumbUserData* cb = (thumbUserData*)data;
			if (cb->func != NULL)
				cb->func(err, recv_msg.dst_path, cb->user_data);
		}
	}

	__media_thumb_pop_req_queue(recv_msg.org_path);

	thumb_dbg("Done");

	return FALSE;
}

gboolean _media_thumb_raw_data_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	thumbMsg recv_msg;
	int header_size = 0;
	int sock = 0;
	int len = -1;
	int err = MS_MEDIA_ERR_NONE;

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	sock = g_io_channel_unix_get_fd(src);

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2 - sizeof(unsigned char *);

	thumb_err("_media_thumb_write_socket socket : %d", sock);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		if (recv_msg.request_id > 0) {
			__media_thumb_pop_raw_data_req_queue(recv_msg.request_id, TRUE);
		} else {
			thumb_err("origin path size is wrong.");
		}

		return FALSE;
	}

	g_io_channel_shutdown(src, TRUE, NULL);
	g_io_channel_unref(src);

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		err = MS_MEDIA_ERR_INTERNAL;
	}

	if (__media_thumb_check_cancel_for_raw()) {
		if (data) {
			thumbRawUserData* cb = (thumbRawUserData*)data;
			if (cb->func != NULL)
				cb->func(err, recv_msg.request_id, recv_msg.org_path, recv_msg.thumb_width, recv_msg.thumb_height, recv_msg.thumb_data, recv_msg.thumb_size, cb->user_data);
		}
	}

	__media_thumb_pop_raw_data_req_queue(recv_msg.request_id, FALSE);

	thumb_dbg("Done");

	SAFE_FREE(recv_msg.thumb_data);

	/* Check manage queue */
	if (g_manage_raw_queue) {
		len = g_queue_get_length(g_manage_raw_queue);

		if (len > 0) {
			_media_thumb_raw_data_send_request();
		} else {
			g_queue_free(g_manage_raw_queue);
			g_manage_raw_queue = NULL;
		}
	}

	return FALSE;
}

int _media_thumb_send_request()
{
	int err = MS_MEDIA_ERR_NONE;
	int sock = -1;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	thumbReq *req_manager = NULL;
	int pid;
	sock_info.port = MS_THUMB_CREATOR_PORT;

	err = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("ms_ipc_create_client_socket failed");
		return err;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, THUMB_IPC_PATH, strlen(THUMB_IPC_PATH));

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	int source_id = -1;

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_stderror("connect");
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}
	req_manager = (thumbReq *)g_queue_pop_head(g_manage_queue);
	GSource *source = NULL;
	source = g_io_create_watch(channel, G_IO_IN);
	g_source_set_callback(source, (GSourceFunc)_media_thumb_write_socket, req_manager->userData, NULL);
	source_id = g_source_attach(source, g_main_context_get_thread_default());

	thumbMsg req_msg;

	memset((void *)&req_msg, 0, sizeof(thumbMsg));

	pid = getpid();
	req_msg.pid = pid;
	req_msg.msg_type = req_manager->msg_type;
	req_msg.request_id = 0;
	req_msg.uid = req_manager->uid;
	strncpy(req_msg.org_path, req_manager->path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';
	req_msg.dst_path[0] = '\0';
	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = 1;
	req_msg.thumb_size = 0;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d", errno);
		SAFE_FREE(buf);
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), source_id));
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	SAFE_FREE(buf);
	thumb_dbg("Sending msg to thumbnail daemon is successful");


	if (req_manager->msg_type == THUMB_REQUEST_DB_INSERT) {
		if (g_request_queue == NULL) {
			g_request_queue = g_queue_new();
		}

		thumbReq *thumb_req = NULL;
		THUMB_MALLOC(thumb_req, sizeof(thumbReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}

		thumb_req->channel = channel;
		thumb_req->path = strdup(req_manager->path);
		thumb_req->source_id = source_id;
		thumb_req->userData = req_manager->userData;

		g_queue_push_tail(g_request_queue, (gpointer)thumb_req);
	}

	return err;
}

int _media_thumb_raw_data_send_request()
{
	int err = MS_MEDIA_ERR_NONE;
	int sock = -1;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	thumbRawReq *req_manager = NULL;
	int pid;
	sock_info.port = MS_THUMB_CREATOR_PORT;

	err = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("ms_ipc_create_client_socket failed");
		return err;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, THUMB_IPC_PATH, strlen(THUMB_IPC_PATH));

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	int source_id = -1;

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_stderror("connect error");
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	req_manager = (thumbRawReq *)g_queue_pop_head(g_manage_raw_queue);
	GSource *source = NULL;
	source = g_io_create_watch(channel, G_IO_IN);
	g_source_set_callback(source, (GSourceFunc)_media_thumb_raw_data_write_socket, req_manager->userData, NULL);
	source_id = g_source_attach(source, g_main_context_get_thread_default());

	thumbMsg req_msg;
	memset((void *)&req_msg, 0, sizeof(thumbMsg));

	pid = getpid();
	req_msg.pid = pid;
	req_msg.msg_type = req_manager->msg_type;
	req_msg.request_id = req_manager->request_id;
	req_msg.thumb_width = req_manager->width;
	req_msg.thumb_height = req_manager->height;
	req_msg.uid = req_manager->uid;

	strncpy(req_msg.org_path, req_manager->path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';
	req_msg.dst_path[0] = '\0';

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = 1;
	req_msg.thumb_size = 0;

	if (req_msg.origin_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d", errno);
		SAFE_FREE(buf);
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), source_id));
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	SAFE_FREE(buf);

	if (req_manager->msg_type == THUMB_REQUEST_RAW_DATA) {
		if (g_request_raw_queue == NULL) {
			g_request_raw_queue = g_queue_new();
		}
		thumbRawReq *thumb_req = NULL;
		THUMB_MALLOC(thumb_req, sizeof(thumbRawReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}
		thumb_req->channel = channel;
		thumb_req->request_id = req_manager->request_id;
		thumb_req->source_id = source_id;
		thumb_req->userData = req_manager->userData;

		g_queue_push_tail(g_request_raw_queue, (gpointer)thumb_req);
	}
	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_request_async(int msg_type, const char *origin_path, thumbUserData *userData, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	if (g_manage_queue == NULL) {
		if (msg_type == THUMB_REQUEST_CANCEL_MEDIA)
			return MS_MEDIA_ERR_INTERNAL;

		g_manage_queue = g_queue_new();

		thumbReq *thumb_req = NULL;
		THUMB_MALLOC(thumb_req, sizeof(thumbReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}

		thumb_req->msg_type = msg_type;
		thumb_req->path = strdup(origin_path);
		thumb_req->userData = userData;
		thumb_req->isCanceled = false;
		thumb_req->uid = uid;

		g_queue_push_tail(g_manage_queue, (gpointer)thumb_req);

		/* directly request at first time */
		err = _media_thumb_send_request();

	} else {
		if (msg_type != THUMB_REQUEST_CANCEL_MEDIA) {
			/* Enqueue */
			if ((msg_type == THUMB_REQUEST_DB_INSERT) && (__media_thumb_check_req_queue(origin_path) < 0)) {
				thumb_err("duplicated request");
				return MS_MEDIA_ERR_THUMB_DUPLICATED_REQUEST;
			}

			thumbReq *thumb_req = NULL;
			THUMB_MALLOC(thumb_req, sizeof(thumbReq));
			if (thumb_req == NULL) {
				thumb_err("Failed to create request element");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}

			thumb_req->msg_type = msg_type;
			thumb_req->path = strdup(origin_path);
			thumb_req->userData = userData;
			thumb_req->isCanceled = false;
			thumb_req->uid = uid;

			g_queue_push_tail(g_manage_queue, (gpointer)thumb_req);
		} else {
			/* Dequeue */
			err = __media_thumb_pop_manage_queue(origin_path);
		}
	}

	return err;
}

int _media_thumb_request_raw_data_async(int msg_type, int request_id, const char *origin_path, int width, int height, thumbRawUserData *userData, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	if (g_manage_raw_queue == NULL) {
		if (msg_type == THUMB_REQUEST_CANCEL_RAW_DATA)
			return MS_MEDIA_ERR_INTERNAL;

		g_manage_raw_queue = g_queue_new();

		thumbRawReq *thumb_req = NULL;
		THUMB_MALLOC(thumb_req, sizeof(thumbRawReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}

		thumb_req->msg_type = msg_type;
		thumb_req->request_id = request_id;
		thumb_req->path = strdup(origin_path);
		thumb_req->width = width;
		thumb_req->height = height;
		thumb_req->userData = userData;
		thumb_req->isCanceled = false;
		thumb_req->uid = uid;

		g_queue_push_tail(g_manage_raw_queue, (gpointer)thumb_req);

		/* directly request at first time */
		err = _media_thumb_raw_data_send_request();

	} else {
		if (msg_type != THUMB_REQUEST_CANCEL_RAW_DATA) {
			/* Enqueue */
			thumbRawReq *thumb_req = NULL;
			THUMB_MALLOC(thumb_req, sizeof(thumbRawReq));
			if (thumb_req == NULL) {
				thumb_err("Failed to create request element");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}

			thumb_req->msg_type = msg_type;
			thumb_req->request_id = request_id;
			thumb_req->path = strdup(origin_path);
			thumb_req->width = width;
			thumb_req->height = height;
			thumb_req->userData = userData;
			thumb_req->isCanceled = false;
			thumb_req->uid = uid;

			g_queue_push_tail(g_manage_raw_queue, (gpointer)thumb_req);

		} else {
			/* Dequeue */
			err = __media_thumb_pop_raw_data_manage_queue(request_id);
		}
	}

	return err;
}

int _media_thumb_request_cancel_all(bool isRaw)
{
	int i;
	int req_len = -1;
	int len = 0;

	if (isRaw) {
		thumbRawReq *tmp_raw_req = NULL;
		if (g_manage_raw_queue == NULL) {
			thumb_err("manage_queue is NULL");
			if (g_request_raw_queue != NULL) {
				/* Check request queue */
				len = g_queue_get_length(g_request_raw_queue);
				if (len > 0) {
					tmp_raw_req = g_queue_peek_head(g_request_raw_queue);
					if (tmp_raw_req != NULL)
						tmp_raw_req->isCanceled = true;
					return MS_MEDIA_ERR_NONE;
				}
			}
			thumb_err("request_queue is NULL");
			return MS_MEDIA_ERR_INTERNAL;
		}
		req_len = g_queue_get_length(g_manage_raw_queue);
		for (i = 0; i < req_len; i++) {
			tmp_raw_req = g_queue_pop_tail(g_manage_raw_queue);
			if (tmp_raw_req) {
				SAFE_FREE(tmp_raw_req->path);
				SAFE_FREE(tmp_raw_req->userData);
				SAFE_FREE(tmp_raw_req);
				tmp_raw_req = NULL;
			}
		}
		g_queue_free(g_manage_raw_queue);
		g_manage_raw_queue = NULL;
		if (g_request_raw_queue != NULL) {
			/* Check request queue */
			len = g_queue_get_length(g_request_raw_queue);
			if (len > 0) {
				tmp_raw_req = g_queue_peek_head(g_request_raw_queue);
				if (tmp_raw_req != NULL)
					tmp_raw_req->isCanceled = true;
			}
		}
	} else {
		thumbReq *tmp_req = NULL;
		if (g_manage_queue == NULL) {
			thumb_err("manage_queue is NULL");
			if (g_request_queue != NULL) {
				/* Check request queue */
				len = g_queue_get_length(g_request_queue);
				if (len > 0) {
					tmp_req = g_queue_peek_head(g_request_queue);
					if (tmp_req != NULL)
						tmp_req->isCanceled = true;
					return MS_MEDIA_ERR_NONE;
				}
			}
			thumb_err("request_queue is NULL");
			return MS_MEDIA_ERR_INTERNAL;
		}
		req_len = g_queue_get_length(g_manage_queue);
		for (i = 0; i < req_len; i++) {
			tmp_req = g_queue_pop_tail(g_manage_queue);
			if (tmp_req) {
				SAFE_FREE(tmp_req->path);
				SAFE_FREE(tmp_req->userData);
				SAFE_FREE(tmp_req);
				tmp_req = NULL;
			}
		}
		g_queue_free(g_manage_queue);
		g_manage_queue = NULL;
		if (g_request_queue != NULL) {
			/* Check request queue */
			len = g_queue_get_length(g_request_queue);
			if (len > 0) {
				tmp_req = g_queue_peek_head(g_request_queue);
				if (tmp_req != NULL)
					tmp_req->isCanceled = true;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}
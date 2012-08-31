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
#include <glib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static __thread int g_tries = 0;

int
_media_thumb_request(int msg_type, media_thumb_type thumb_type, const char *origin_path, char *thumb_path, int max_length, media_thumb_info *thumb_info)
{
	int sock;
	const char *serv_ip = "127.0.0.1";
	struct sockaddr_in serv_addr;
	struct sockaddr_in from_addr;

	unsigned int from_size;

	int send_str_len = strlen(origin_path);
	int recv_msg_len;
	int recv_str_len;
	int pid;

	thumbMsg req_msg;
	thumbMsg recv_msg;

	memset((void *)&req_msg, 0, sizeof(thumbMsg));
	memset((void *)&recv_msg, 0, sizeof(thumbMsg));

	req_msg.msg_type = msg_type;
	req_msg.thumb_type = thumb_type;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(req_msg.dst_path, thumb_path, sizeof(req_msg.dst_path));
		req_msg.dst_path[strlen(req_msg.dst_path)] = '\0';
	}

	struct timeval tv_timeout = { TIMEOUT_SEC, 0 };

	if (send_str_len > MAX_PATH_SIZE) {
		thumb_err("original path's length exceeds %d(max packet size)", MAX_PATH_SIZE);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	pid = getpid();
	req_msg.pid = pid;

	/* Creaete a datagram/UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		thumb_err("socket failed: %d\n", errno);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
		thumb_err("setsockopt failed: %d\n", errno);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
	serv_addr.sin_port = htons(THUMB_DAEMON_PORT);

	if (sendto(sock, &req_msg, sizeof(req_msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(req_msg)) {
		thumb_err("sendto failed: %d\n", errno);
		close(sock);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	thumb_dbg("Sending msg to thumbnail daemon is successful");

	/* recv a response */
	from_size = sizeof(from_addr);

	while(1) {
		if ((recv_msg_len = recvfrom(sock, &recv_msg, sizeof(recv_msg), 0, (struct sockaddr *)&from_addr, &from_size)) < 0) {

			if (errno == EWOULDBLOCK) {
#ifdef DO_RETRY
				thumb_warn("Timeout..%d more tries", MAX_TRIES - g_tries);
				if (g_tries < MAX_TRIES) {
					if (sendto(sock, &req_msg, sizeof(req_msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(req_msg)) {
						thumb_err("sendto failed: %d\n", errno);
						close(sock);
						g_tries = 0;
						return MEDIA_THUMB_ERROR_NETWORK;
					}
				} else {
					thumb_err("Timeout. Can't try any more");
					close(sock);
					g_tries = 0;
					return MEDIA_THUMB_ERROR_TIMEOUT;
				}
				g_tries++;
#else
				thumb_warn("Timeout. Can't try any more");
				g_tries = 0;
				close(sock);
				return MEDIA_THUMB_ERROR_TIMEOUT;
#endif
			} else {
				thumb_err("recvfrom failed");
				g_tries = 0;
				close(sock);
				return MEDIA_THUMB_ERROR_NETWORK;
			}
		} else {
			thumb_dbg("recvfrom done");
			break;
		}
	}

	g_tries = 0;

	if (serv_addr.sin_addr.s_addr != from_addr.sin_addr.s_addr) {
		thumb_err("addr is different");
		close(sock);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	recv_msg.org_path[MAX_PATH_SIZE - 1] = '\0';
	recv_str_len = strlen(recv_msg.org_path);
	thumb_dbg("recv %s(%d) from thumb daemon is successful", recv_msg.org_path, recv_str_len);

	close(sock);

	if (recv_str_len > max_length) {
		thumb_err("user buffer is too small. Output's length is %d", recv_str_len);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		return -1;
	}

	if (msg_type != THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, recv_msg.dst_path, max_length);
	}

	thumb_info->origin_width = recv_msg.origin_width;
	thumb_info->origin_height = recv_msg.origin_height;

	return 0;
}

int
_media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg)
{
	int err = -1;
	unsigned char *data = NULL;
	int thumb_size = 0;
	int thumb_w = 0;
	int thumb_h = 0;
	int origin_w = 0;
	int origin_h = 0;
	int max_length = 0;
	char *thumb_path = NULL;
	int need_update_db = 0;

	if (req_msg == NULL || res_msg == NULL) {
		thumb_err("Invalid msg!");
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	int msg_type = req_msg->msg_type;
	media_thumb_type thumb_type = req_msg->thumb_type;
	const char *origin_path = req_msg->org_path;

	media_thumb_format thumb_format = MEDIA_THUMB_BGRA;

	thumb_path = res_msg->dst_path;
	thumb_path[0] = '\0';
	max_length = sizeof(res_msg->dst_path);

	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (msg_type == THUMB_REQUEST_DB_INSERT) {
		err = _media_thumb_get_thumb_from_db_with_size(origin_path, thumb_path, max_length, &need_update_db, &origin_w, &origin_h);
		if (err == 0) {
			res_msg->origin_width = origin_w;
			res_msg->origin_width = origin_h;
			_media_thumb_db_disconnect();
			return MEDIA_THUMB_ERROR_NONE;
		} else {
			if (strlen(thumb_path) == 0) {
				err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length);
				if (err < 0) {
					thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
					strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
					_media_thumb_db_disconnect();
					return err;
				}

				thumb_path[strlen(thumb_path)] = '\0';
			}
		}

	} else if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, req_msg->dst_path, max_length);

	} else if (msg_type == THUMB_REQUEST_ALL_MEDIA) {
		err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length);
		if (err < 0) {
			thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
			strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
			_media_thumb_db_disconnect();
			return err;
		}

		thumb_path[strlen(thumb_path)] = '\0';
	}

	thumb_dbg("Thumb path : %s", thumb_path);

	if (g_file_test(thumb_path, 
					G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_warn("thumb path already exists in file system.. remove the existed file");
		_media_thumb_remove_file(thumb_path);
	}

	err = _thumbnail_get_data(origin_path, thumb_type, thumb_format, &data, &thumb_size, &thumb_w, &thumb_h, &origin_w, &origin_h);
	if (err < 0) {
		thumb_err("_thumbnail_get_data failed - %d\n", err);
		SAFE_FREE(data);

		strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);
		_media_thumb_db_disconnect();
		return err;
	}

	thumb_dbg("Size : %d, W:%d, H:%d", thumb_size, thumb_w, thumb_h);
	thumb_dbg("Origin W:%d, Origin H:%d\n", origin_w, origin_h);
	thumb_dbg("Thumb : %s", thumb_path);

	res_msg->msg_type = THUMB_RESPONSE;
	res_msg->thumb_size = thumb_size;
	res_msg->thumb_width = thumb_w;
	res_msg->thumb_height = thumb_h;
	res_msg->origin_width = origin_w;
	res_msg->origin_height = origin_h;

	err = _media_thumb_save_to_file_with_evas(data, thumb_w, thumb_h, thumb_path);
	if (err < 0) {
		thumb_err("save_to_file_with_evas failed - %d\n", err);
		SAFE_FREE(data);

		if (msg_type == THUMB_REQUEST_DB_INSERT || msg_type == THUMB_REQUEST_ALL_MEDIA)
			strncpy(thumb_path, THUMB_DEFAULT_PATH, max_length);

		_media_thumb_db_disconnect();
		return err;
	} else {
		thumb_dbg("file save success\n");
	}

	/* fsync */
	int fd = 0;
	fd = open(thumb_path, O_WRONLY);
	if (fd < 0) {
		thumb_warn("open failed");
	} else {
		err = fsync(fd);
		if (err == -1) {
			thumb_warn("fsync failed");
		}

		close(fd);
	}
	/* End of fsync */

	SAFE_FREE(data);

	/* DB update if needed */
	if (need_update_db == 1) {
		err = _media_thumb_update_db(origin_path, thumb_path, res_msg->origin_width, res_msg->origin_height);
		if (err < 0) {
			thumb_err("_media_thumb_update_db failed : %d", err);
		}
	}

	_media_thumb_db_disconnect();

	return 0;
}

gboolean _media_thumb_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	thumb_err("_media_thumb_write_socket is called");

	thumbMsg recv_msg;
	int recv_msg_len;
	int recv_str_len;
	int sock = 0;
	int err = MEDIA_THUMB_ERROR_NONE;
	unsigned int from_size;

	struct sockaddr_in from_addr;

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	sock = g_io_channel_unix_get_fd(src);

	/* recv a response */
	from_size = sizeof(from_addr);

	if ((recv_msg_len = recvfrom(sock, &recv_msg, sizeof(recv_msg), 0, (struct sockaddr *)&from_addr, &from_size)) < 0) {

		if (errno == EWOULDBLOCK) {
			thumb_warn("Timeout. Can't try any more");
			g_io_channel_shutdown(src, TRUE, NULL);
			err = MEDIA_THUMB_ERROR_TIMEOUT;
			goto callback;
		} else {
			thumb_err("recvfrom failed");
			g_tries = 0;
			g_io_channel_shutdown(src, TRUE, NULL);
			err = MEDIA_THUMB_ERROR_NETWORK;
			goto callback;
		}
	} else {
		thumb_dbg("recvfrom done");
	}

	recv_msg.org_path[MAX_PATH_SIZE - 1] = '\0';
	recv_str_len = strlen(recv_msg.dst_path);
	thumb_dbg("recv %s(%d) from thumb daemon is successful", recv_msg.dst_path, recv_str_len);

	g_io_channel_shutdown(src, TRUE, NULL);

	if (recv_msg.status == THUMB_FAIL) {
		thumb_err("Failed to make thumbnail");
		err = MEDIA_THUMB_ERROR_UNSUPPORTED;
		goto callback;
	}

callback:
	if (data) {
		thumbUserData* cb = (thumbUserData*)data;
		cb->func(err, recv_msg.dst_path, cb->user_data);
		free(cb);
		cb = NULL;
	}

	return FALSE;
}

int
_media_thumb_request_async(int msg_type, media_thumb_type thumb_type, const char *origin_path, thumbUserData *userData)
{
	int sock;
	const char *serv_ip = "127.0.0.1";
	struct sockaddr_in serv_addr;

	int send_str_len = strlen(origin_path);
	int pid;

	thumbMsg req_msg;
	memset((void *)&req_msg, 0, sizeof(thumbMsg));

	req_msg.msg_type = msg_type;
	req_msg.thumb_type = thumb_type;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	struct timeval tv_timeout = { 0, 100 };

	if (send_str_len > MAX_PATH_SIZE) {
		thumb_err("original path's length exceeds %d(max packet size)", MAX_PATH_SIZE);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	pid = getpid();
	req_msg.pid = pid;

	/* Creaete a datagram/UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		thumb_err("socket failed: %d\n", errno);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
		thumb_err("setsockopt failed: %d\n", errno);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	g_io_add_watch(channel, G_IO_IN, _media_thumb_write_socket, userData );

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
	serv_addr.sin_port = htons(THUMB_DAEMON_PORT);

	if (sendto(sock, &req_msg, sizeof(req_msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(req_msg)) {
		thumb_err("sendto failed: %d\n", errno);
		g_io_channel_shutdown(channel, TRUE, NULL);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	thumb_dbg("Sending msg to thumbnail daemon is successful");

	return 0;
}


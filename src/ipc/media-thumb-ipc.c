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
#include <grp.h>
#include <pwd.h>

#define GLOBAL_USER    0 //#define     tzplatform_getenv(TZ_GLOBAL) //TODO

static __thread GQueue *g_request_queue = NULL;
typedef struct {
	GIOChannel *channel;
	char *path;
	int source_id;
} thumbReq;

int
_media_thumb_create_socket(int sock_type, int *sock)
{
	int sock_fd = 0;

	if ((sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		thumb_err("socket failed: %s", strerror(errno));
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	if (sock_type == CLIENT_SOCKET) {

#ifdef _USE_MEDIA_UTIL_
		struct timeval tv_timeout = { MS_TIMEOUT_SEC_10, 0 };
#else
		struct timeval tv_timeout = { TIMEOUT_SEC, 0 };
#endif

		if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			thumb_err("setsockopt failed: %s", strerror(errno));
			close(sock_fd);
			return MEDIA_THUMB_ERROR_NETWORK;
		}
	} else if (sock_type == SERVER_SOCKET) {

		int n_reuse = 1;

		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &n_reuse, sizeof(n_reuse)) == -1) {
			thumb_err("setsockopt failed: %s", strerror(errno));
			close(sock_fd);
			return MEDIA_THUMB_ERROR_NETWORK;
		}
	}

	*sock = sock_fd;

	return MEDIA_THUMB_ERROR_NONE;
}


int
_media_thumb_create_udp_socket(int *sock)
{
	int sock_fd = 0;

	if ((sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		thumb_err("socket failed: %s", strerror(errno));
		return MEDIA_THUMB_ERROR_NETWORK;
	}

#ifdef _USE_MEDIA_UTIL_
	struct timeval tv_timeout = { MS_TIMEOUT_SEC_10, 0 };
#else
	struct timeval tv_timeout = { TIMEOUT_SEC, 0 };
#endif

	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
		thumb_err("setsockopt failed: %s", strerror(errno));
		close(sock_fd);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	*sock = sock_fd;

	return MEDIA_THUMB_ERROR_NONE;
}

int _media_thumb_get_error()
{
	if (errno == EWOULDBLOCK) {
		thumb_err("Timeout. Can't try any more");
		return MEDIA_THUMB_ERROR_TIMEOUT;
	} else {
		thumb_err("recvfrom failed : %s", strerror(errno));
		return MEDIA_THUMB_ERROR_NETWORK;
	}
}

int __media_thumb_pop_req_queue(const char *path, bool shutdown_channel)
{
	int req_len = 0, i;

	if (g_request_queue == NULL) return -1;
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
				//thumb_dbg("Popped %s", path);
				if (shutdown_channel) {
					g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), req->source_id));

					g_io_channel_shutdown(req->channel, TRUE, NULL);
					g_io_channel_unref(req->channel);
				}
				g_queue_pop_nth(g_request_queue, i);

				SAFE_FREE(req->path);
				SAFE_FREE(req);

				break;
			}
		}
		if (i == req_len) {
//			thumb_dbg("There's no %s in the queue", path);
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
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
				return -1;

				break;
			}
		}
	}

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_recv_msg(int sock, int header_size, thumbMsg *msg)
{
	int recv_msg_len = 0;
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(header_size);

	if ((recv_msg_len = recv(sock, buf, header_size, 0)) <= 0) {
		thumb_err("recvfrom failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);
	//thumb_dbg("origin_path_size : %d, dest_path_size : %d", msg->origin_path_size, msg->dest_path_size);

	SAFE_FREE(buf);

	buf = (unsigned char*)malloc(msg->origin_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->origin_path_size, 0)) < 0) {
		thumb_err("recvfrom failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->org_path, (char*)buf, msg->origin_path_size);
	//thumb_dbg("original path : %s", msg->org_path);

	SAFE_FREE(buf);
	buf = (unsigned char*)malloc(msg->dest_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->dest_path_size, 0)) < 0) {
		thumb_err("recvfrom failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->dst_path, (char*)buf, msg->dest_path_size);
	//thumb_dbg("destination path : %s", msg->dst_path);

	SAFE_FREE(buf);
	return MEDIA_THUMB_ERROR_NONE;
}

#ifdef _USE_UDS_SOCKET_
int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size)
#else
int
_media_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_in *from_addr, unsigned int *from_size)
#endif
{
	int recv_msg_len = 0;
#ifdef _USE_UDS_SOCKET_
	unsigned int from_addr_size = sizeof(struct sockaddr_un);
#else
	unsigned int from_addr_size = sizeof(struct sockaddr_in);
#endif
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(sizeof(thumbMsg));

	if ((recv_msg_len = recvfrom(sock, buf, sizeof(thumbMsg), 0, (struct sockaddr *)from_addr, &from_addr_size)) < 0) {
		thumb_err("recvfrom failed : %s", strerror(errno));
		SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);

	if (msg->origin_path_size <= 0  || msg->origin_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->origin_path_size );
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	strncpy(msg->org_path, (char*)buf + header_size, msg->origin_path_size);
	//thumb_dbg("original path : %s", msg->org_path);

	if (msg->dest_path_size <= 0  || msg->dest_path_size > MAX_PATH_SIZE) {
		SAFE_FREE(buf);
		thumb_err("msg->origin_path_size is invalid %d", msg->dest_path_size );
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	strncpy(msg->dst_path, (char*)buf + header_size + msg->origin_path_size, msg->dest_path_size);
	//thumb_dbg("destination path : %s", msg->dst_path);

	SAFE_FREE(buf);
	*from_size = from_addr_size;

	return MEDIA_THUMB_ERROR_NONE;
}

int
_media_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size)
{
	if (req_msg == NULL || buf == NULL) {
		return -1;
	}

	int org_path_len = 0;
	int dst_path_len = 0;
	int size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;
	org_path_len = strlen(req_msg->org_path) + 1;
	dst_path_len = strlen(req_msg->dst_path) + 1;

	//thumb_dbg("Basic Size : %d, org_path : %s[%d], dst_path : %s[%d]", header_size, req_msg->org_path, org_path_len, req_msg->dst_path, dst_path_len);

	size = header_size + org_path_len + dst_path_len;
	*buf = malloc(size);
	memcpy(*buf, req_msg, header_size);
	memcpy((*buf)+header_size, req_msg->org_path, org_path_len);
	memcpy((*buf)+header_size + org_path_len, req_msg->dst_path, dst_path_len);

	*buf_size = size;

	return 0;
}

int
_media_thumb_request(int msg_type, media_thumb_type thumb_type, const char *origin_path, char *thumb_path, int max_length, media_thumb_info *thumb_info, uid_t uid)
{
	int sock;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#elif defined(_USE_UDS_SOCKET_TCP_)
	struct sockaddr_un serv_addr;
#else
	const char *serv_ip = "127.0.0.1";
	struct sockaddr_in serv_addr;
#endif

	int recv_str_len = 0;
	int err;
	int pid;


#ifdef _USE_MEDIA_UTIL_
#ifdef _USE_UDS_SOCKET_
	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_CREATOR_PORT) < 0) {
#elif defined(_USE_UDS_SOCKET_TCP_)
	if (ms_ipc_create_client_tcp_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_CREATOR_TCP_PORT) < 0) {
#else
	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock) < 0) {
#endif
		thumb_err("ms_ipc_create_client_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}
#else
	/* Creaete a TCP socket */
	if (_media_thumb_create_socket(CLIENT_SOCKET, &sock) < 0) {
		thumb_err("_media_thumb_create_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}
#endif

	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
#elif defined(_USE_UDS_SOCKET_TCP_)
	serv_addr.sun_family = AF_UNIX;
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
#endif

#ifdef _USE_MEDIA_UTIL_
#ifdef _USE_UDS_SOCKET_
	strcpy(serv_addr.sun_path, "/var/run/media-server/media_ipc_thumbcreator.socket");
#elif defined(_USE_UDS_SOCKET_TCP_)
	thumb_dbg("");
	strcpy(serv_addr.sun_path, "/var/run/media-server/media_ipc_thumbcreator.socket");
#else
	serv_addr.sin_port = htons(MS_THUMB_CREATOR_PORT);
#endif
#else
	serv_addr.sin_port = htons(THUMB_DAEMON_PORT);
#endif

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("connect error : %s", strerror(errno));
		return MEDIA_THUMB_ERROR_NETWORK;
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
	req_msg.thumb_type = thumb_type;
	req_msg.uid = uid;
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(req_msg.dst_path, thumb_path, sizeof(req_msg.dst_path));
		req_msg.dst_path[strlen(req_msg.dst_path)] = '\0';
	}

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = strlen(req_msg.dst_path) + 1;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		close(sock);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d\n", errno);
		SAFE_FREE(buf);
		close(sock);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	thumb_dbg("Sending msg to thumbnail daemon is successful");

	SAFE_FREE(buf);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		close(sock);
		return err;
	}

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

static int _mkdir(const char *dir, mode_t mode) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, mode);
                        *p = '/';
                }
        return mkdir(tmp, mode);
}

static char* _media_thumb_get_default_path(uid_t uid)
{
	char *result_psswd = NULL;
	struct group *grpinfo = NULL;
	if(uid == getuid())
	{
		result_psswd = strdup(THUMB_DEFAULT_PATH);
		grpinfo = getgrnam("users");
		if(grpinfo == NULL) {
			thumb_err("getgrnam(users) returns NULL !");
			return NULL;
		}
	}
	else
	{
		struct passwd *userinfo = getpwuid(uid);
		if(userinfo == NULL) {
			thumb_err("getpwuid(%d) returns NULL !", uid);
			return NULL;
		}
		grpinfo = getgrnam("users");
		if(grpinfo == NULL) {
			thumb_err("getgrnam(users) returns NULL !");
			return NULL;
		}
		// Compare git_t type and not group name
		if (grpinfo->gr_gid != userinfo->pw_gid) {
			thumb_err("UID [%d] does not belong to 'users' group!", uid);
			return NULL;
		}
		asprintf(&result_psswd, "%s/data/file-manager-service/.thumb/phone", userinfo->pw_dir);
	}

	/* create dir */
	_mkdir(result_psswd,S_IRWXU | S_IRWXG | S_IRWXO);

	return result_psswd;
}

int
_media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg, uid_t uid)
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
	int alpha = 0;

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

	err = _media_thumb_db_connect(uid);
	if (err < 0) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	if (msg_type == THUMB_REQUEST_DB_INSERT) {
		err = _media_thumb_get_thumb_from_db_with_size(origin_path, thumb_path, max_length, &need_update_db, &origin_w, &origin_h);
		if (err == 0) {
			res_msg->origin_width = origin_w;
			res_msg->origin_height = origin_h;
			_media_thumb_db_disconnect();
			return MEDIA_THUMB_ERROR_NONE;
		} else {
			if (strlen(thumb_path) == 0) {
				err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length,uid);
				if (err < 0) {
					thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
					strncpy(thumb_path, _media_thumb_get_default_path(uid), max_length);
					_media_thumb_db_disconnect();
					return err;
				}

				thumb_path[strlen(thumb_path)] = '\0';
			}
		}

	} else if (msg_type == THUMB_REQUEST_SAVE_FILE) {
		strncpy(thumb_path, req_msg->dst_path, max_length);

	} else if (msg_type == THUMB_REQUEST_ALL_MEDIA) {
		err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length,uid);
		if (err < 0) {
			thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
			strncpy(thumb_path, _media_thumb_get_default_path(uid), max_length);
			_media_thumb_db_disconnect();
			return err;
		}

		thumb_path[strlen(thumb_path)] = '\0';
	}

	if (g_file_test(thumb_path, 
					G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_warn("thumb path already exists in file system.. remove the existed file");
		_media_thumb_remove_file(thumb_path);
	}

	err = _thumbnail_get_data(origin_path, thumb_type, thumb_format, &data, &thumb_size, &thumb_w, &thumb_h, &origin_w, &origin_h, &alpha, uid);
	if (err < 0) {
		thumb_err("_thumbnail_get_data failed - %d\n", err);
		SAFE_FREE(data);

		strncpy(thumb_path, _media_thumb_get_default_path(uid), max_length);
		_media_thumb_db_disconnect();
		return err;
	}

	//thumb_dbg("Size : %d, W:%d, H:%d", thumb_size, thumb_w, thumb_h);
	//thumb_dbg("Origin W:%d, Origin H:%d\n", origin_w, origin_h);
	//thumb_dbg("Thumb : %s", thumb_path);

	res_msg->msg_type = THUMB_RESPONSE;
	res_msg->thumb_size = thumb_size;
	res_msg->thumb_width = thumb_w;
	res_msg->thumb_height = thumb_h;
	res_msg->origin_width = origin_w;
	res_msg->origin_height = origin_h;

	/* If the image is transparent PNG format, make png file as thumbnail of this image */
	if (alpha) {
		char file_ext[10];
		err = _media_thumb_get_file_ext(origin_path, file_ext, sizeof(file_ext));
		if (strncasecmp(file_ext, "png", 3) == 0) {
			int len = strlen(thumb_path);
			thumb_path[len - 3] = 'p';
			thumb_path[len - 2] = 'n';
			thumb_path[len - 1] = 'g';
		}
		thumb_dbg("Thumb path is changed : %s", thumb_path);
	}

	err = _media_thumb_save_to_file_with_evas(data, thumb_w, thumb_h, alpha, thumb_path);
	if (err < 0) {
		thumb_err("save_to_file_with_evas failed - %d\n", err);
		SAFE_FREE(data);

		if (msg_type == THUMB_REQUEST_DB_INSERT || msg_type == THUMB_REQUEST_ALL_MEDIA)
			strncpy(thumb_path, _media_thumb_get_default_path(uid), max_length);

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
		err = _media_thumb_update_db(origin_path, thumb_path, res_msg->origin_width, res_msg->origin_height, uid);
		if (err < 0) {
			thumb_err("_media_thumb_update_db failed : %d", err);
		}
	}

	_media_thumb_db_disconnect();

	return 0;
}

gboolean _media_thumb_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	thumbMsg recv_msg;
	int header_size = 0;
	int recv_str_len = 0;
	int sock = 0;
	int err = MEDIA_THUMB_ERROR_NONE;

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	sock = g_io_channel_unix_get_fd(src);

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE*2;

	thumb_err("_media_thumb_write_socket socket : %d", sock);

	if ((err = _media_thumb_recv_msg(sock, header_size, &recv_msg)) < 0) {
		thumb_err("_media_thumb_recv_msg failed ");
		g_io_channel_shutdown(src, TRUE, NULL);
		return FALSE;
	}

	recv_str_len = strlen(recv_msg.dst_path);
	thumb_dbg("recv %s(%d) in  [ %s ] from thumb daemon is successful", recv_msg.dst_path, recv_str_len, recv_msg.org_path);

	g_io_channel_shutdown(src, TRUE, NULL);

	//thumb_dbg("Completed..%s", recv_msg.org_path);
	__media_thumb_pop_req_queue(recv_msg.org_path, FALSE);

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

	thumb_dbg("Done");
	return FALSE;
}

int
_media_thumb_request_async(int msg_type, media_thumb_type thumb_type, const char *origin_path, thumbUserData *userData, uid_t uid)
{
	int sock;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#elif defined(_USE_UDS_SOCKET_TCP_)
	struct sockaddr_un serv_addr;
#else
	const char *serv_ip = "127.0.0.1";
	struct sockaddr_in serv_addr;
#endif

	int pid;

	if ((msg_type == THUMB_REQUEST_DB_INSERT) && (__media_thumb_check_req_queue(origin_path) < 0)) {
		return MEDIA_THUMB_ERROR_DUPLICATED_REQUEST;
	}

#ifdef _USE_MEDIA_UTIL_
#ifdef _USE_UDS_SOCKET_
	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_CREATOR_PORT) < 0) {
#elif defined(_USE_UDS_SOCKET_TCP_)
	if (ms_ipc_create_client_tcp_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_CREATOR_TCP_PORT) < 0) {
#else
	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock) < 0) {
#endif
		thumb_err("ms_ipc_create_client_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}
#else
	/* Creaete a TCP socket */
	if (_media_thumb_create_socket(CLIENT_SOCKET, &sock) < 0) {
		thumb_err("_media_thumb_create_socket failed");
		return MEDIA_THUMB_ERROR_NETWORK;
	}
#endif

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	int source_id = -1;


	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
#elif defined(_USE_UDS_SOCKET_TCP_)
	serv_addr.sun_family = AF_UNIX;
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
#endif

#ifdef _USE_MEDIA_UTIL_
#ifdef _USE_UDS_SOCKET_
	strcpy(serv_addr.sun_path, "/tmp/media_ipc_thumbcreator.dat");
#elif defined(_USE_UDS_SOCKET_TCP_)
	strcpy(serv_addr.sun_path, "/tmp/media_ipc_thumbcreator.dat");
#else
	serv_addr.sin_port = htons(MS_THUMB_CREATOR_PORT);
#endif
#else
	serv_addr.sin_port = htons(THUMB_DAEMON_PORT);
#endif

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("connect error : %s", strerror(errno));
		g_io_channel_shutdown(channel, TRUE, NULL);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	if (msg_type != THUMB_REQUEST_CANCEL_MEDIA) {
		//source_id = g_io_add_watch(channel, G_IO_IN, _media_thumb_write_socket, userData );

		/* Create new channel to watch udp socket */
		GSource *source = NULL;
		source = g_io_create_watch(channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)_media_thumb_write_socket, userData, NULL);
		source_id = g_source_attach(source, g_main_context_get_thread_default());
	}

	thumbMsg req_msg;
	memset((void *)&req_msg, 0, sizeof(thumbMsg));

	pid = getpid();
	req_msg.pid = pid;
	req_msg.msg_type = msg_type;
	req_msg.thumb_type = thumb_type;
	req_msg.uid = uid;	
	
	strncpy(req_msg.org_path, origin_path, sizeof(req_msg.org_path));
	req_msg.org_path[strlen(req_msg.org_path)] = '\0';

	req_msg.origin_path_size = strlen(req_msg.org_path) + 1;
	req_msg.dest_path_size = strlen(req_msg.dst_path) + 1;

	if (req_msg.origin_path_size > MAX_PATH_SIZE || req_msg.dest_path_size > MAX_PATH_SIZE) {
		thumb_err("path's length exceeds %d", MAX_PATH_SIZE);
		g_io_channel_shutdown(channel, TRUE, NULL);
		return MEDIA_THUMB_ERROR_INVALID_PARAMETER;
	}

	unsigned char *buf = NULL;
	int buf_size = 0;
	_media_thumb_set_buffer(&req_msg, &buf, &buf_size);

	//thumb_dbg("buffer size : %d", buf_size);

	if (send(sock, buf, buf_size, 0) != buf_size) {
		thumb_err("sendto failed: %d\n", errno);
		SAFE_FREE(buf);
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), source_id));
		g_io_channel_shutdown(channel, TRUE, NULL);
		return MEDIA_THUMB_ERROR_NETWORK;
	}

	SAFE_FREE(buf);
	thumb_dbg("Sending msg to thumbnail daemon is successful");

#if 0
	if (msg_type == THUMB_REQUEST_CANCEL_MEDIA) {
		g_io_channel_shutdown(channel, TRUE, NULL);
	}
#else
	if (msg_type == THUMB_REQUEST_CANCEL_MEDIA) {
		g_io_channel_shutdown(channel, TRUE, NULL);

		//thumb_dbg("Cancel : %s[%d]", origin_path, sock);
		__media_thumb_pop_req_queue(origin_path, TRUE);
	} else if (msg_type == THUMB_REQUEST_DB_INSERT) {
		if (g_request_queue == NULL) {
		 	g_request_queue = g_queue_new();
		}

		thumbReq *thumb_req = NULL;
		thumb_req = calloc(1, sizeof(thumbReq));
		if (thumb_req == NULL) {
			thumb_err("Failed to create request element");
			return 0;
		}

		thumb_req->channel = channel;
		thumb_req->path = strdup(origin_path);
		thumb_req->source_id = source_id;

		//thumb_dbg("Push : %s [%d]", origin_path, sock);
		g_queue_push_tail(g_request_queue, (gpointer)thumb_req);
	}
#endif

	return 0;
}


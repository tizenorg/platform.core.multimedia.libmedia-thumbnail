/*
 * media-thumbnail-server
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

#include "thumb-server-internal.h"
#include "media-thumb-util.h"
#include "media-thumb-debug.h"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <vconf.h>
#include <grp.h>
#include <pwd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_THUMBNAIL_SERVER"
#define THUMB_BLOCK_SIZE 512

static __thread char **arr_path;
static __thread uid_t *arr_uid;
static __thread int g_idx = 0;
static __thread int g_cur_idx = 0;

GMainLoop *g_thumb_server_mainloop; // defined in thumb-server.c as extern

gboolean _thumb_server_send_msg_to_agent(int msg_type);
void _thumb_daemon_stop_job();

gboolean _thumb_daemon_start_jobs(gpointer data)
{
	thumb_dbg("");

	_thumb_server_send_msg_to_agent(MS_MSG_THUMB_SERVER_READY);

	return FALSE;
}

void _thumb_daemon_finish_jobs(void)
{
	sqlite3 *sqlite_db_handle = _media_thumb_db_get_handle();

	if (sqlite_db_handle != NULL) {
		_media_thumb_db_disconnect();
		thumb_dbg("sqlite3 handle is alive. So disconnect to sqlite3");
	}

	g_main_loop_quit(g_thumb_server_mainloop);

	return;
}

int _thumb_daemon_mmc_status(void)
{
	int err = -1;
	int status = -1;

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status);
	if (err == 0) {
		return status;
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return status;
}

void _thumb_daemon_mmc_eject_vconf_cb(void *data)
{
	int err = -1;
	int status = 0;

	thumb_warn("_thumb_daemon_vconf_cb called");

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status);
	if (err == 0) {
		if (status == VCONFKEY_SYSMAN_MMC_REMOVED || status == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {
			thumb_warn("SD card is ejected or not mounted. So media-thumbnail-server stops jobs to extract all thumbnails");

			_thumb_daemon_stop_job();
		}
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return;
}

void _thumb_daemon_vconf_cb(void *data)
{
	int err = -1;
	int status = 0;

	thumb_warn("_thumb_daemon_vconf_cb called");

	err = vconf_get_int(VCONFKEY_SYSMAN_MMC_FORMAT, &status);
	if (err == 0) {
		if (status == VCONFKEY_SYSMAN_MMC_FORMAT_COMPLETED) {
			thumb_warn("SD card format is completed. So media-thumbnail-server stops jobs to extract all thumbnails");

			_thumb_daemon_stop_job();
		} else {
			thumb_dbg("not completed");
		}
	} else if (err == -1) {
		thumb_err("vconf_get_int failed : %d", err);
	} else {
		thumb_err("vconf_get_int Unexpected error code: %d", err);
	}

	return;
}

void _thumb_daemon_stop_job()
{
	int i = 0;
	char *path = NULL;

	thumb_warn("There are %d jobs in the queue. But all jobs will be stopped", g_idx - g_cur_idx);

	for (i = g_cur_idx; i < g_idx; i++) {
		path = arr_path[g_cur_idx++];
		SAFE_FREE(path);
	}

	return;
}

int _thumb_daemon_process_job(thumbMsg *req_msg, thumbMsg *res_msg, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	err = _media_thumb_process(req_msg, res_msg, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		if (req_msg->msg_type == THUMB_REQUEST_SAVE_FILE) {
			thumb_err("_media_thumb_process is failed: %d", err);
			res_msg->status = THUMB_FAIL;
		} else {
			thumb_warn("_media_thumb_process is failed: %d, So use default thumb", err);
			res_msg->status = THUMB_SUCCESS;
		}
	} else {
		res_msg->status = THUMB_SUCCESS;
	}

	return err;
}

static int __thumb_daemon_process_job_raw(thumbMsg *req_msg, thumbMsg *res_msg, thumbRawAddMsg *res_raw_msg, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	err = _media_thumb_process_raw(req_msg, res_msg, res_raw_msg, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		if (err != MS_MEDIA_ERR_FILE_NOT_EXIST) {
			thumb_warn("_media_thumb_process is failed: %d, So use default thumb", err);
			res_msg->status = THUMB_SUCCESS;
		} else {
			thumb_warn("_media_thumb_process is failed: %d, (file not exist) ", err);
			res_msg->status = THUMB_FAIL;
		}
	} else {
		res_msg->status = THUMB_SUCCESS;
	}

	return err;
}

int _thumb_daemon_all_extract(uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	char query_string[MAX_PATH_SIZE + 1] = { 0, };
	char path[MAX_PATH_SIZE + 1] = { 0, };
	sqlite3 *sqlite_db_handle = NULL;
	sqlite3_stmt *sqlite_stmt = NULL;

	err = _media_thumb_db_connect(uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_db_connect failed: %d", err);
		return err;
	}

	sqlite_db_handle = _media_thumb_db_get_handle();
	if (sqlite_db_handle == NULL) {
		thumb_err("sqlite handle is NULL");
		return MS_MEDIA_ERR_INTERNAL;
	}

	if (_thumb_daemon_mmc_status() == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		snprintf(query_string, sizeof(query_string), SELECT_PATH_FROM_UNEXTRACTED_THUMB_MEDIA);
	} else {
		snprintf(query_string, sizeof(query_string), SELECT_PATH_FROM_UNEXTRACTED_THUMB_INTERNAL_MEDIA);
	}

	thumb_warn("Query: %s", query_string);

	err = sqlite3_prepare_v2(sqlite_db_handle, query_string, strlen(query_string), &sqlite_stmt, NULL);
	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]", sqlite3_errmsg(sqlite_db_handle));
		_media_thumb_db_disconnect();
		return MS_MEDIA_ERR_INTERNAL;
	}

	while(1) {
		err = sqlite3_step(sqlite_stmt);
		if (err != SQLITE_ROW) {
			thumb_dbg("end of row [%s]", sqlite3_errmsg(sqlite_db_handle));
			break;
		}

		strncpy(path, (const char *)sqlite3_column_text(sqlite_stmt, 0), sizeof(path));
		path[sizeof(path) - 1] = '\0';

		thumb_dbg("Path : %s", path);

		if (g_idx == 0) {
			arr_path = (char**)malloc(sizeof(char*));
			arr_uid = (uid_t*)malloc(sizeof(uid_t));
		} else {
			arr_path = (char**)realloc(arr_path, (g_idx + 1) * sizeof(char*));
			arr_uid = (uid_t*)realloc(arr_uid, (g_idx + 1) * sizeof(uid_t));
		}
		arr_uid[g_idx] = uid;
		arr_path[g_idx++] = strdup(path);
	}

	sqlite3_finalize(sqlite_stmt);
	_media_thumb_db_disconnect();

	return MS_MEDIA_ERR_NONE;
}

int _thumb_daemon_process_queue_jobs(gpointer data)
{
	int err = MS_MEDIA_ERR_NONE;
	char *path = NULL;
	uid_t uid = NULL;

	if (g_cur_idx < g_idx) {
		thumb_warn("There are %d jobs in the queue", g_idx - g_cur_idx);
		thumb_dbg("Current idx : [%d]", g_cur_idx);
		uid = arr_uid[g_cur_idx];
		path = arr_path[g_cur_idx++];

		thumbMsg recv_msg, res_msg;
		memset(&recv_msg, 0x00, sizeof(thumbMsg));
		memset(&res_msg, 0x00, sizeof(thumbMsg));

		recv_msg.msg_type = THUMB_REQUEST_DB_INSERT;
		recv_msg.thumb_type = MEDIA_THUMB_LARGE;
		strncpy(recv_msg.org_path, path, sizeof(recv_msg.org_path));
		recv_msg.org_path[sizeof(recv_msg.org_path) - 1] = '\0';

		_thumb_daemon_process_job(&recv_msg, &res_msg,uid );

		if (res_msg.status == THUMB_SUCCESS) {

			err = _media_thumb_db_connect(uid);
			if (err != MS_MEDIA_ERR_NONE) {
				thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
				return TRUE;
			}

			/* Need to update DB once generating thumb is done */
			err = _media_thumb_update_db(recv_msg.org_path,
										res_msg.dst_path,
										res_msg.origin_width,
										res_msg.origin_height,
										uid);
			if (err < 0) {
				thumb_err("_media_thumb_update_db failed : %d", err);
			}

			_media_thumb_db_disconnect();
		}

		SAFE_FREE(path);
	} else {
		g_cur_idx = 0;
		g_idx = 0;
		thumb_warn("Deleting array");
		SAFE_FREE(arr_path);
		SAFE_FREE(arr_uid);
		//_media_thumb_db_disconnect();

		_thumb_server_send_msg_to_agent(MS_MSG_THUMB_EXTRACT_ALL_DONE); // MS_MSG_THUMB_EXTRACT_ALL_DONE

		return FALSE;
	}

	return TRUE;
}

gboolean _thumb_server_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	int client_sock;
	thumbMsg recv_msg;
	thumbMsg res_msg;
	thumbRawAddMsg res_raw_msg;

	int sock = -1;
	int header_size = 0;

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	memset((void *)&res_raw_msg, 0, sizeof(res_raw_msg));

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		thumb_err("sock fd is invalid!");
		return TRUE;
	}

	header_size = sizeof(thumbMsg) - MAX_PATH_SIZE * 2 - sizeof(unsigned char *);

	if (_media_thumb_recv_udp_msg(sock, header_size, &recv_msg, &client_addr, &client_addr_len) < 0) {
		thumb_err("_media_thumb_recv_udp_msg failed");
		return TRUE;
	}

	thumb_warn("Received [%d] %s(%d) from PID(%d) \n", recv_msg.msg_type, recv_msg.org_path, strlen(recv_msg.org_path), recv_msg.pid);

	if (recv_msg.msg_type == THUMB_REQUEST_ALL_MEDIA) {
		thumb_dbg("All thumbnails are being extracted now");
		_thumb_daemon_all_extract(recv_msg.uid);
		g_idle_add(_thumb_daemon_process_queue_jobs, NULL);
	} else if(recv_msg.msg_type == THUMB_REQUEST_RAW_DATA) {
		__thumb_daemon_process_job_raw(&recv_msg, &res_msg, &res_raw_msg, recv_msg.uid);
	} else if(recv_msg.msg_type == THUMB_REQUEST_KILL_SERVER) {
		thumb_warn("received KILL msg from thumbnail agent.");
	} else {
		_thumb_daemon_process_job(&recv_msg, &res_msg,recv_msg.uid);
	}

	if(res_msg.msg_type == 0)
		res_msg.msg_type = recv_msg.msg_type;
	res_msg.request_id = recv_msg.request_id;
	strncpy(res_msg.org_path, recv_msg.org_path, recv_msg.origin_path_size);
	res_msg.origin_path_size = recv_msg.origin_path_size;
	res_msg.thumb_data = (unsigned char *)"\0";
	if(res_msg.msg_type != THUMB_RESPONSE_RAW_DATA) {
		res_msg.dest_path_size = strlen(res_msg.dst_path) + 1;
		res_msg.thumb_size = 1;
	} else {
		res_msg.dest_path_size = 1;
		res_msg.dst_path[0] = '\0';
	}

	int buf_size = 0;
	int block_size = 0;
	int sent_size = 0;
	unsigned char *buf = NULL;
	_media_thumb_set_buffer_for_response(&res_msg, &buf, &buf_size);

	if (sendto(sock, buf, buf_size, 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != buf_size) {
		thumb_stderror("sendto failed");
		SAFE_FREE(buf);
		return TRUE;
	}

	thumb_warn_slog("Sent %s(%d)", res_msg.dst_path, strlen(res_msg.dst_path));
	SAFE_FREE(buf);

	//Add sendto_raw_data
	if(recv_msg.msg_type == THUMB_REQUEST_RAW_DATA) {
		_media_thumb_set_add_raw_data_buffer(&res_raw_msg, &buf, &buf_size);
		block_size = THUMB_BLOCK_SIZE;
		while(buf_size > 0) {
			if (buf_size < THUMB_BLOCK_SIZE) {
				block_size = buf_size;
			}
			if (sendto(sock, buf + sent_size, block_size, 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != block_size) {
				thumb_err("sendto failed: %s", strerror(errno));
				SAFE_FREE(buf);
				return TRUE;
			}
			sent_size += block_size;
			buf_size -= block_size;
		}
		thumb_warn_slog("Sent [%s] additional data[%d]", res_msg.org_path, sent_size);
	}

	SAFE_FREE(buf);

	if(recv_msg.msg_type == THUMB_REQUEST_KILL_SERVER) {
		thumb_warn("Shutting down...");
		g_main_loop_quit(g_thumb_server_mainloop);
	}

	return TRUE;
}

gboolean _thumb_server_send_msg_to_agent(int msg_type)
{
	int sock;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	ms_thumb_server_msg send_msg;
	sock_info.port = MS_THUMB_COMM_PORT;

	if (ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		thumb_err("ms_ipc_create_server_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, "/var/run/media-server/media_ipc_thumbcomm.socket");

	send_msg.msg_type = msg_type;

	if (sendto(sock, &send_msg, sizeof(ms_thumb_server_msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != sizeof(ms_thumb_server_msg)) {
		thumb_stderror("sendto failed");
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	thumb_dbg("Sending msg to thumbnail agent[%d] is successful", send_msg.msg_type);

	ms_ipc_delete_client_socket(&sock_info);

 	return TRUE;
}

gboolean _thumb_server_prepare_socket(int *sock_fd)
{
	int sock;
	unsigned short serv_port;

	thumbMsg recv_msg;
	thumbMsg res_msg;

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	serv_port = MS_THUMB_DAEMON_PORT;

	if (ms_ipc_create_server_socket(MS_PROTOCOL_UDP, serv_port, &sock) < 0) {
		thumb_err("ms_ipc_create_server_socket failed");
		return FALSE;
	}

	*sock_fd = sock;

	return TRUE;
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
			if(result_psswd)
				free(result_psswd);
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

int _thumbnail_get_data(const char *origin_path, 
						media_thumb_type thumb_type, 
						media_thumb_format format, 
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						int *origin_width,
						int *origin_height,
						int *alpha,
						uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	int thumb_width = -1;
	int thumb_height = -1;

	if (origin_path == NULL || size == NULL
			|| width == NULL || height == NULL) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (format < MEDIA_THUMB_BGRA || format > MEDIA_THUMB_RGB888) {
		thumb_err("parameter format is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path (%s) does not exist", origin_path);
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_width = _media_thumb_get_width(thumb_type);
	if (thumb_width < 0) {
		thumb_err("media_thumb_type is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_height = _media_thumb_get_height(thumb_type);
	if (thumb_height < 0) {
		thumb_err("media_thumb_type is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_dbg("Origin path : %s", origin_path);

	int file_type = THUMB_NONE_TYPE;
	media_thumb_info thumb_info = {0,};
	file_type = _media_thumb_get_file_type(origin_path);

	if (file_type == THUMB_IMAGE_TYPE) {
		err = _media_thumb_image(origin_path, thumb_width, thumb_height, format, &thumb_info, false, uid);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_image failed");
			return err;
		}

	} else if (file_type == THUMB_VIDEO_TYPE) {
		err = _media_thumb_video(origin_path, thumb_width, thumb_height, format, &thumb_info,uid);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_image failed");
			return err;
		}
	}

	if (size) *size = thumb_info.size;
	if (width) *width = thumb_info.width;
	if (height) *height = thumb_info.height;
	*data = thumb_info.gdkdata;
	if (origin_width) *origin_width = thumb_info.origin_width;
	if (origin_height) *origin_height = thumb_info.origin_height;
	if (alpha) *alpha = thumb_info.alpha;

	thumb_dbg("Thumb data is generated successfully (Size:%d, W:%d, H:%d) 0x%x",
				*size, *width, *height, *data);

	return MS_MEDIA_ERR_NONE;
}

int _thumbnail_get_raw_data(const char *origin_path,
						media_thumb_format format,
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	int thumb_width = -1;
	int thumb_height = -1;

	if (origin_path == NULL || *width <= 0 || *height <= 0) {
		thumb_err("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (format < MEDIA_THUMB_BGRA || format > MEDIA_THUMB_RGB888) {
		thumb_err("parameter format is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!g_file_test
	    (origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			thumb_err("Original path (%s) does not exist", origin_path);
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	thumb_dbg("Origin path : %s", origin_path);

	int file_type = THUMB_NONE_TYPE;
	media_thumb_info thumb_info = {0,};
	file_type = _media_thumb_get_file_type(origin_path);
	thumb_width = *width;
	thumb_height = *height;

	if (file_type == THUMB_IMAGE_TYPE) {
		err = _media_thumb_image(origin_path, thumb_width, thumb_height, format, &thumb_info, true, uid);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_image failed");
			return err;
		}
	} else if (file_type == THUMB_VIDEO_TYPE) {
		err = _media_thumb_video(origin_path, thumb_width, thumb_height, format, &thumb_info,uid);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_image failed");
			return err;
		}
	}

	if (size) *size = thumb_info.size;
	if (width) *width = thumb_info.width;
	if (height) *height = thumb_info.height;
	*data = thumb_info.data;

	//thumb_dbg("Thumb data is generated successfully (Size:%d, W:%d, H:%d) 0x%x", *size, *width, *height, *data);

	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	GdkPixbuf *gdkdata = NULL;
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
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	int msg_type = req_msg->msg_type;
	media_thumb_type thumb_type = req_msg->thumb_type;
	const char *origin_path = req_msg->org_path;

	// Currently, The color space that is supported by the gdk-pixbuf is only RGB.
	media_thumb_format thumb_format = MEDIA_THUMB_RGB888;

	thumb_path = res_msg->dst_path;
	thumb_path[0] = '\0';
	max_length = sizeof(res_msg->dst_path);

	err = _media_thumb_db_connect(uid);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	if (msg_type == THUMB_REQUEST_DB_INSERT) {
		err = _media_thumb_get_thumb_from_db_with_size(origin_path, thumb_path, max_length, &need_update_db, &origin_w, &origin_h);
		if (err == MS_MEDIA_ERR_NONE) {
			res_msg->origin_width = origin_w;
			res_msg->origin_height = origin_h;
			_media_thumb_db_disconnect();
			return MS_MEDIA_ERR_NONE;
		} else {
			if (strlen(thumb_path) == 0) {
				err = _media_thumb_get_hash_name(origin_path, thumb_path, max_length,uid);
				if (err != MS_MEDIA_ERR_NONE) {
					char *default_path = _media_thumb_get_default_path(uid);
					if(default_path) {
						thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
						strncpy(thumb_path, default_path, max_length);
						free(default_path);
					}
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
		if (err != MS_MEDIA_ERR_NONE) {
			char *default_path = _media_thumb_get_default_path(uid);
			if(default_path) {
				thumb_err("_media_thumb_get_hash_name failed - %d\n", err);
				strncpy(thumb_path, default_path, max_length);
				free(default_path);
			}
			_media_thumb_db_disconnect();
			return err;
		}

		thumb_path[strlen(thumb_path)] = '\0';
	}

	thumb_dbg_slog("Thumb path : %s", thumb_path);

	if (g_file_test(thumb_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_warn("thumb path already exists in file system.. remove the existed file");
		_media_thumb_remove_file(thumb_path);
	}

	err = _thumbnail_get_data(origin_path, thumb_type, thumb_format, &gdkdata, &thumb_size, &thumb_w, &thumb_h, &origin_w, &origin_h, &alpha, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		char *default_path = _media_thumb_get_default_path(uid);
		thumb_err("_thumbnail_get_data failed - %d\n", err);
		if ( gdkdata != NULL ){
			g_object_unref(gdkdata);
		}
		if(default_path) {
			strncpy(thumb_path, default_path, max_length);
			free(default_path);
		}
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

	err = _media_thumb_save_to_file_with_gdk(gdkdata, thumb_w, thumb_h, alpha, thumb_path);
	if (err != MS_MEDIA_ERR_NONE) {
		char *default_path = _media_thumb_get_default_path(uid);
		thumb_err("save_to_file_with_gdk failed - %d", err);
		if ( gdkdata != NULL ){
			g_object_unref(gdkdata);
		}

		if (msg_type == THUMB_REQUEST_DB_INSERT || msg_type == THUMB_REQUEST_ALL_MEDIA) {
			if(default_path) {
				strncpy(thumb_path, default_path, max_length);
				free(default_path);
			}
		}
		_media_thumb_db_disconnect();
		return err;
	} else {
		thumb_dbg("file save success");
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

	g_object_unref(gdkdata);

	/* DB update if needed */
	if (need_update_db == 1) {
		err = _media_thumb_update_db(origin_path, thumb_path, res_msg->origin_width, res_msg->origin_height, uid);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_update_db failed : %d", err);
		}
	}

	_media_thumb_db_disconnect();

	return MS_MEDIA_ERR_NONE;
}

int
_media_thumb_process_raw(thumbMsg *req_msg, thumbMsg *res_msg, thumbRawAddMsg *res_raw_msg, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	unsigned char *data = NULL;
	int thumb_size = 0;
	int thumb_w = 0;
	int thumb_h = 0;

	if (req_msg == NULL || res_msg == NULL) {
		thumb_err("Invalid msg!");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	const char *origin_path = req_msg->org_path;

	media_thumb_format thumb_format = MEDIA_THUMB_BGRA;
	thumb_w = req_msg->thumb_width;
	thumb_h = req_msg->thumb_height;

	//thumb_dbg("origin_path : %s, thumb_w : %d, thumb_h : %d ", origin_path, thumb_w, thumb_h);

	if (!g_file_test(origin_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb_err("origin_path does not exist in file system.");
		return MS_MEDIA_ERR_FILE_NOT_EXIST;
	}

	err = _thumbnail_get_raw_data(origin_path, thumb_format, &data, &thumb_size, &thumb_w, &thumb_h, uid);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_thumbnail_get_data failed - %d", err);
		SAFE_FREE(data);
	}

	res_msg->msg_type = THUMB_RESPONSE_RAW_DATA;
	res_msg->thumb_width = thumb_w;
	res_msg->thumb_height = thumb_h;
	res_raw_msg->thumb_size = thumb_size;
	res_raw_msg->thumb_data = malloc(thumb_size * sizeof(unsigned char));
	memcpy(res_raw_msg->thumb_data, data, thumb_size);
	res_msg->thumb_size = thumb_size + sizeof(thumbRawAddMsg) - sizeof(unsigned char*);

	//thumb_dbg("Size : %d, W:%d, H:%d", thumb_size, thumb_w, thumb_h);

	return MS_MEDIA_ERR_NONE;
}

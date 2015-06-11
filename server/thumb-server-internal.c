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

#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <vconf.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_THUMBNAIL_SERVER"

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
	int count = 0;
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
		count = sqlite3_column_int(sqlite_stmt, 1);

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
		long start = thumb_get_debug_time();

		_thumb_daemon_process_job(&recv_msg, &res_msg,recv_msg.uid);

		long end = thumb_get_debug_time();
		thumb_dbg("Time : %f (%s)", ((double)(end - start) / (double)CLOCKS_PER_SEC), recv_msg.org_path);
	}

	if(res_msg.msg_type == 0)
		res_msg.msg_type = recv_msg.msg_type;
	res_msg.request_id = recv_msg.request_id;
	strncpy(res_msg.org_path, recv_msg.org_path, recv_msg.origin_path_size);
	res_msg.origin_path_size = recv_msg.origin_path_size;
	res_msg.thumb_data = (unsigned char *)"\0";
	if(res_msg.msg_type != THUMB_RESPONSE_RAW_DATA) {
		res_msg.dest_path_size = strlen(res_msg.dst_path);
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

	thumb_warn("Sent %s(%d)", res_msg.dst_path, strlen(res_msg.dst_path));

	SAFE_FREE(buf);

	//Add sendto_raw_data
	if(recv_msg.msg_type == THUMB_REQUEST_RAW_DATA) {
		_media_thumb_set_add_raw_data_buffer(&res_raw_msg, &buf, &buf_size);
		block_size = 512;
		while(buf_size > 0) {
			if (buf_size < 512) {
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
	const char *serv_ip = "127.0.0.1";
	struct sockaddr_un serv_addr;
	ms_thumb_server_msg send_msg;

	if (ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_COMM_PORT) < 0) {
		thumb_err("ms_ipc_create_server_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, "/var/run/media-server/media_ipc_thumbcomm.socket");

	send_msg.msg_type = msg_type;


    if (connect(sock, &serv_addr, sizeof(serv_addr)) < 0) {
        thumb_err("connect failed [%s]",strerror(errno));
        close(sock);
        return FALSE;
    }

	if (send(sock, &send_msg, sizeof(ms_thumb_server_msg), 0) != sizeof(ms_thumb_server_msg)) {
		thumb_err("sendto failed: %s\n", strerror(errno));
		close(sock);
		return FALSE;
	}

	thumb_dbg("Sending msg to thumbnail agent[%d] is successful", send_msg.msg_type);

	close(sock);
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


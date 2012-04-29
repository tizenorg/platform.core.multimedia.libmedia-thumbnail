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


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "Thumb-Server"

static __thread int sock;
static __thread char **arr_path;
static __thread int g_idx = 0;
static __thread int g_cur_idx = 0;

static __thread _server_mode_e g_server_mode = BLOCK_MODE;
static __thread int g_media_svr_pid = 0;

int _thumb_daemon_get_sockfd()
{
	return sock;
}

int _thumb_daemon_compare_pid_with_mediaserver_fast(int pid)
{
    int find_pid = -1;

	char path[128];
	char buff[128];
	snprintf(path, sizeof(path), "/proc/%d/status", g_media_svr_pid);

	FILE *fp = NULL;
	fp = fopen(path, "rt");
	if (fp) {
		fgets(buff, sizeof(buff), fp);
		fclose(fp);

		if (strstr(buff, "media-server")) {
			find_pid = g_media_svr_pid;
			thumb_dbg(" find_pid : %d", find_pid);
		} else {
			g_media_svr_pid = 0;
			return GETPID_FAIL;
		}
	} else {
		thumb_err("Can't read file [%s]", path);
		g_media_svr_pid = 0;
		return GETPID_FAIL;
	}

	if (find_pid == pid) {
		thumb_dbg("This is a request from media-server");
		return MEDIA_SERVER_PID;
	} else {
		thumb_dbg("This is a request from other apps");
		return OTHERS_PID;
	}
}

int _thumb_daemon_compare_pid_with_mediaserver(int pid)
{
    DIR *pdir;
    struct dirent pinfo;
    struct dirent *result = NULL;

    pdir = opendir("/proc");
    if (pdir == NULL) {
        thumb_err("err: NO_DIR");
        return GETPID_FAIL;
    }

    while (!readdir_r(pdir, &pinfo, &result)) {
        if (result == NULL)
            break;

        if (pinfo.d_type != 4 || pinfo.d_name[0] == '.'
            || pinfo.d_name[0] > 57)
            continue;

        FILE *fp;
        char buff[128];
        char path[128];

        snprintf(path, sizeof(path), "/proc/%s/status", pinfo.d_name);

        fp = fopen(path, "rt");
        if (fp) {
            fgets(buff, sizeof(buff), fp);
            fclose(fp);

            if (strstr(buff, "media-server")) {
                thumb_dbg("pinfo->d_name : %s", pinfo.d_name);
                g_media_svr_pid = atoi(pinfo.d_name);
                thumb_dbg("Media Server PID : %d", g_media_svr_pid);
            }
        }
    }

	closedir(pdir);

	if (g_media_svr_pid == pid) {
		thumb_dbg("This is a request from media-server");
		return MEDIA_SERVER_PID;
	} else {
		thumb_dbg("This is a request from other apps");
		return OTHERS_PID;
	}
}

int _thumb_daemon_recv_by_select(int fd, gboolean is_timeout)
{
	fd_set fds;
	int ret = -1;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (is_timeout) {
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;

		ret = select(fd + 1, &fds, 0, 0, &timeout);
	} else {
		ret = select(fd + 1, &fds, 0, 0, NULL);
	}

    return ret;
}

int _thumb_daemon_process_job(thumbMsg *req_msg, thumbMsg *res_msg)
{
	int err = -1;

	err = _media_thumb_process(req_msg, res_msg);
	if (err < 0) {
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

int _thumb_daemon_all_extract()
{
	int err = -1;
	int count = 0;
	char query_string[MAX_PATH_SIZE + 1] = { 0, };
	char path[MAX_PATH_SIZE + 1] = { 0, };
	sqlite3 *sqlite_db_handle = NULL;
	sqlite3_stmt *sqlite_stmt = NULL;

	err = _media_thumb_db_connect();
	if (err < 0) {
		thumb_err("_media_thumb_db_connect failed: %d", err);
		return MEDIA_THUMB_ERROR_DB;
	}

	sqlite_db_handle = _media_thumb_db_get_handle();
	if (sqlite_db_handle == NULL) {
		thumb_err("sqlite handle is NULL");
		return MEDIA_THUMB_ERROR_DB;
	}

	snprintf(query_string, sizeof(query_string), SELECT_PATH_FROM_UNEXTRACTED_THUMB_MEDIA);
	thumb_dbg("Query: %s", query_string);

	err = sqlite3_prepare_v2(sqlite_db_handle, query_string, strlen(query_string), &sqlite_stmt, NULL);
	if (SQLITE_OK != err) {
		thumb_err("prepare error [%s]\n", sqlite3_errmsg(sqlite_db_handle));
		return MEDIA_THUMB_ERROR_DB;
	}

	while(1) {
		err = sqlite3_step(sqlite_stmt);
		if (err != SQLITE_ROW) {
			thumb_dbg("end of row [%s]\n", sqlite3_errmsg(sqlite_db_handle));
			break;
		}

		strncpy(path, (const char *)sqlite3_column_text(sqlite_stmt, 0), MAX_PATH_SIZE + 1);
		count = sqlite3_column_int(sqlite_stmt, 1);

		thumb_dbg("Path : %s", path);

		if (g_idx == 0) {
			arr_path = (char**)malloc(sizeof(char*));
		} else {
			arr_path = (char**)realloc(arr_path, (g_idx + 1) * sizeof(char*));
		}

		arr_path[g_idx++] = strdup(path);
	}

	sqlite3_finalize(sqlite_stmt);
	_media_thumb_db_disconnect();

	return MEDIA_THUMB_ERROR_NONE;
}

int _thumb_daemon_process_queue_jobs()
{
	int err = -1;
	char *path = NULL;

	if (g_cur_idx < g_idx) {
		thumb_dbg("There are %d jobs in the queue", g_idx - g_cur_idx);
		thumb_dbg("Current idx : [%d]", g_cur_idx);
		path = arr_path[g_cur_idx++];

		thumbMsg recv_msg, res_msg;
		memset(&recv_msg, 0x00, sizeof(thumbMsg));
		memset(&res_msg, 0x00, sizeof(thumbMsg));

		recv_msg.msg_type = THUMB_REQUEST_DB_INSERT;
		recv_msg.thumb_type = MEDIA_THUMB_LARGE;
		strncpy(recv_msg.org_path, path, sizeof(recv_msg.org_path));

		_thumb_daemon_process_job(&recv_msg, &res_msg);

		if (res_msg.status == THUMB_SUCCESS) {
			err = _media_thumb_db_connect();
			if (err < 0) {
				thumb_err("_media_thumb_mb_svc_connect failed: %d", err);
				return MEDIA_THUMB_ERROR_DB;
			}

			/* Need to update DB once generating thumb is done */
			err = _media_thumb_update_db(recv_msg.org_path,
										res_msg.dst_path,
										res_msg.origin_width,
										res_msg.origin_height);
			if (err < 0) {
				thumb_err("_media_thumb_update_db failed : %d", err);
			}

			_media_thumb_db_disconnect();
		}

		free(path);
		path = NULL;
		g_server_mode = TIMEOUT_MODE;
	} else {
		g_cur_idx = 0;
		g_idx = 0;
		g_server_mode = BLOCK_MODE;
		thumb_warn("Deleting array");
		free(arr_path);
	}

	return MEDIA_THUMB_ERROR_NONE;
}

gboolean _thumb_daemon_udp_thread(void *data)
{
	int err = -1;
	gboolean is_timeout = FALSE;

	struct sockaddr_in serv_addr;
	struct sockaddr_in client_addr;
	unsigned short serv_port;
	unsigned int client_addr_len;

	thumbMsg recv_msg;
	thumbMsg res_msg;
	int recv_msg_size;
	char thumb_path[MAX_PATH_SIZE + 1];

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	serv_port = THUMB_DAEMON_PORT;

	/* Creaete a datagram/UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		thumb_err("socket failed");
		return FALSE;
	}

	memset(thumb_path, 0, sizeof(thumb_path));
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);

	/* Bind to the local address */
	if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("bind failed");
		return FALSE;
	}

	thumb_dbg("bind success");

	while(1) {
		if (g_server_mode == TIMEOUT_MODE) {
			thumb_dbg("Wait for other app's request for 10 msec");
			is_timeout = TRUE;
		} else {
			is_timeout = FALSE;
		}

		err = _thumb_daemon_recv_by_select(sock, is_timeout);

		if (err == 0) {
			/* timeout in select() */
			err = _thumb_daemon_process_queue_jobs();
			continue;

		} else if (err == -1) {
			/* error in select() */
			thumb_err("ERROR in select()");
			continue;

		} else {
			/* Socket is readable */
			client_addr_len = sizeof(client_addr);
			if ((recv_msg_size = recvfrom(sock, &recv_msg, sizeof(recv_msg), 0, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
				thumb_err("recvfrom failed\n");
				continue;
			}

			thumb_dbg("Received [%d] %s(%d) from PID(%d) \n", recv_msg.msg_type, recv_msg.org_path, strlen(recv_msg.org_path), recv_msg.pid);

			if (recv_msg.msg_type == THUMB_REQUEST_ALL_MEDIA) {
				thumb_dbg("All thumbnails are being extracted now");
				_thumb_daemon_all_extract();

				g_server_mode = TIMEOUT_MODE;
			} else {
				long start = thumb_get_debug_time();

				_thumb_daemon_process_job(&recv_msg, &res_msg);

				long end = thumb_get_debug_time();
				thumb_dbg("Time : %f (%s)\n", ((double)(end - start) / (double)CLOCKS_PER_SEC), recv_msg.org_path);
			}

			if (sendto(sock, &res_msg, sizeof(res_msg), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != sizeof(res_msg)) {
				thumb_err("sendto failed\n");
			} else {
				thumb_dbg("Sent %s(%d) \n", res_msg.dst_path, strlen(res_msg.dst_path));
			}

			memset((void *)&recv_msg, 0, sizeof(recv_msg));
			memset((void *)&res_msg, 0, sizeof(res_msg));
		}
	}

	close(sock);

	return TRUE;
}


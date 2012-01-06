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

#define LOG_TAG "Thumb-Daemon"

static int sock;
GAsyncQueue *g_req_queue = NULL;

static __thread _server_mode_e g_server_mode = BLOCK_MODE;
static __thread int g_media_svr_pid = 0;

int _thumb_daemon_get_sockfd()
{
	return sock;
}


GAsyncQueue *_thumb_daemon_get_queue()
{
	return g_req_queue;
}

void _thumb_daemon_destroy_queue_msg(queueMsg *msg)
{
	if (msg != NULL) {
		if (msg->recv_msg != NULL) {
			free(msg->recv_msg);
			msg->recv_msg = NULL;
		}

		if (msg->res_msg != NULL) {
			free(msg->res_msg);
			msg->res_msg = NULL;
		}

		msg = NULL;
	}
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
	thumb_err("");
	fd_set fds;
	int ret = -1;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (is_timeout) {
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

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
		if (req_msg->msg_type == THUMB_REQUEST_SAVE) {
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

int _thumb_daemon_process_queue_jobs()
{
	int length = 0;

	while(1) {
		length = g_async_queue_length(g_req_queue);
		if (length > 0) {
			thumb_dbg("There are %d jobs in the queue", length);
			queueMsg *data = g_async_queue_pop(g_req_queue);

			_thumb_daemon_process_job(data->recv_msg, data->res_msg);

			if (sendto(sock, data->res_msg, sizeof(thumbMsg), 0, (struct sockaddr *)data->client_addr, sizeof(struct sockaddr_in)) != sizeof(thumbMsg)) {
				thumb_err("sendto failed\n");
			} else {
				thumb_dbg("Sent %s(%d) \n", data->res_msg->dst_path, strlen(data->res_msg->dst_path));
			}

			_thumb_daemon_destroy_queue_msg(data);
		} else {
			break;
		}
	}

	g_server_mode = BLOCK_MODE;

	return 0;
}

gboolean _thumb_daemon_udp_thread(void *data)
{
	int err = -1;
	gboolean is_timeout = FALSE;

	struct sockaddr_in serv_addr;
	struct sockaddr_in client_addr;
	unsigned short serv_port;
	unsigned int client_addr_len;

	queueMsg *queue_msg = NULL;
	thumbMsg recv_msg;
	thumbMsg res_msg;
	int recv_msg_size;
	char thumb_path[MAX_PATH_SIZE + 1];

	memset((void *)&recv_msg, 0, sizeof(recv_msg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	serv_port = THUMB_DAEMON_PORT;

	if (!g_req_queue) {
		g_req_queue = g_async_queue_new();
	}

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
			thumb_dbg("Wait for other app's request for 1 sec");
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
			_pid_e pid_type = 0;
			/* Socket is readable */
			client_addr_len = sizeof(client_addr);
			if ((recv_msg_size = recvfrom(sock, &recv_msg, sizeof(recv_msg), 0, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
				thumb_err("recvfrom failed\n");
				continue;
			}

			thumb_dbg("Received [%d] %s(%d) from PID(%d) \n", recv_msg.msg_type, recv_msg.org_path, strlen(recv_msg.org_path), recv_msg.pid);

			if (g_media_svr_pid > 0) {
				pid_type = _thumb_daemon_compare_pid_with_mediaserver_fast(recv_msg.pid);
			}

			if (g_media_svr_pid <= 0) {
				pid_type = _thumb_daemon_compare_pid_with_mediaserver(recv_msg.pid);
			}

			if (pid_type == GETPID_FAIL) {
				thumb_err("Failed to get media svr pid. So This req is regarded as other app's request.");
				pid_type = OTHERS_PID;
			}

			if (g_server_mode == BLOCK_MODE || pid_type == OTHERS_PID) {
				long start = thumb_get_debug_time();

				_thumb_daemon_process_job(&recv_msg, &res_msg);

				long end = thumb_get_debug_time();
				thumb_dbg("Time : %f (%s)\n", ((double)(end - start) / (double)CLOCKS_PER_SEC), recv_msg.org_path);

				if (sendto(sock, &res_msg, sizeof(res_msg), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != sizeof(res_msg)) {
					thumb_err("sendto failed\n");
				} else {
					thumb_dbg("Sent %s(%d) \n", res_msg.dst_path, strlen(res_msg.dst_path));
				}

				memset((void *)&recv_msg, 0, sizeof(recv_msg));
				memset((void *)&res_msg, 0, sizeof(res_msg));

				if (pid_type == OTHERS_PID) {
					g_server_mode = TIMEOUT_MODE;
				}
			} else {
				/* This is a request from media-server on TIMEOUT mode.
				   So this req is going to be pushed to the queue */

				queue_msg = malloc(sizeof(queueMsg));
				if (queue_msg == NULL) {
					thumb_err("malloc failed");
					continue;
				}
				queue_msg->recv_msg = malloc(sizeof(thumbMsg));
				if (queue_msg->recv_msg == NULL) {
					thumb_err("malloc failed");
					SAFE_FREE(queue_msg);
					continue;
				}
				queue_msg->res_msg = malloc(sizeof(thumbMsg));
				if (queue_msg->res_msg == NULL) {
					thumb_err("malloc failed");
					SAFE_FREE(queue_msg->recv_msg);
					SAFE_FREE(queue_msg);
					continue;
				}
				queue_msg->client_addr = malloc(sizeof(struct sockaddr_in));
				if (queue_msg->client_addr == NULL) {
					thumb_err("malloc failed");
					SAFE_FREE(queue_msg->recv_msg);
					SAFE_FREE(queue_msg->res_msg);
					SAFE_FREE(queue_msg);
					continue;
				}

				memcpy(queue_msg->recv_msg, &recv_msg, sizeof(thumbMsg));
				memcpy(queue_msg->res_msg, &res_msg, sizeof(thumbMsg));
				memcpy(queue_msg->client_addr, &client_addr, sizeof(struct sockaddr_in));

				g_async_queue_push(g_req_queue, GINT_TO_POINTER(queue_msg));
				g_server_mode = TIMEOUT_MODE;
			}
		}
	}

	close(sock);

	return TRUE;
}


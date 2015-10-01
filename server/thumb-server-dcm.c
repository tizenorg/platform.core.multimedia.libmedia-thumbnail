/*
 * media-thumbnail-server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Lei Zhang <lei527.zhang@samsung.com>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <aul.h>
#include <fcntl.h>
#include <dirent.h>

#include "media-thumb-ipc.h"
#include "media-thumb-db.h"
#include "media-thumb-util.h"
#include "media-thumb-debug.h"

#include "thumb-server-dcm.h"

#ifdef _SUPPORT_DCM

static GMainLoop *g_dcm_thread_mainloop;
static bool b_dcm_svc_launched;
static int g_dcm_timer_id = 0;
static int g_dcm_server_pid = 0;

static char DCM_IPC_PATH[3][100] =
			{"/var/run/media-server/media_ipc_thumbdcm_dcmrecv",
			 "/var/run/media-server/media_ipc_thumbdcm_thumbrecv",
			 "/var/run/media-server/dcm_ipc_thumbserver_comm_recv"};

#define DCM_SVC_NAME "dcm-svc"
#define DCM_SVC_EXEC_NAME "/usr/bin/dcm-svc"

static gboolean g_folk_dcm_server = FALSE;
static gboolean g_dcm_server_extracting = FALSE;
static gboolean g_shutdowning_dcm_server = FALSE;
static int g_dcm_communicate_sock = 0;
static thumb_server_dcm_ipc_msg_s last_msg;

static int __thumb_server_dcm_create_socket(int *socket_fd, thumb_server_dcm_port_type_e port);
static int __thumb_server_dcm_process_recv_msg(thumb_server_dcm_ipc_msg_s *recv_msg);
void __thumb_server_dcm_create_timer(int id);

gboolean __thumb_server_dcm_agent_timer()
{
	if (g_dcm_server_extracting) {
		thumb_dbg("Timer is called.. But dcm-svc[%d] is busy.. so timer is recreated", g_dcm_server_pid);

		__thumb_server_dcm_create_timer(g_dcm_timer_id);
		return FALSE;
	}

	g_dcm_timer_id = 0;
	thumb_dbg("Timer is called.. Now killing dcm-svc[%d]", g_dcm_server_pid);

	if (g_dcm_server_pid > 0) {
		/* Command Kill to thumbnail server */
		g_shutdowning_dcm_server = TRUE;
		if (!_thumb_server_dcm_send_msg(THUMB_SERVER_DCM_MSG_STOP, 0, NULL, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV)) {
			thumb_err("_thumb_server_dcm_send_msg is failed");
			g_shutdowning_dcm_server = FALSE;
		}
		usleep(200000);
		g_dcm_communicate_sock = 0;
	} else {
		thumb_err("g_server_pid is %d. Maybe there's problem in thumbnail-server", g_dcm_server_pid);
	}

	return FALSE;
}

void __thumb_server_dcm_create_timer(int id)
{
	if (id > 0)
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), id));

	GSource *timer_src = g_timeout_source_new_seconds(MS_TIMEOUT_SEC_20);
	g_source_set_callback (timer_src, __thumb_server_dcm_agent_timer, NULL, NULL);
	g_dcm_timer_id = g_source_attach (timer_src, g_main_context_get_thread_default());

}

int __thumb_strcopy(char *res, const int size, const char *pattern, const char *str1)
{
	int len = 0;
	int real_size = size;

	if (!res || !pattern || !str1) {
		thumb_err("parameta is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (real_size < strlen(str1)) {
		thumb_err("size is wrong");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	len = snprintf(res, real_size, pattern, str1);
	if (len < 0) {
		thumb_err("snprintf failed");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res[len] = '\0';

	return MS_MEDIA_ERR_NONE;
}

/* This checks if thumbnail server is running */
bool __thumb_server_dcm_check_process()
{
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
	bool ret = FALSE;

	pdir = opendir("/proc");
	if (pdir == NULL) {
		thumb_err("err: NO_DIR");
		return 0;
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

		__thumb_strcopy(path, sizeof(path), "/proc/%s/status", pinfo.d_name);
		fp = fopen(path, "rt");
		if (fp) {
			if (fgets(buff, 128, fp) == NULL)
				thumb_err("fgets failed");
			fclose(fp);

			if (strstr(buff, DCM_SVC_NAME)) {
				thumb_err("%s proc is already running", buff);
				ret = TRUE;
				break;
			}
		} else {
			thumb_err("Can't read file [%s]", path);
		}
	}

	closedir(pdir);

	return ret;
}

/*gboolean __thumb_server_dcm_agent_recv_msg_from_server()
{
	int recv_msg_size = 0;
	thumb_server_dcm_ipc_msg_s recv_msg;

	if (g_dcm_communicate_sock <= 0) {
		if (__thumb_server_dcm_create_socket(&g_dcm_communicate_sock, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV) < 0) {
			thumb_err("ms_ipc_create_server_socket failed");
			return FALSE;
		}
	}

	if ((recv_msg_size = read(g_dcm_communicate_sock, &recv_msg, sizeof(thumb_server_dcm_ipc_msg_s))) < 0) {
		if (errno == EWOULDBLOCK) {
			thumb_err("Timeout. Can't try any more");
			return MEDIA_THUMB_ERROR_NETWORK;
		} else {
			thumb_err("recv failed : %s", strerror(errno));
			return MEDIA_THUMB_ERROR_NETWORK;
		}
	}
	dcm_sdbg("[receive msg] type: %d, msg: %s, msg_size: %d", recv_msg.msg_type, recv_msg.msg, recv_msg.msg_size);

	if (!(recv_msg.msg_type >= 0 && recv_msg.msg_type < THUMB_SERVER_DCM_MSG_MAX)) {
		thumb_err("IPC message is wrong!");
		return  MEDIA_THUMB_ERROR_NETWORK;
	}

	if (recv_msg.msg_type == THUMB_SERVER_DCM_MSG_SERVER_READY) {
		thumb_dbg("DcmSvc server is ready");
		__thumb_server_dcm_process_recv_msg(&last_msg);
	}

	return TRUE;
}*/

gboolean __thumb_server_dcm_agent_execute_server()
{
	thumb_dbg("Exec DCM_SVC: start");

	int pid;
	pid = fork();

	if (pid < 0) {
		return FALSE;
	} else if (pid == 0) {
		execl(DCM_SVC_EXEC_NAME, DCM_SVC_NAME, NULL);
	} else {
		thumb_dbg("Child process is %d", pid);
		g_folk_dcm_server = TRUE;
	}

	g_dcm_server_pid = pid;

	if (g_dcm_communicate_sock <= 0) {
		if (__thumb_server_dcm_create_socket(&g_dcm_communicate_sock, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV) < 0) {
			thumb_err("ms_ipc_create_server_socket failed");
			return FALSE;
		}
	}
/*	if (!__thumb_server_dcm_agent_recv_msg_from_server()) {
		thumb_err("_thumb_server_dcm_agent_recv_msg_from_server is failed");
		return FALSE;
	}*/

	thumb_dbg("Exec DCM_SVC: end");

	return TRUE;
}

static int __thumb_server_dcm_process_recv_msg(thumb_server_dcm_ipc_msg_s *recv_msg)
{
	DCM_CHECK_VAL(recv_msg, MS_MEDIA_ERR_INVALID_PARAMETER);
	int err = MS_MEDIA_ERR_NONE;

	if (g_shutdowning_dcm_server) {
		thumb_err("Thumb server is shutting down... wait for complete");
		usleep(10000);
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	if (g_folk_dcm_server == FALSE && g_dcm_server_extracting == FALSE) {
		if(__thumb_server_dcm_check_process() == FALSE) {  // dcm-svc is running?
			thumb_warn("Dcm-svc server is not running.. so start it");
			memcpy(&last_msg, recv_msg, sizeof(thumb_server_dcm_ipc_msg_s));
			if (!__thumb_server_dcm_agent_execute_server()) {
				thumb_err("__thumb_server_dcm_agent_execute_server is failed");
				return MS_MEDIA_ERR_SOCKET_INTERNAL;
			} else {
				__thumb_server_dcm_create_timer(g_dcm_timer_id);
				return err;
			}
		}
	} else {
		/* Timer is re-created*/
		__thumb_server_dcm_create_timer(g_dcm_timer_id);
	}

	if (recv_msg->msg_type == THUMB_SERVER_DCM_MSG_SCAN_ALL) {
		/* Launch dcm-svc application to scan all images */
		err = _thumb_server_dcm_send_msg(THUMB_SERVER_DCM_MSG_SCAN_ALL, recv_msg->uid, NULL, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV);
	} else if (recv_msg->msg_type == THUMB_SERVER_DCM_MSG_SCAN_SINGLE) {
		/* Launch dcm-svc application to scan a single image */
		if (recv_msg->msg_size > 0) {
			char *file_path = strdup(recv_msg->msg);
			if (file_path == NULL) {
				thumb_err("Failed to get file path!");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}

			thumb_warn("Send socket message to dcm-svc");
			err = _thumb_server_dcm_send_msg(THUMB_SERVER_DCM_MSG_SCAN_SINGLE, recv_msg->uid, (const char *)file_path, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV);
			SAFE_FREE(file_path);
		} else {
			thumb_err("Invalid receive message!");
			err = MS_MEDIA_ERR_SOCKET_INTERNAL;
		}
	} else if (recv_msg->msg_type == THUMB_SERVER_DCM_MSG_SERVER_READY) {
		thumb_warn("Recieve ready message from dcm-svc, send message to dcm-svc again");
		err = __thumb_server_dcm_process_recv_msg(&last_msg);
		//memset(&last_msg, 0, sizeof(thumb_server_dcm_ipc_msg_s));
	} else if (recv_msg->msg_type == THUMB_SERVER_DCM_MSG_STOP) {
		err = _thumb_server_dcm_send_msg(THUMB_SERVER_DCM_MSG_STOP, 0, NULL, THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV);
		__thumb_server_dcm_create_timer(g_dcm_timer_id);
	} else {
		thumb_err("Invalid message type!");
		err = MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	if (g_dcm_communicate_sock > 0) { // THUMB_REQUEST_ALL_MEDIA
		g_dcm_server_extracting = TRUE;
	}

	return err;
}

static int __thumb_server_dcm_receive_tcp_msg(int client_sock, thumb_server_dcm_ipc_msg_s *recv_msg)
{
	int recv_msg_size = 0;

	if ((recv_msg_size = read(client_sock, recv_msg, sizeof(thumb_server_dcm_ipc_msg_s))) < 0) {
		if (errno == EWOULDBLOCK) {
			thumb_err("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		} else {
			thumb_err("recv failed : %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	dcm_sdbg("tomoryu __thumb_server_dcm_receive_tcp_msg() uid : %d, sock : %d", recv_msg->uid, client_sock);
	dcm_sdbg("[receive msg] type: %d, msg: %s, msg_size: %d", recv_msg->msg_type, recv_msg->msg, recv_msg->msg_size);

	if (!(recv_msg->msg_type >= 0 && recv_msg->msg_type < THUMB_SERVER_DCM_MSG_MAX)) {
		thumb_err("IPC message is wrong!");
		return  MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	return MS_MEDIA_ERR_NONE;
}

static int __thumb_server_dcm_accept_tcp_socket(int serv_sock, int* client_sock)
{
	DCM_CHECK_VAL(client_sock, MS_MEDIA_ERR_INVALID_PARAMETER);
	int sockfd = -1;
	struct sockaddr_un client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	if ((sockfd = accept(serv_sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		thumb_err("accept failed : %s", strerror(errno));
		*client_sock  = -1;
		return MS_MEDIA_ERR_SOCKET_ACCEPT;
	}

	*client_sock  = sockfd;

	return MS_MEDIA_ERR_NONE;
}

static gboolean __thumb_server_dcm_read_dcm_recv_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	int sock = -1;
	int client_sock = -1;
	thumb_server_dcm_ipc_msg_s recv_msg;
	int err = 0;

	/* Get socket fd from IO channel */
	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		thumb_err("Invalid socket fd!");
		return TRUE;
	}

	/* Accept tcp client socket */
	err = __thumb_server_dcm_accept_tcp_socket(sock, &client_sock);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("Failed to accept tcp socket! err: %d", err);
		return TRUE;
	}

	/* Receive message from tcp socket */
	err = __thumb_server_dcm_receive_tcp_msg(client_sock, &recv_msg);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("Failed to receive tcp msg! err: %d", err);
		goto DCM_IPC_READ_TCP_SOCKET_FAILED;
	}

	/* Process received message */
	err = __thumb_server_dcm_process_recv_msg(&recv_msg);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("Error ocurred when process received message: %d", err);
		goto DCM_IPC_READ_TCP_SOCKET_FAILED;
	}

DCM_IPC_READ_TCP_SOCKET_FAILED:

	if (close(client_sock) < 0) {
		thumb_err("close failed [%s]", strerror(errno));
	}

	return TRUE;
}

static int __thumb_server_dcm_create_socket(int *socket_fd, thumb_server_dcm_port_type_e port)
{
	DCM_CHECK_VAL(socket_fd, MS_MEDIA_ERR_INVALID_PARAMETER);
	int sock = -1;
	struct sockaddr_un serv_addr;
	bool bind_success = false;
	int i = 0;

	/* Create a new TCP socket */
	if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
		thumb_err("socket failed: %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Set socket address */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	unlink(DCM_IPC_PATH[port]);
	strcpy(serv_addr.sun_path, DCM_IPC_PATH[port]);

	/* Bind socket to local address */
	for (i = 0; i < 20; i ++) {
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
			bind_success = true;
			break;
		}
		thumb_dbg("#%d bind",i);
		usleep(250000);
	}

	if (bind_success == false) {
		thumb_err("bind failed : %s %d_", strerror(errno), errno);
		close(sock);
		return MS_MEDIA_ERR_SOCKET_BIND;
	}
	thumb_dbg("bind success");

	/* Listenint */
	if (listen(sock, SOMAXCONN) < 0) {
		thumb_err("listen failed : %s", strerror(errno));
		close(sock);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}
	thumb_dbg("Listening...");

	/* change permission of local socket file */
	if (chmod(DCM_IPC_PATH[port], 0660) < 0)
		thumb_err("chmod failed [%s]", strerror(errno));
	if (chown(DCM_IPC_PATH[port], 0, 5000) < 0)
		thumb_err("chown failed [%s]", strerror(errno));

	*socket_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

gboolean _thumb_server_dcm_thread(void *data)
{
	int socket_fd = -1;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;
	int err = 0;

	thumb_err( "_thumb_server_dcm_thread!S");


	/* Create TCP Socket to receive message from thumb server main thread */
	err = __thumb_server_dcm_create_socket(&socket_fd, THUMB_SERVER_DCM_PORT_DCM_RECV);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("Failed to create socket! err: %d", err);
		return FALSE;
	}
	dcm_sdbgW("dcm recv socket: %d", socket_fd);

	/* Create a new main context for dcm thread */
	context = g_main_context_new();

	/* Create a new main event loop */
	g_dcm_thread_mainloop = g_main_loop_new(context, FALSE);

	/* Create a new channel to watch TCP socket */
	channel = g_io_channel_unix_new(socket_fd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Attach channel to main context in dcm thread */
	g_source_set_callback(source, (GSourceFunc)__thumb_server_dcm_read_dcm_recv_socket, NULL, NULL);
	g_source_attach(source, context);

	/* Push main context to dcm thread */
	g_main_context_push_thread_default(context);

	/* dcm-svc is launched by service_send_launch_request the first time */
	b_dcm_svc_launched = false;

	thumb_dbg("********************************************");
	thumb_dbg("*** Thumb Server DCM thread is running   ***");
	thumb_dbg("********************************************");

	/* Start to run main event loop for dcm thread */
	g_main_loop_run(g_dcm_thread_mainloop);

	thumb_dbg("*** Thumb Server DCM thread will be closed ***");

	/* Destroy IO channel */
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);

	/* Close the TCP socket */
	close(socket_fd);

	/* Descrease the reference count of main loop of dcm thread */
	g_main_loop_unref(g_dcm_thread_mainloop);

	return FALSE;
}

int _thumb_server_dcm_send_msg(thumb_server_dcm_ipc_msg_type_e msg_type, uid_t uid, const char *msg, thumb_server_dcm_port_type_e port)
{
	if (port < 0 || port >= THUMB_SERVER_DCM_PORT_MAX) {
		thumb_err("Invalid port! Stop sending message...");
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	thumb_err( "_thumb_server_dcm_send_msg to %d ", port);

	int socket_fd = -1;
	struct sockaddr_un serv_addr;
	//struct timeval tv_timeout = { 10, 0 }; /* timeout: 10 seconds */
	thumb_server_dcm_ipc_msg_s send_msg;

	/* Prepare send message */
	memset((void *)&send_msg, 0, sizeof(thumb_server_dcm_ipc_msg_s));
	send_msg.uid = uid;
	send_msg.msg_type = msg_type;
	if (msg != NULL) {
		send_msg.msg_size = strlen(msg);
		strncpy(send_msg.msg, msg, send_msg.msg_size);
	}

	/* Create a new TCP socket */
	if ((socket_fd = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
		thumb_err("socket failed: %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Set dcm thread socket address */
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, DCM_IPC_PATH[port]);

	/* Connect to the socket */
	if (connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		thumb_err("connect error : %s", strerror(errno));
		close(socket_fd);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Send msg to the socket */
	if (send(socket_fd, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
		thumb_err("send failed : %s", strerror(errno));
		close(socket_fd);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	if (msg_type >= THUMB_SERVER_DCM_MSG_STOP && g_dcm_communicate_sock > 0) { // THUMB_REQUEST_ALL_MEDIA
		g_dcm_server_extracting = TRUE;
	}

	close(socket_fd);
	return MS_MEDIA_ERR_NONE;
}

void _thumb_server_dcm_quit_main_loop()
{
	if (g_dcm_thread_mainloop != NULL) {
		thumb_warn("Quit DCM thread main loop!");
		g_main_loop_quit(g_dcm_thread_mainloop);
	} else {
		thumb_err("Invalid DCM thread main loop!");
	}
}

int thumb_server_dcm_get_server_pid()
{
	return g_dcm_server_pid;
}

void thumb_server_dcm_reset_server_status()
{
	g_folk_dcm_server = FALSE;
	g_shutdowning_dcm_server = FALSE;

	if (g_dcm_timer_id > 0) {
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), g_dcm_timer_id));
		g_dcm_timer_id = 0;
	}

	if (g_dcm_server_extracting) {
		/* Need to inplement when crash happens */
		thumb_err("Thumbnail server is dead when processing all-thumbs extraction");
		g_dcm_server_extracting = FALSE;
		g_dcm_server_pid = 0;
	} else {
		g_dcm_server_extracting = FALSE;
		g_dcm_server_pid = 0;
	}

	return;
}
#endif /* _SUPPORT_DCM */

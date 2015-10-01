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

#ifndef _THUMB_SERVER_DCM_H_
#define _THUMB_SERVER_DCM_H_

#include <glib.h>
#include "media-thumb-ipc.h"

typedef enum {
	THUMB_SERVER_DCM_MSG_NONE = 0,
	THUMB_SERVER_DCM_MSG_SERVER_READY,
	THUMB_SERVER_DCM_MSG_STOP,
	THUMB_SERVER_DCM_MSG_SCAN_READY,  /* NOT_USED */
	THUMB_SERVER_DCM_MSG_SCAN_ALL,
	THUMB_SERVER_DCM_MSG_SCAN_SINGLE,
	THUMB_SERVER_DCM_MSG_MAX,
} thumb_server_dcm_ipc_msg_type_e;

typedef enum {
	THUMB_SERVER_DCM_PORT_DCM_RECV,
	THUMB_SERVER_DCM_PORT_THUMB_RECV,
	THUMB_SERVER_DCM_PORT_DCM_SVC_APP_RECV,
	THUMB_SERVER_DCM_PORT_MAX,
} thumb_server_dcm_port_type_e;

typedef struct {
	thumb_server_dcm_ipc_msg_type_e msg_type;
	uid_t uid;
	size_t msg_size; /*this is size of message below and this does not include the terminationg null byte ('\0'). */
	char msg[MAX_PATH_SIZE];
} thumb_server_dcm_ipc_msg_s;

gboolean _thumb_server_dcm_thread(void *data);
int _thumb_server_dcm_send_msg(thumb_server_dcm_ipc_msg_type_e msg_type, uid_t uid, const char *msg, thumb_server_dcm_port_type_e port);
void _thumb_server_dcm_quit_main_loop();
int thumb_server_dcm_get_server_pid();
void thumb_server_dcm_reset_server_status();

#endif /*_THUMB_SERVER_DCM_H_*/

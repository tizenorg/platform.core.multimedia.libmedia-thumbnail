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

#include <glib.h>
#include <vconf.h>
#include "media-thumb-ipc.h"
#include "media-thumb-db.h"

#ifndef _THUMB_DAEMON_INTERNAL_H_
#define _THUMB_DAEMON_INTERNAL_H_

typedef enum {
	MEDIA_SERVER_PID = 1,
	OTHERS_PID = 0,
	GETPID_FAIL = -1
} _pid_e;

typedef enum {
	BLOCK_MODE = 0,
	TIMEOUT_MODE = 1
} _server_mode_e;

gboolean _thumb_daemon_start_jobs(gpointer data);
void _thumb_daemon_finish_jobs(void);
void _thumb_daemon_mmc_eject_vconf_cb(void *data);
void _thumb_daemon_vconf_cb(void *data);
gboolean _thumb_server_prepare_socket(int *sock_fd);
gboolean _thumb_server_read_socket(GIOChannel *src, GIOCondition condition, gpointer data);

int _thumbnail_get_data(const char *origin_path,
						media_thumb_format format,
						char *thumb_path,
						unsigned char **data,
						int *size,
						int *width,
						int *height,
						int *origin_width,
						int *origin_height,
						int *alpha,
						bool *is_saved);

int _thumbnail_get_raw_data(const char *origin_path,
						media_thumb_format format,
						int *width,
						int *height,
						unsigned char **data,
						int *size);

int _media_thumb_process(thumbMsg *req_msg, thumbMsg *res_msg);

int _media_thumb_process_raw(thumbMsg *req_msg, thumbMsg *res_msg);

#endif /*_THUMB_DAEMON_INTERNAL_H_*/

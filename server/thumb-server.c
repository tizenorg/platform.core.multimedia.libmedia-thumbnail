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

#include "media-thumbnail.h"
#include "media-thumb-debug.h"
#include "media-thumb-ipc.h"
#include "media-thumb-util.h"
#include "thumb-server-internal.h"

#include <pthread.h>
#include <heynoti.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "Thumb-Daemon"

#ifdef USE_HIB
#undef USE_HIB
#endif

#ifdef USE_HIB
int hib_fd = 0;

void _hibernation_leave_callback(void *data)
{
	thumb_dbg("hibernation leave callback\n");

	GMainLoop *mainloop = NULL;
	GThread *udp_thr = NULL;

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}
	
	udp_thr = g_thread_create((GThreadFunc)_thumb_daemon_udp_thread, NULL, FALSE, NULL);
	mainloop = g_main_loop_new(NULL, FALSE);

	thumb_dbg("*****************************************");
	thumb_dbg("*** Server of thumbnail is running ***");
	thumb_dbg("*****************************************");

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);

	exit(0);
}

void _hibernation_enter_callback(void *data)
{
	thumb_dbg("hibernation enter callback\n");

	int sock = _thumb_daemon_get_sockfd();
	if (sock != 0) {
		close(sock);
	}

	GAsyncQueue *req_queue = _thumb_daemon_get_queue();

	if (req_queue != NULL) {
		g_async_queue_unref(req_queue);
		req_queue = NULL;
	}

	if (hib_fd != 0) {
		heynoti_close(hib_fd);
		hib_fd = 0;
	}
}

void _hibernation_initialize(void)
{
	hib_fd = heynoti_init();
	heynoti_subscribe(hib_fd, "HIBERNATION_ENTER",
			  _hibernation_enter_callback, (void *)hib_fd);
	heynoti_subscribe(hib_fd, "HIBERNATION_LEAVE",
			  _hibernation_leave_callback, (void *)hib_fd);
	heynoti_attach_handler(hib_fd);

	return;
}

void _hibernation_fianalize(void)
{
	heynoti_close(hib_fd);
}
#endif

int main()
{
#ifdef USE_HIB
	_hibernation_initialize();
#endif

	GMainLoop *mainloop = NULL;
	GThread *udp_thr = NULL;
	
	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}
	
	udp_thr = g_thread_create((GThreadFunc)_thumb_daemon_udp_thread, NULL, FALSE, NULL);
	mainloop = g_main_loop_new(NULL, FALSE);

	thumb_dbg("*****************************************");
	thumb_dbg("*** Server of thumbnail is running ***");
	thumb_dbg("*****************************************");

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);

#ifdef USE_HIB
	_hibernation_fianalize();
#endif

	return 0;
}


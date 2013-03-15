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
#include <vconf.h>
//#include <signal.h>
//#include <glib-unix.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_THUMBNAIL_SERVER"
#define POWEROFF_NOTI_NAME "power_off_start" /*poeroff noti from system-server*/


extern GMainLoop *g_thumb_server_mainloop;

static void _media_thumb_signal_handler(void *user_data)
{
	thumb_dbg("Singal Hander for HEYNOTI \"power_off_start\"");

	if (g_thumb_server_mainloop)
		g_main_loop_quit(g_thumb_server_mainloop);
	else 
		exit(1);

	return;
}


int main()
{
	int sockfd = -1;

    GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	/*heynoti for power off*/
	int err = 0;
	int heynoti_id = heynoti_init();

	if (heynoti_id < 0) {
		thumb_err("heynoti_init failed");
	} else {
		err = heynoti_subscribe(heynoti_id, POWEROFF_NOTI_NAME, _media_thumb_signal_handler, NULL);
		if (err < 0) {
			thumb_err("heynoti_attach_handler failed: %d", err);
		} else {
			err = heynoti_attach_handler(heynoti_id);
			if (err < 0)
				thumb_err("heynoti_attach_handler failed: %d", err);
		}
	}

	/* Set VCONFKEY_SYSMAN_MMC_FORMAT callback to get noti for SD card format */
	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_FORMAT, (vconf_callback_fn) _thumb_daemon_vconf_cb, NULL);
	if (err == -1)
		thumb_err("vconf_notify_key_changed %s fails", VCONFKEY_SYSMAN_MMC_FORMAT);

	/* Create and bind new UDP socket */
	if (!_thumb_server_prepare_socket(&sockfd)) {
		thumb_err("Failed to create socket\n");
		return -1;
	}

	g_thumb_server_mainloop = g_main_loop_new(context, FALSE);
	context = g_main_loop_get_context(g_thumb_server_mainloop);

	/* Create new channel to watch udp socket */
	channel = g_io_channel_unix_new(sockfd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Set callback to be called when socket is readable */
	g_source_set_callback(source, (GSourceFunc)_thumb_server_read_socket, NULL, NULL);
	g_source_attach(source, context);

	GSource *source_evas_init = NULL;
	source_evas_init = g_idle_source_new ();
	g_source_set_callback (source_evas_init, _thumb_daemon_start_jobs, NULL, NULL);
	g_source_attach (source_evas_init, context);

/*	Would be used when glib 2.32 is installed
	GSource *sig_handler_src = NULL;
	sig_handler_src = g_unix_signal_source_new (SIGTERM);
	g_source_set_callback(sig_handler_src, (GSourceFunc)_media_thumb_signal_handler, NULL, NULL);
	g_source_attach(sig_handler_src, context);
*/
	thumb_dbg("************************************");
	thumb_dbg("*** Thumbnail server is running ***");
	thumb_dbg("************************************");

	g_main_loop_run(g_thumb_server_mainloop);

	thumb_dbg("Thumbnail server is shutting down...");
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);
	_thumb_daemon_finish_jobs();
	g_main_loop_unref(g_thumb_server_mainloop);

	return 0;
}

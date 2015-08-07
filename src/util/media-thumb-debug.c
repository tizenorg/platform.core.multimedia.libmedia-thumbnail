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

#include <unistd.h>
#include <asm/unistd.h>
#include <time.h>
#include <sys/time.h>
#include "media-thumb-debug.h"

#ifdef _PERFORMANCE_CHECK_
static long g_time_usec = 0L;


#ifdef _USE_LOG_FILE_
static FILE *g_log_fp = NULL;
static char _g_file_path[1024] = "\0";

FILE *get_fp()
{
	return g_log_fp;
}

void thumb_init_file_debug()
{
	if (g_log_fp == NULL) {
		snprintf(_g_file_path, sizeof(_g_file_path), "/tmp/%s",
			 "media-thumb.log");
		if (access(_g_file_path, R_OK == 0)) {
			remove(_g_file_path);
		}

		g_log_fp = fopen(_g_file_path, "a");
	}
}

void thumb_close_file_debug()
{
	if (g_log_fp != NULL) {
		fclose(g_log_fp);
		g_log_fp = NULL;
	}
}

#endif

long thumb_get_debug_time(void)
{
#ifdef _PERFORMANCE_CHECK_
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec * 1000000 + time.tv_usec;
#else
	return 0L;
#endif
}

void thumb_reset_debug_time(void)
{
#ifdef _PERFORMANCE_CHECK_
	struct timeval time;
	gettimeofday(&time, NULL);
	g_time_usec = time.tv_sec * 1000000 + time.tv_usec;
#endif
}

void thumb_print_debug_time(char *time_string)
{
#ifdef _PERFORMANCE_CHECK_
	struct timeval time;
	double totaltime = 0.0;

	gettimeofday(&time, NULL);
	totaltime =
	    (double)(time.tv_sec * 1000000 + time.tv_usec -
		     g_time_usec) / CLOCKS_PER_SEC;

	thumb_dbg("time [%s] : %f", time_string, totaltime);
#endif
}

void
thumb_print_debug_time_ex(long start, long end, const char *func_name,
			      char *time_string)
{
#ifdef _PERFORMANCE_CHECK_
	double totaltime = 0.0;

	totaltime = (double)(end - start) / CLOCKS_PER_SEC;

	thumb_dbg("time [%s: %s] : %f", func_name, time_string,
		      totaltime);
#endif
}
#endif
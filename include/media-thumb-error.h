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



#ifndef _MEDIA_THUMB_ERROR_H_
#define _MEDIA_THUMB_ERROR_H_

/**
	@addtogroup THUMB_GENERATE
	 @{
	 * @file		media-thumb-error.h
	 * @brief	This file defines error codes for thumbnail generation.

 */

/**
        @defgroup THUMB_GENERAE_ERROR  error code
        @{

        @par
         error code
 */


#define MEDIA_THUMB_ERROR_NONE						0		/* No Error */
#define MEDIA_THUMB_ERROR_INVALID_PARAMETER			-1		/* invalid parameter(s) */
#define MEDIA_THUMB_ERROR_TOO_BIG						-2		/* Original is too big to make thumb */
#define MEDIA_THUMB_ERROR_MM_UTIL						-3		/* Error in mm-util lib */
#define MEDIA_THUMB_ERROR_UNSUPPORTED					-4		/* Unsupported type */
#define MEDIA_THUMB_ERROR_NETWORK						-5		/* Error in socket */
#define MEDIA_THUMB_ERROR_DB							-6		/* Timeout */
#define MEDIA_THUMB_ERROR_TIMEOUT						-7		/* Timeout */
#define MEDIA_THUMB_ERROR_HASHCODE						-8		/* Failed to generate hash code */

/**
	@}
*/

/**
	@}
*/

#endif /*_MEDIA_THUMB_ERROR_H_*/

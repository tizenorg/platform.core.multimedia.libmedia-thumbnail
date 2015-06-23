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

#include "md5.h"
#include <string.h>
#include <alloca.h>
#include <media-util-err.h>


static const char ACCEPTABLE_URI_CHARS[96] = {
	/*      !    "    #    $    %    &    '    (    )    *    +    ,    -    .    / */
	0x00, 0x3F, 0x20, 0x20, 0x28, 0x00, 0x2C, 0x3F, 0x3F, 0x3F, 0x3F, 0x2A,
	    0x28, 0x3F, 0x3F, 0x1C,
	/* 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ? */
	0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x38, 0x20,
	    0x20, 0x2C, 0x20, 0x20,
	/* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
	0x38, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
	    0x3F, 0x3F, 0x3F, 0x3F,
	/* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
	0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20,
	    0x20, 0x20, 0x20, 0x3F,
	/* `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o */
	0x20, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
	    0x3F, 0x3F, 0x3F, 0x3F,
	/* p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~  DEL */
	0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20,
	    0x20, 0x20, 0x3F, 0x20
};

char *_media_thumb_generate_hash_name(const char *file)
{
	int n;
	MD5_CTX ctx;
	static char md5out[(2 * MD5_HASHBYTES) + 1];
	unsigned char hash[MD5_HASHBYTES];
	static const char hex[] = "0123456789abcdef";

	char *uri;
	char *t;
	const unsigned char *c;
	int length;

	if (!file) {
		return NULL;
	}

	length = 3 * strlen(file) + 9;

	memset(md5out, 0, sizeof(md5out));

#define _check_uri_char(c) \
((c) >= 32 && (c) < 128 && (ACCEPTABLE_URI_CHARS[(c) - 32] & 0x08))

	uri = alloca(length);
	if (uri == NULL) {
		return NULL;
	}

	strncpy(uri, "file://", length);
	uri[length - 1] = '\0';
	t = uri + sizeof("file://") - 1;

	for (c = (const unsigned char *)file; *c != '\0'; c++) {
		if (!_check_uri_char(*c)) {
			*t++ = '%';
			*t++ = hex[*c >> 4];
			*t++ = hex[*c & 15];
		} else {
			*t++ = *c;
		}
	}
	*t = '\0';
#undef _check_uri_char

	memset(&ctx, 0x00, sizeof(MD5_CTX));

	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char const *)uri, (unsigned)strlen(uri));
	MD5Final(hash, &ctx);

	for (n = 0; n < MD5_HASHBYTES; n++) {
		md5out[2 * n] = hex[hash[n] >> 4];
		md5out[2 * n + 1] = hex[hash[n] & 0x0f];
	}
	md5out[2 * n] = '\0';

	return md5out;
}
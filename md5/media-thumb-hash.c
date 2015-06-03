/* GLIB - Library of useful routines for C programming
 *
 * gconvert.c: Convert between character sets using iconv
 * Copyright Red Hat Inc., 2000
 * Authors: Havoc Pennington <hp@redhat.com>, Owen Taylor <otaylor@redhat.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* The array below is taken from gconvert.c, which is licensed by GNU Lesser General Public License
 * Code to escape string is also taken partially from gconvert.c
 * File name is changed to media-thumb-hash.c
 */

#include "md5.h"
#include "media-thumbnail-private.h"
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

int thumbnail_generate_hash_code(const char *origin_path, char *hash_code, int max_length)
{
	char *hash = NULL;

	if (max_length < ((2 * MD5_HASHBYTES) + 1)) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	hash = _media_thumb_generate_hash_name(origin_path);

	if (hash == NULL) {
		return MS_MEDIA_ERR_INTERNAL;
	}

	strncpy(hash_code, hash, max_length);
	hash_code[strlen(hash_code)] ='\0';

	return MS_MEDIA_ERR_NONE;
}


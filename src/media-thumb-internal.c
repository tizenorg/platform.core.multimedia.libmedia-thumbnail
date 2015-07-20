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

#include "media-thumb-debug.h"
#include "media-thumb-util.h"
#include "media-thumb-internal.h"
#include "media-thumb-ipc.h"

#include "AGifFrameInfo.h"
#include "IfegDecodeAGIF.h"
#include "img-codec.h"
#include "img-codec-agif.h"
#include "img-codec-common.h"
#include "img-codec-osal.h"
#include "img-codec-parser.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <mm_file.h>
#include <mm_error.h>
#include <mm_util_imgp.h>
#include <mm_util_jpeg.h>
#include <libexif/exif-data.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define MEDA_THUMB_ROUND_UP_4(num)  (((num)+3)&~3)

int _media_thumb_get_proper_thumb_size(media_thumb_type thumb_type,
								int orig_w, int orig_h,
								int *thumb_w, int *thumb_h)
{
	thumb_dbg("orig w: %d orig h: %d thumb type: %d", orig_w, orig_h, thumb_type);

	BOOL portrait = FALSE;
	double ratio;

	if (orig_w < orig_h) {
		portrait = TRUE;
	}

	/* Set smaller length to default size */
	if (portrait) {
		if (orig_w < _media_thumb_get_width(thumb_type)) {
			*thumb_w = orig_w;
		} else {
			*thumb_w = _media_thumb_get_width(thumb_type);
		}

		ratio = (double)orig_h / (double)orig_w;
		*thumb_h = *thumb_w * ratio;

	} else {

		if (orig_h < _media_thumb_get_width(thumb_type)) {
			*thumb_h = orig_h;
		} else {
			*thumb_h = _media_thumb_get_width(thumb_type);
		}

		ratio = (double)orig_w / (double)orig_h;
		*thumb_w = *thumb_h * ratio;
	}

	/* The width of RGB888 raw data has to be rounded by 4 */
	*thumb_w = MEDA_THUMB_ROUND_UP_4(*thumb_w);

	thumb_dbg("proper thumb w: %d h: %d", *thumb_w, *thumb_h);

	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_get_exif_info(ExifData *ed, char *buf, int max_size, int *value,
		     int ifdtype, long tagtype)
{
	ExifEntry *entry;
	ExifIfd ifd;
	ExifTag tag;

	if (ed == NULL) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	ifd = ifdtype;
	tag = tagtype;

	entry = exif_content_get_entry(ed->ifd[ifd], tag);
	if (entry) {
		if (tag == EXIF_TAG_ORIENTATION ||
				tag == EXIF_TAG_PIXEL_X_DIMENSION ||
				tag == EXIF_TAG_PIXEL_Y_DIMENSION) {

			if (value == NULL) {
				thumb_err("value is NULL");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}

			ExifByteOrder mByteOrder = exif_data_get_byte_order(ed);
			short exif_value = exif_get_short(entry->data, mByteOrder);
			thumb_dbg("%s : %d", exif_tag_get_name_in_ifd(tag,ifd), exif_value);
			*value = (int)exif_value;
		} else {
			/* Get the contents of the tag in human-readable form */
			if (buf == NULL) {
				thumb_err("buf is NULL");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}
			exif_entry_get_value(entry, buf, max_size);
			buf[strlen(buf)] = '\0';

			if (*buf) {
				thumb_dbg("%s: %s\n", exif_tag_get_name_in_ifd(tag, ifd), buf);
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_get_thumb_from_exif(ExifData *ed,
								const char *file_full_path,
								int orientation,
								int required_width,
								int required_height,
								media_thumb_info *thumb_info, uid_t uid)
{
	ExifEntry *entry;
	ExifIfd ifd;
	ExifTag tag;

	int err = -1;
	int size = 0;
	int thumb_width = 0;
	int thumb_height = 0;
	void *thumb = NULL;

	if (ed == NULL) {
		return -1;
	}

	ExifByteOrder byte_order = exif_data_get_byte_order(ed);

	ifd = EXIF_IFD_1;
	tag = EXIF_TAG_COMPRESSION;

	entry = exif_content_get_entry(ed->ifd[ifd], tag);

	if (entry) {
		/* Get the contents of the tag in human-readable form */
		ExifShort value = exif_get_short(entry->data, byte_order);
		//thumb_dbg("%s: %d", exif_tag_get_name_in_ifd(tag,ifd), value);

		if (value == 6) {
			thumb_dbg("There's jpeg thumb in this image");
		} else {
			thumb_dbg("There's NO jpeg thumb in this image");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}
	} else {
		thumb_dbg("entry is NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	/* Get width and height of thumbnail */
	tag = EXIF_TAG_IMAGE_WIDTH;
	entry = exif_content_get_entry(ed->ifd[ifd], tag);

	if (entry) {
		/* Get the contents of the tag in human-readable form */
		ExifShort value = exif_get_short(entry->data, byte_order);
		//thumb_dbg("%s: %d\n", exif_tag_get_name_in_ifd(tag,ifd), value);

		thumb_width = value;
	}

	tag = EXIF_TAG_IMAGE_LENGTH;
	entry = exif_content_get_entry(ed->ifd[ifd], tag);
	if (entry) {
		/* Get the contents of the tag in human-readable form */
		ExifShort value = exif_get_short(entry->data, byte_order);
		//thumb_dbg("%s: %d\n", exif_tag_get_name_in_ifd(tag,ifd), value);

		thumb_height = value;
	}

	if (ed->data && ed->size) {
		//thumb_dbg("Size: %d, thumb: 0x%x\n", ed->size, ed->data);
		thumb = (char *)malloc(ed->size);

		if (thumb == NULL) { 
			thumb_dbg("malloc failed!");
			return -1;
		}

		memcpy(thumb, (void *)ed->data, ed->size);
		size = ed->size;
	} else {
		thumb_dbg("data is NULL");
		return -1;
	}

	/* Get width and height of original image from exif */
	ifd = EXIF_IFD_EXIF;
	tag = EXIF_TAG_PIXEL_X_DIMENSION;
	entry = exif_content_get_entry(ed->ifd[ifd], tag);

	if (entry) {
		ExifShort value = exif_get_short(entry->data, byte_order);
		//thumb_dbg("%s: %d\n", exif_tag_get_name_in_ifd(tag,ifd), value);
		thumb_info->origin_width = value;
	} else {
		thumb_dbg("entry is NULL");
	}

	tag = EXIF_TAG_PIXEL_Y_DIMENSION;
	entry = exif_content_get_entry(ed->ifd[ifd], tag);

	if (entry) {
		ExifShort value = exif_get_short(entry->data, byte_order);
		//thumb_dbg("%s: %d\n", exif_tag_get_name_in_ifd(tag,ifd), value);
		thumb_info->origin_height = value;
	} else {
		thumb_dbg("entry is NULL");
	}

	char thumb_path[1024];

	err =
	    _media_thumb_get_hash_name(file_full_path,
					     thumb_path,
					     sizeof(thumb_path), uid);
	if (err < 0) {
		thumb_dbg("_media_thumb_get_hash_name failed\n");
		SAFE_FREE(thumb);
		return err;
	}

	thumb_dbg("Thumb is :%s", thumb_path);

	int nwrite;
	int fd = open(thumb_path, O_RDWR | O_CREAT | O_EXCL | O_SYNC, 0644);
	if (fd < 0) {
		if (errno == EEXIST) {
			thumb_dbg("thumb alread exist!");
		} else {
			thumb_dbg("open failed\n");
			SAFE_FREE(thumb);
			return -1;
		}
	} else {
		nwrite = write(fd, thumb, size);
		if (nwrite < 0) {
			thumb_dbg("write failed\n");
			close(fd);
	
			SAFE_FREE(thumb);
			return -1;
		}
		close(fd);
	}

	SAFE_FREE(thumb);

	mm_util_jpeg_yuv_data decoded = {0,};

	err = mm_util_decode_from_jpeg_file(&decoded, thumb_path, MM_UTIL_JPEG_FMT_RGB888);
	if (err < 0) {
		thumb_dbg("mm_util_decode_from_jpeg_turbo_memory failed : %d", err);
		return err;
	}

	//thumb_dbg("size:%d, w:%d, h:%d\n", decoded.size, decoded.width, decoded.height);

	thumb_width = decoded.width;
	thumb_height = decoded.height;

	media_thumb_type thumb_type;
	int need_resize = 0;
	unsigned int buf_size = decoded.size;

	if (required_width == _media_thumb_get_width(MEDIA_THUMB_LARGE)) {
		thumb_type = MEDIA_THUMB_LARGE;
	} else {
		thumb_type = MEDIA_THUMB_SMALL;
	}

	if (thumb_type == MEDIA_THUMB_LARGE) {
		if (thumb_width < _media_thumb_get_width(MEDIA_THUMB_LARGE)) {
			thumb_dbg("Thumb data in exif is too small for MEDIA_THUMB_LARGE");
			SAFE_FREE(decoded.data);
			return -1;
		}
	} else {
		if (thumb_width > _media_thumb_get_width(MEDIA_THUMB_SMALL)) {
			need_resize = 1;
		}
	}

	if (need_resize == 1) {
		int resized_w = _media_thumb_get_width(MEDIA_THUMB_LARGE);
		int resized_h = _media_thumb_get_height(MEDIA_THUMB_LARGE);

		err = mm_util_get_image_size(MM_UTIL_IMG_FMT_RGB888, resized_w, resized_h, &buf_size);
		if (err < 0) {
			thumb_dbg("mm_util_get_image_size failed : %d", err);

			SAFE_FREE(decoded.data);
			return err;
		}

		//thumb_dbg("size(RGB888) : %d", buf_size);

		unsigned char *dst = (unsigned char *)malloc(buf_size);
		if (dst == NULL) {
			thumb_err("Failed to allocate memory!");
			SAFE_FREE(decoded.data);
			return -1;
		}

		if (mm_util_resize_image(decoded.data, thumb_width,
				thumb_height, MM_UTIL_IMG_FMT_RGB888,
				dst, (unsigned int *)&resized_w,
				(unsigned int *)&resized_h) < 0) {
			thumb_err("Failed to resize the thumbnails");

			SAFE_FREE(decoded.data);
			SAFE_FREE(dst);

			return -1;
		}

		SAFE_FREE(decoded.data);
		decoded.data = dst;
		decoded.width = thumb_width = resized_w;
		decoded.height = thumb_height = resized_h;
	}

	if (orientation == ROT_90 || orientation == ROT_180 || orientation == ROT_270) {
		/* Start to decode to rotate */
		unsigned char *rotated = NULL;
		unsigned int r_w = decoded.height;
		unsigned int r_h = decoded.width;
		unsigned int r_size = 0;
		mm_util_img_rotate_type rot_type = MM_UTIL_ROTATE_0;

		if (orientation == ROT_90) {
			rot_type = MM_UTIL_ROTATE_90;
		} else if (orientation == ROT_180) {
			rot_type = MM_UTIL_ROTATE_180;
			r_w = decoded.width;
			r_h = decoded.height;
		} else if (orientation == ROT_270) {
			rot_type = MM_UTIL_ROTATE_270;
		}

		err = mm_util_get_image_size(MM_UTIL_IMG_FMT_RGB888, r_w, r_h, &r_size);
		if (err < 0) {
			thumb_dbg("mm_util_get_image_size failed : %d", err);
			SAFE_FREE(decoded.data);
			return err;
		}

		//thumb_dbg("Size of Rotated : %d", r_size);

		rotated = (unsigned char *)malloc(r_size);
		err = mm_util_rotate_image(decoded.data, decoded.width, decoded.height, 
									MM_UTIL_IMG_FMT_RGB888,
									rotated, &r_w, &r_h, 
									rot_type);
	
		if (err < 0) {
			thumb_err("mm_util_rotate_image failed : %d", err);
			SAFE_FREE(decoded.data);
			SAFE_FREE(rotated);
			return err;
		} else {
			thumb_dbg("mm_util_rotate_image succeed");
		}

		SAFE_FREE(decoded.data);

		//thumb_dbg("Width : %d, Height : %d", r_w, r_h);
		thumb_info->data = rotated;
		thumb_info->size = r_size;
		thumb_info->width = r_w;
		thumb_info->height = r_h;
	} else if (orientation == NORMAL) {
		thumb_info->data = decoded.data;
		thumb_info->size = buf_size;
		thumb_info->width = thumb_width;
		thumb_info->height = thumb_height;
	} else {
		thumb_warn("Unknown orientation");
		SAFE_FREE(decoded.data);
		return -1;
	}

	return 0;
}

int _media_thumb_resize_data(unsigned char *src_data,
							int src_width,
							int src_height,
							mm_util_img_format src_format,
							media_thumb_info *thumb_info,
							int dst_width,
							int dst_height)
{
	int thumb_width = dst_width;
	int thumb_height = dst_height;
	unsigned int buf_size = 0;

	if (mm_util_get_image_size(src_format, thumb_width, thumb_height, &buf_size) < 0) {
		thumb_err("Failed to get buffer size");
		return MS_MEDIA_ERR_INTERNAL;
	}

	thumb_dbg("mm_util_get_image_size : %d", buf_size);

	unsigned char *dst = (unsigned char *)malloc(buf_size);

	if (mm_util_resize_image((unsigned char *)src_data, src_width,
			src_height, src_format,
			dst, (unsigned int *)&thumb_width,
			(unsigned int *)&thumb_height) < 0) {
		thumb_err("Failed to resize the thumbnails");

		SAFE_FREE(dst);

		return MS_MEDIA_ERR_INTERNAL;
	}

	thumb_info->size = buf_size;
	thumb_info->width = thumb_width;
	thumb_info->height = thumb_height;
	thumb_info->data = malloc(buf_size);
	memcpy(thumb_info->data, dst, buf_size);

	SAFE_FREE(dst);

	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_get_wh_with_gdk(const char *origin_path, int *width, int *height)
{

	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;

	pixbuf = gdk_pixbuf_new_from_file(origin_path, &error);
	if (error) {
		thumb_err ("Error loading image file %s",origin_path);
		g_error_free (error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	/* Get w/h of original image */
	*width = gdk_pixbuf_get_width(pixbuf);
	*height = gdk_pixbuf_get_height(pixbuf);

	return MS_MEDIA_ERR_NONE;
}

int _media_thumb_decode_with_gdk(const char *origin_path,
					int thumb_width, int thumb_height,
					media_thumb_info *thumb_info, int need_scale, int orientation)
{
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;

	pixbuf = gdk_pixbuf_new_from_file(origin_path, &error);
	if (error) {
		thumb_err ("Error loading image file %s",origin_path);
		g_error_free (error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	/* Get w/h of original image */
	int width = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);

	thumb_info->origin_width = width;
	thumb_info->origin_height = height;


	pixbuf = gdk_pixbuf_apply_embedded_orientation(pixbuf);

	int rotated_orig_w = 0;
	int rotated_orig_h = 0;

	if (orientation == ROT_90 || orientation == ROT_270) {
		rotated_orig_w = height;
		rotated_orig_h = width;
	} else {
		rotated_orig_w = width;
		rotated_orig_h = height;
	}

	int err = MS_MEDIA_ERR_NONE;
	media_thumb_type thumb_type;

	if (thumb_width == _media_thumb_get_width(MEDIA_THUMB_LARGE)) {
		thumb_type = MEDIA_THUMB_LARGE;
	} else {
		thumb_type = MEDIA_THUMB_SMALL;
	}

	err = _media_thumb_get_proper_thumb_size(thumb_type,
									rotated_orig_w, rotated_orig_h,
									&thumb_width, &thumb_height);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_get_proper_thumb_size failed: %d", err);
		g_object_unref(pixbuf);
		return err;
	}

	pixbuf = gdk_pixbuf_scale_simple(pixbuf, thumb_width, thumb_height, GDK_INTERP_NEAREST);

	unsigned int buf_size = 0;

	if (mm_util_get_image_size(MM_UTIL_IMG_FMT_BGRA8888, thumb_width,
			thumb_height, &buf_size) < 0) {
		thumb_err("Failed to get buffer size");

		g_object_unref(pixbuf);

		return MS_MEDIA_ERR_INTERNAL;
	}

	thumb_info->size = buf_size;
	thumb_info->alpha = gdk_pixbuf_get_has_alpha(pixbuf);
	thumb_info->width = thumb_width;
	thumb_info->height = thumb_height;
	thumb_info->gdkdata = gdk_pixbuf_copy(pixbuf);

	g_object_unref(pixbuf);
	return MS_MEDIA_ERR_NONE;
}

mm_util_img_format _media_thumb_get_format(media_thumb_format src_format)
{
	switch(src_format) {
		case MEDIA_THUMB_BGRA:
			return MM_UTIL_IMG_FMT_BGRA8888;
		case MEDIA_THUMB_RGB888:
			return MM_UTIL_IMG_FMT_RGB888;
		default:
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}
}

int _media_thumb_convert_data(media_thumb_info *thumb_info,
							int thumb_width,
							int thumb_height,
							mm_util_img_format src_format,
							mm_util_img_format dst_format)
{
	int err = MS_MEDIA_ERR_NONE;
	unsigned int buf_size = 0;
	unsigned char *src_data = thumb_info->data;
	unsigned char *dst_data = NULL;

	thumb_dbg("src format:%d, dst format:%d", src_format, dst_format);

	if (mm_util_get_image_size(dst_format, thumb_width, thumb_height, &buf_size) < 0) {
		thumb_err("Failed to get buffer size");
		return MS_MEDIA_ERR_INTERNAL;
	}

	thumb_dbg("mm_util_get_image_size : %d", buf_size);

	dst_data = (unsigned char *)malloc(buf_size);

	if (src_format == MM_UTIL_IMG_FMT_RGB888 &&
		dst_format == MM_UTIL_IMG_FMT_BGRA8888) {

		int i = 0, j;
		for (j = 0; j < thumb_width * 3 * thumb_height;
				j += 3) {
			dst_data[i++] = (src_data[j + 2]);
			dst_data[i++] = (src_data[j + 1]);
			dst_data[i++] = (src_data[j]);
			dst_data[i++] = 0x0;
		}

	} else {
		err = mm_util_convert_colorspace(src_data,
						thumb_width,
						thumb_height,
						src_format,
						dst_data,
						dst_format);

		if (err < 0) {
			thumb_err("Failed to change from rgb888 to argb8888 %d", err);
			SAFE_FREE(dst_data);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	SAFE_FREE(thumb_info->data);
	thumb_info->data = dst_data;
	thumb_info->size = buf_size;

	thumb_dbg("_media_thumb_convert_data success");

	return err;
}

int _media_thumb_convert_format(media_thumb_info *thumb_info,
							media_thumb_format src_format,
							media_thumb_format dst_format)
{
	int err = MS_MEDIA_ERR_NONE;

	if (src_format == dst_format) {
		//thumb_dbg("src_format == dst_format");
		return err;
	}

	mm_util_img_format src_mm_format;
	mm_util_img_format dst_mm_format;

	src_mm_format = _media_thumb_get_format(src_format);
	dst_mm_format = _media_thumb_get_format(dst_format);

	if (src_mm_format == -1 || dst_mm_format == -1) {
		thumb_err("Format is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	err = _media_thumb_convert_data(thumb_info,
					thumb_info->width,
					thumb_info->height,
					src_mm_format,
					dst_mm_format);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("media_thumb_convert_format failed : %d", err);
		return err;
	}

	return err;
}

int _media_thumb_agif(const char *origin_path,
					ImgImageInfo *image_info,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info)
{
	int err = MS_MEDIA_ERR_NONE;
	unsigned int *thumb = NULL;
	media_thumb_type thumb_type;

	thumb = ImgGetFirstFrameAGIFAtSize(origin_path, image_info);

	if (!thumb) {
		thumb_err("Frame data is NULL!!");
		return MS_MEDIA_ERR_INTERNAL;
	}

	if (thumb_width == _media_thumb_get_width(MEDIA_THUMB_LARGE)) {
		thumb_type = MEDIA_THUMB_LARGE;
	} else {
		thumb_type = MEDIA_THUMB_SMALL;
	}

	err = _media_thumb_get_proper_thumb_size(thumb_type,
									thumb_info->origin_width, thumb_info->origin_height,
									&thumb_width, &thumb_height);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_get_proper_thumb_size failed: %d", err);
		SAFE_FREE(thumb);
		return err;
	}

	err = _media_thumb_resize_data((unsigned char *)thumb,
									image_info->width,
									image_info->height,
									MM_UTIL_IMG_FMT_RGB888,
									thumb_info,
									thumb_width,
									thumb_height);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_resize_data failed: %d", err);
		SAFE_FREE(thumb);
		return err;
	}

	SAFE_FREE(thumb);

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}

	return err;
}

int _media_thumb_png(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info)
{
	int err = MS_MEDIA_ERR_NONE;
	err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, NORMAL);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("decode_with_gdk failed : %d", err);
		return err;
	}

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}

	return err;
}

int _media_thumb_bmp(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info)
{
	int err = MS_MEDIA_ERR_NONE;
	err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, NORMAL);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("decode_with_gdk failed : %d", err);
		return err;
	}

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}

	return err;
}

int _media_thumb_wbmp(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info)
{
	int err = MS_MEDIA_ERR_NONE;
	err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, NORMAL);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("decode_with_gdk failed : %d", err);
		return err;
	}

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}

	return err;
}

int _media_thumb_gif(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info)
{
	int err = MS_MEDIA_ERR_NONE;
	err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, NORMAL);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("decode_with_gdk failed : %d", err);
		return err;
	}

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}

	return err;
}

int _media_thumb_jpeg(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info,
					uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	ExifData *ed = NULL;
	int thumb_done = 0;
	int orientation = NORMAL;

	/* Load an ExifData object from an EXIF file */
	ed = exif_data_new_from_file(origin_path);

	if (ed) {
		/* First, Get orientation from exif */
		err = _media_thumb_get_exif_info(ed, NULL, 0, &orientation, EXIF_IFD_0, EXIF_TAG_ORIENTATION);

		if (err != MS_MEDIA_ERR_NONE) {
			thumb_warn("_media_thumb_get_exif_info failed");
		}

		/* Second, Get thumb from exif */
		err = _media_thumb_get_thumb_from_exif(ed, origin_path, orientation, thumb_width, thumb_height, thumb_info, uid);

		if (err != MS_MEDIA_ERR_NONE) {
			thumb_dbg("_media_thumb_get_thumb_from_exif failed");
		} else {
			thumb_done = 1;
			thumb_dbg("_media_thumb_get_thumb_from_exif succeed");

			mm_util_img_format dst_format = _media_thumb_get_format(format);

			err = _media_thumb_convert_data(thumb_info, 
							thumb_info->width, 
							thumb_info->height,
							MM_UTIL_IMG_FMT_RGB888,
							dst_format);

			if (err != MS_MEDIA_ERR_NONE) {
				thumb_err("_media_thumb_convert_data failed : %d", err);
				exif_data_unref(ed);
				return err;
			}
		}

		exif_data_unref(ed);
	}

	if (!thumb_done) {

		err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, orientation);

		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("decode_with_gdk failed : %d", err);
			return err;
		}

		err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_convert_format falied: %d", err);
			SAFE_FREE(thumb_info->data);
			return err;
		}
	}

	return err;
}

int _media_thumb_jpeg_for_raw(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info,
					uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	int orientation = NORMAL;

	err = _media_thumb_decode_with_gdk(origin_path, thumb_width, thumb_height, thumb_info, 0, orientation);

	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("decode_with_gdk failed : %d", err);
		return err;
	}
	/* Temporary process */
	thumb_info->data = gdk_pixbuf_get_pixels(thumb_info->gdkdata);

	err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
	if (err != MS_MEDIA_ERR_NONE) {
		thumb_err("_media_thumb_convert_format falied: %d", err);
		SAFE_FREE(thumb_info->data);
		return err;
	}
	return err;
}

int _media_thumb_image(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info, 
					bool is_raw,
					uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	int image_type = 0;
	int origin_w = 0;
	int origin_h = 0;
	ImgImageInfo image_info = { 0 };

	image_type = ImgGetInfoFile(origin_path, &image_info);

	thumb_info->origin_width = origin_w = image_info.width;
	thumb_info->origin_height = origin_h = image_info.height;

	thumb_dbg("image type is %d\n", image_type);

	if ((image_type != IMG_CODEC_JPEG) &&
		(origin_w * origin_h > THUMB_MAX_ALLOWED_MEM_FOR_THUMB)) {
		thumb_warn("This original image is too big");
		return MS_MEDIA_ERR_THUMB_TOO_BIG;
	}

	if (image_type == IMG_CODEC_AGIF) {
		err = _media_thumb_agif(origin_path, &image_info, thumb_width, thumb_height, format, thumb_info);
	} else if (image_type == IMG_CODEC_JPEG) {
		if(is_raw) {
			err = _media_thumb_jpeg_for_raw(origin_path, thumb_width, thumb_height, format, thumb_info, uid);
		} else {
			err = _media_thumb_jpeg(origin_path, thumb_width, thumb_height, format, thumb_info, uid);
		}
	} else if (image_type == IMG_CODEC_PNG) {
		err = _media_thumb_png(origin_path, thumb_width, thumb_height, format, thumb_info);
	} else if (image_type == IMG_CODEC_GIF) {
		err = _media_thumb_gif(origin_path, thumb_width, thumb_height, format, thumb_info);
	} else if (image_type == IMG_CODEC_BMP) {
		err = _media_thumb_bmp(origin_path, thumb_width, thumb_height, format, thumb_info);
	} else {
		char file_ext[10];
		err = _media_thumb_get_file_ext(origin_path, file_ext, sizeof(file_ext));
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_warn("_media_thumb_get_file_ext failed");
		} else {
			if (strcasecmp(file_ext, "wbmp") == 0) {
				image_type = IMG_CODEC_WBMP;
				int wbmp_width = 0;
				int wbmp_height = 0;

				err = _media_thumb_get_wh_with_gdk(origin_path, &wbmp_width, &wbmp_height);
				if (err != MS_MEDIA_ERR_NONE) {
					thumb_err("_media_thumb_get_wh_with_gdk in WBMP : %d", err);
					return err;
				}

				if (wbmp_width * wbmp_height > THUMB_MAX_ALLOWED_MEM_FOR_THUMB) {
					thumb_warn("This original image is too big");
					return MS_MEDIA_ERR_THUMB_TOO_BIG;
				}

				thumb_info->origin_width = wbmp_width;
				thumb_info->origin_height = wbmp_height;

				err = _media_thumb_wbmp(origin_path, thumb_width, thumb_height, format, thumb_info);

				return err;
			}
		}

		thumb_warn("Unsupported image type");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	return err;
}

int _media_thumb_video(const char *origin_path,
					int thumb_width,
					int thumb_height,
					media_thumb_format format,
					media_thumb_info *thumb_info,
					uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	MMHandleType content = (MMHandleType) NULL;
	void *frame = NULL;
	int video_track_num = 0;
	char *err_msg = NULL;
	int is_drm = 0;
	int size = 0;
	int width = 0;
	int height = 0;
	GdkPixbuf *pixbuf;
	bool drm_type = FALSE;

	is_drm = drm_type;
	err = mm_file_create_content_attrs(&content, origin_path);

	if (err != MM_ERROR_NONE) {
		thumb_err("mm_file_create_content_attrs fails : %d", err);
		return MS_MEDIA_ERR_INTERNAL;
	}

	err = mm_file_get_attrs(content, &err_msg, MM_FILE_CONTENT_VIDEO_TRACK_COUNT, &video_track_num, NULL);
	if (err != MM_ERROR_NONE) {
		thumb_err("mm_file_get_attrs fails : %s", err_msg);
		SAFE_FREE(err_msg);
		mm_file_destroy_content_attrs(content);
		return MS_MEDIA_ERR_INTERNAL;
	}

	/* MMF api handle both normal and DRM video */
	if (video_track_num > 0 || is_drm) {

		err = mm_file_get_attrs(content, &err_msg,
					MM_FILE_CONTENT_VIDEO_WIDTH,
					&width,
					MM_FILE_CONTENT_VIDEO_HEIGHT,
					&height,
					MM_FILE_CONTENT_VIDEO_THUMBNAIL, &frame, /* raw image is RGB888 format */
					&size, NULL);

		if (err != MM_ERROR_NONE) {
			thumb_err("mm_file_get_attrs fails : %s", err_msg);
			SAFE_FREE(err_msg);
			mm_file_destroy_content_attrs(content);
			return MS_MEDIA_ERR_INTERNAL;
		}

		thumb_dbg("video width: %d", width);
		thumb_dbg("video height: %d", height);
		thumb_dbg("thumbnail size=%d", size);
		thumb_dbg("frame = 0x%x", frame);

		if (frame == NULL || width == 0 || height == 0) {
			thumb_err("Failed to get frame data");
			mm_file_destroy_content_attrs(content);
			return MS_MEDIA_ERR_INTERNAL;
		}

		media_thumb_type thumb_type;
		if (thumb_width == _media_thumb_get_width(MEDIA_THUMB_LARGE)) {
			thumb_type = MEDIA_THUMB_LARGE;
		} else {
			thumb_type = MEDIA_THUMB_SMALL;
		}

		thumb_info->origin_width = width;
		thumb_info->origin_height = height;

		err = _media_thumb_get_proper_thumb_size(thumb_type, width, height, &thumb_width, &thumb_height);

		if (width > thumb_width || height > thumb_height) {
			err = _media_thumb_resize_data(frame,
										width,
										height,
										MM_UTIL_IMG_FMT_RGB888,
										thumb_info,
										thumb_width,
										thumb_height);

			if (err != MS_MEDIA_ERR_NONE) {
				thumb_err("_media_thumb_resize_data failed - %d", err);
				SAFE_FREE(thumb_info->data);
				mm_file_destroy_content_attrs(content);
				return err;
			}
		} else {
			thumb_info->size = size;
			thumb_info->width = width;
			thumb_info->height = height;
			thumb_info->data = malloc(size);
			memcpy(thumb_info->data, frame, size);
		}

		mm_file_destroy_content_attrs(content);

		/* Get Content Tag attribute for orientatin  */
		MMHandleType tag = (MMHandleType) NULL;
		char *p = NULL;
		int size = -1;
		err = mm_file_create_tag_attrs(&tag, origin_path);
		mm_util_img_rotate_type rot_type = MM_UTIL_ROTATE_0;

		if (err == MM_ERROR_NONE) {
			err = mm_file_get_attrs(tag, &err_msg, MM_FILE_TAG_ROTATE, &p, &size, NULL);
			if (err == MM_ERROR_NONE && size >= 0) {

				if (p == NULL) {
					rot_type = MM_UTIL_ROTATE_0;
				} else {
					if (strncmp(p, "90", size) == 0) {
						rot_type = MM_UTIL_ROTATE_90;
					} else if(strncmp(p, "180", size) == 0) {
						rot_type = MM_UTIL_ROTATE_180;
					} else if(strncmp(p, "270", size) == 0) {
						rot_type = MM_UTIL_ROTATE_270;
					} else {
						rot_type = MM_UTIL_ROTATE_0;
					}
				}
			} else {
				rot_type = MM_UTIL_ROTATE_0;
				SAFE_FREE(err_msg);
			}
		} else {
			rot_type = MM_UTIL_ROTATE_0;
		}

		err = mm_file_destroy_tag_attrs(tag);
		if (err != MM_ERROR_NONE) {
			thumb_err("fail to free tag attr - err(%x)", err);
		}

		if (rot_type == MM_UTIL_ROTATE_90 || rot_type == MM_UTIL_ROTATE_180 || rot_type == MM_UTIL_ROTATE_270) {
			/* Start to decode to rotate */
			unsigned char *rotated = NULL;
			unsigned int r_w = thumb_info->height;
			unsigned int r_h = thumb_info->width;
			unsigned int r_size = 0;

			if (rot_type == MM_UTIL_ROTATE_180) {
				r_w = thumb_info->width;
				r_h = thumb_info->height;
			}

			err = mm_util_get_image_size(MM_UTIL_IMG_FMT_RGB888, r_w, r_h, &r_size);
			if (err != MM_ERROR_NONE) {
				thumb_err("mm_util_get_image_size failed : %d", err);
				SAFE_FREE(thumb_info->data);
				return err;
			}

			//thumb_dbg("Size of Rotated : %d", r_size);
			rotated = (unsigned char *)malloc(r_size);
			err = mm_util_rotate_image(thumb_info->data, thumb_info->width, thumb_info->height,
										MM_UTIL_IMG_FMT_RGB888,
										rotated, &r_w, &r_h,
										rot_type);

			if (err != MM_ERROR_NONE) {
				thumb_err("mm_util_rotate_image failed : %d", err);
				SAFE_FREE(thumb_info->data);
				SAFE_FREE(rotated);
				return err;
			} else {
				thumb_dbg("mm_util_rotate_image succeed");
			}

			SAFE_FREE(thumb_info->data);
			thumb_info->data = rotated;
			thumb_info->size = r_size;
			thumb_info->width = r_w;
			thumb_info->height = r_h;
		}

		err = _media_thumb_convert_format(thumb_info, MEDIA_THUMB_RGB888, format);
		if (err != MS_MEDIA_ERR_NONE) {
			thumb_err("_media_thumb_convert_format falied: %d", err);
			SAFE_FREE(thumb_info->data);
			return err;
		}
		pixbuf = gdk_pixbuf_new_from_data (thumb_info->data, GDK_COLORSPACE_RGB, FALSE, 8, thumb_info->width, thumb_info->height, 3*thumb_info->width, NULL, NULL);
		thumb_info->gdkdata = gdk_pixbuf_copy(pixbuf);
		g_object_unref(pixbuf);
		
	} else {
		thumb_dbg("no contents information");
		frame = NULL;
		mm_file_destroy_content_attrs(content);

		return MS_MEDIA_ERR_INTERNAL;
	}

	return err;
}


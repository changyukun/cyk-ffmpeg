/*
 * unbuffered I/O
 * Copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/avstring.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "os_support.h"
#include "avformat.h"
#if CONFIG_NETWORK
#include "network.h"
#endif
#include "url.h"


/* 
	见函数ffurl_register_protocol  分析，函数ffurl_register_protocol  是在av_register_all  中被调用的，其中各个协议(http/rtmp/rtp....) 的数据
	结构的定义都是以ff_##xxx##_protocol  形式定义的，如rtmp  协议的数据结构就是在文件rtmpproto.c  中定义的，rtp 协议的
	ff_rtp_protocol  就在rtpproto.c  中定义的
*/
static URLProtocol *first_protocol = NULL; 

URLProtocol *ffurl_protocol_next(URLProtocol *prev)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
    	return prev ? prev->next : first_protocol;
}

/** @name Logging context. */
/*@{*/
static const char *urlcontext_to_name(void *ptr)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLContext *h = (URLContext *)ptr;
	if(h->prot) 
		return h->prot->name;
	else        
		return "NULL";
}

static void *urlcontext_child_next(void *obj, void *prev)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLContext *h = obj;
	if (!prev && h->priv_data && h->prot->priv_data_class)
		return h->priv_data;
	return NULL;
}

static const AVClass *urlcontext_child_class_next(const AVClass *prev)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLProtocol *p = NULL;

	/* find the protocol that corresponds to prev */
	while (prev && (p = ffurl_protocol_next(p)))
		if (p->priv_data_class == prev)
			break;

	/* find next protocol with priv options */
	while (p = ffurl_protocol_next(p))
		if (p->priv_data_class)
			return p->priv_data_class;
		
	return NULL;
}

static const AVOption options[] = {{NULL}};
const AVClass ffurl_context_class = {
    .class_name     = "URLContext",
    .item_name      = urlcontext_to_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
    .child_next     = urlcontext_child_next,
    .child_class_next = urlcontext_child_class_next,
};
/*@}*/


const char *avio_enum_protocols(void **opaque, int output)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLProtocol *p;
	*opaque = ffurl_protocol_next(*opaque);
	if (!(p = *opaque)) 
		return NULL;
	if ((output && p->url_write) || (!output && p->url_read))
		return p->name;
	return avio_enum_protocols(opaque, output);
}

int ffurl_register_protocol(URLProtocol *protocol, int size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数的实质就是根据传入的参数protocol  ，分配一个相应的内存将其保存起来，然后
			讲个这个内存插入到全局变量first_protocol  所代表的数组中
*/
	URLProtocol **p;
	if (size < sizeof(URLProtocol)) 
	{
		URLProtocol* temp = av_mallocz(sizeof(URLProtocol));
		memcpy(temp, protocol, size);
		protocol = temp;
	}
	
	p = &first_protocol;
	
	while (*p != NULL) 
		p = &(*p)->next;
	
	*p = protocol;
	protocol->next = NULL;
	return 0;
}

static int url_alloc_for_protocol (URLContext **puc, struct URLProtocol *up, const char *filename, int flags, const AVIOInterruptCB *int_cb)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLContext *uc;
	int err;

#if CONFIG_NETWORK
	if (up->flags & URL_PROTOCOL_FLAG_NETWORK && !ff_network_init())
		return AVERROR(EIO);
#endif
	uc = av_mallocz(sizeof(URLContext) + strlen(filename) + 1);
	if (!uc) 
	{
		err = AVERROR(ENOMEM);
		goto fail;
	}
	uc->av_class = &ffurl_context_class;
	uc->filename = (char *) &uc[1];
	strcpy(uc->filename, filename);
	uc->prot = up;
	uc->flags = flags;
	uc->is_streamed = 0; /* default = not streamed */
	uc->max_packet_size = 0; /* default: stream file */
	if (up->priv_data_size) 
	{
		uc->priv_data = av_mallocz(up->priv_data_size);
		if (up->priv_data_class) 
		{
			int proto_len= strlen(up->name);
			char *start = strchr(uc->filename, ',');
			*(const AVClass**)uc->priv_data = up->priv_data_class;
			av_opt_set_defaults(uc->priv_data);
			
			if(!strncmp(up->name, uc->filename, proto_len) && uc->filename + proto_len == start)
			{
				int ret= 0;
				char *p= start;
				char sep= *++p;
				char *key, *val;
				p++;
				while(ret >= 0 && (key= strchr(p, sep)) && p<key && (val = strchr(key+1, sep)))
				{
					*val= *key= 0;
					ret= av_opt_set(uc->priv_data, p, key+1, 0);
					if (ret == AVERROR_OPTION_NOT_FOUND)
						av_log(uc, AV_LOG_ERROR, "Key '%s' not found.\n", p);
					
					*val= *key= sep;
					p= val+1;
				}
				
				if(ret<0 || p!=key)
				{
					av_log(uc, AV_LOG_ERROR, "Error parsing options string %s\n", start);
					av_freep(&uc->priv_data);
					av_freep(&uc);
					err = AVERROR(EINVAL);
					goto fail;
				}
				memmove(start, key+1, strlen(key));
			}
		}
	}
	
	if (int_cb)
		uc->interrupt_callback = *int_cb;

	*puc = uc;
	return 0;

fail:
	*puc = NULL;
#if CONFIG_NETWORK
	if (up->flags & URL_PROTOCOL_FLAG_NETWORK)
		ff_network_close();
#endif
	return err;
}

int ffurl_connect(URLContext* uc, AVDictionary **options)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、调用具体的文件打开函数
*/
	int err = uc->prot->url_open2 ? uc->prot->url_open2(uc, uc->filename, uc->flags, options) : uc->prot->url_open(uc, uc->filename, uc->flags);
	if (err)
		return err;
	
	uc->is_connected = 1;
	//We must be careful here as ffurl_seek() could be slow, for example for http
	if(   (uc->flags & AVIO_FLAG_WRITE)|| !strcmp(uc->prot->name, "file"))
		if(!uc->is_streamed && ffurl_seek(uc, 0, SEEK_SET) < 0)
			uc->is_streamed= 1;
	return 0;
}

#define URL_SCHEME_CHARS                        \
    "abcdefghijklmnopqrstuvwxyz"                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                \
    "0123456789+-."

int ffurl_alloc(URLContext **puc, const char *filename, int flags, const AVIOInterruptCB *int_cb)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是在全局变量first_protocol  所保存的队列中查找与传入参数对应的协议，如http、rtsp  等相匹配
			的单元，然后分配URLContext  和文件名字保存地址的内存空间，然后对数据结构URLContext  中的各个
			成员进行填充，通过puc  返回
*/
	URLProtocol *up = NULL;
	char proto_str[128], proto_nested[128], *ptr;
	size_t proto_len = strspn(filename, URL_SCHEME_CHARS);

	if (!first_protocol)
	{
		av_log(NULL, AV_LOG_WARNING, "No URL Protocols are registered. " "Missing call to av_register_all()?\n");
	}

	if (filename[proto_len] != ':' &&  filename[proto_len] != ',' || is_dos_path(filename))
		strcpy(proto_str, "file");
	else
		av_strlcpy(proto_str, filename, FFMIN(proto_len+1, sizeof(proto_str)));

	if ((ptr = strchr(proto_str, ',')))
		*ptr = '\0';
	
	av_strlcpy(proto_nested, proto_str, sizeof(proto_nested));
	
	if ((ptr = strchr(proto_nested, '+')))
		*ptr = '\0';

	while (up = ffurl_protocol_next(up)) /* 遍历全局变量first_protocol  所保存的队列*/
	{
		if (!strcmp(proto_str, up->name))
			return url_alloc_for_protocol (puc, up, filename, flags, int_cb);
		
		if (up->flags & URL_PROTOCOL_FLAG_NESTED_SCHEME && !strcmp(proto_nested, up->name))
			return url_alloc_for_protocol (puc, up, filename, flags, int_cb);
	}
	
	*puc = NULL;
	return AVERROR_PROTOCOL_NOT_FOUND;
}

int ffurl_open(URLContext **puc, const char *filename, int flags, const AVIOInterruptCB *int_cb, AVDictionary **options)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数根据传入的参数(  主要是第二个以后的几个参数) ，分配一个URLContext  类型
			的数据结构，然后对其进行相应的填充，然后通过第一个参数返回这个内存

			URLContext  结构中会含有文件的打开、读、写等具体的函数

			见函数ffurl_alloc  中的分析
*/
	int ret = ffurl_alloc(puc, filename, flags, int_cb); /* 见函数中的分析*/
	if (ret)
		return ret;
	
	if (options && (*puc)->prot->priv_data_class &&(ret = av_opt_set_dict((*puc)->priv_data, options)) < 0)
		goto fail;
	
	ret = ffurl_connect(*puc, options); /* 相当于打开文件*/
	if (!ret)
		return 0;
	
fail:
	ffurl_close(*puc);
	*puc = NULL;
	return ret;
}

static inline int retry_transfer_wrapper(URLContext *h, unsigned char *buf, int size, int size_min, int (*transfer_func)(URLContext *h, unsigned char *buf, int size))
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int ret, len;
	int fast_retries = 5;
	int64_t wait_since = 0;

	len = 0;
	while (len < size_min) 
	{
		ret = transfer_func(h, buf+len, size-len);
		if (ret == AVERROR(EINTR))
			continue;
		
		if (h->flags & AVIO_FLAG_NONBLOCK)
			return ret;
		
		if (ret == AVERROR(EAGAIN)) 
		{
			ret = 0;
			if (fast_retries) 
			{
				fast_retries--;
			} 
			else 
			{
				if (h->rw_timeout) 
				{
					if (!wait_since)
						wait_since = av_gettime();
					else if (av_gettime() > wait_since + h->rw_timeout)
						return AVERROR(EIO);
				}
				av_usleep(1000);
			}
		} 
		else if (ret < 1)
			return ret < 0 ? ret : len;
		
		if (ret)
			fast_retries = FFMAX(fast_retries, 2);
		
		len += ret;
		
		if (len < size && ff_check_interrupt(&h->interrupt_callback))
			return AVERROR_EXIT;
	}
	return len;
}

int ffurl_read(URLContext *h, unsigned char *buf, int size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!(h->flags & AVIO_FLAG_READ))
		return AVERROR(EIO);
	
	return retry_transfer_wrapper(h, buf, size, 1, h->prot->url_read);
}

int ffurl_read_complete(URLContext *h, unsigned char *buf, int size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!(h->flags & AVIO_FLAG_READ))
		return AVERROR(EIO);
	return retry_transfer_wrapper(h, buf, size, size, h->prot->url_read);
}

int ffurl_write(URLContext *h, const unsigned char *buf, int size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!(h->flags & AVIO_FLAG_WRITE))
		return AVERROR(EIO);
	/* avoid sending too big packets */
	if (h->max_packet_size && size > h->max_packet_size)
		return AVERROR(EIO);

	return retry_transfer_wrapper(h, (unsigned char *)buf, size, size, (void*)h->prot->url_write);
}

int64_t ffurl_seek(URLContext *h, int64_t pos, int whence)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int64_t ret;

	if (!h->prot->url_seek)
		return AVERROR(ENOSYS);
	
	ret = h->prot->url_seek(h, pos, whence & ~AVSEEK_FORCE);
	return ret;
}

int ffurl_closep(URLContext **hh)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLContext *h= *hh;
	int ret = 0;
	
	if (!h) 
		return 0; /* can happen when ffurl_open fails */

	if (h->is_connected && h->prot->url_close)
		ret = h->prot->url_close(h);
#if CONFIG_NETWORK
	if (h->prot->flags & URL_PROTOCOL_FLAG_NETWORK)
		ff_network_close();
#endif
	if (h->prot->priv_data_size) 
	{
		if (h->prot->priv_data_class)
			av_opt_free(h->priv_data);
		
		av_freep(&h->priv_data);
	}
	
	av_freep(hh);
	return ret;
}

int ffurl_close(URLContext *h)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
    	return ffurl_closep(&h);
}


int avio_check(const char *url, int flags)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	URLContext *h;
	int ret = ffurl_alloc(&h, url, flags, NULL);
	if (ret)
		return ret;

	if (h->prot->url_check) 
	{
		ret = h->prot->url_check(h, flags);
	} 
	else 
	{
		ret = ffurl_connect(h, NULL);
		if (ret >= 0)
			ret = flags;
	}

	ffurl_close(h);
	return ret;
}

int64_t ffurl_size(URLContext *h)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int64_t pos, size;

	size= ffurl_seek(h, 0, AVSEEK_SIZE);
	if(size<0)
	{
		pos = ffurl_seek(h, 0, SEEK_CUR);
		if ((size = ffurl_seek(h, -1, SEEK_END)) < 0)
			return size;
		size++;
		ffurl_seek(h, pos, SEEK_SET);
	}
	return size;
}

int ffurl_get_file_handle(URLContext *h)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!h->prot->url_get_file_handle)
		return -1;
	
	return h->prot->url_get_file_handle(h);
}

int ffurl_get_multi_file_handle(URLContext *h, int **handles, int *numhandles)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!h->prot->url_get_multi_file_handle)
	{
		if (!h->prot->url_get_file_handle)
			return AVERROR(ENOSYS);
		
		*handles = av_malloc(sizeof(**handles));
		
		if (!*handles)
			return AVERROR(ENOMEM);
		
		*numhandles = 1;
		*handles[0] = h->prot->url_get_file_handle(h);
		return 0;
	}
	return h->prot->url_get_multi_file_handle(h, handles, numhandles);
}

int ffurl_shutdown(URLContext *h, int flags)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!h->prot->url_shutdown)
		return AVERROR(EINVAL);
	
	return h->prot->url_shutdown(h, flags);
}

int ff_check_interrupt(AVIOInterruptCB *cb)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int ret;
	if (cb && cb->callback && (ret = cb->callback(cb->opaque)))
		return ret;
	
	return 0;
}

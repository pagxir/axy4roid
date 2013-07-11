#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <wait/module.h>
#include <wait/platform.h>
#include <wait/callout.h>
#include <wait/slotwait.h>
#include <wait/slotsock.h>

#include "channel.h"

channel::~channel()
{
	return;
}

tcp_channel::tcp_channel(int fd)
{
	mfd = fd;
	moff = 0;
	mlen = 0;
	mclosed = 0;
	msockcb = sock_attach(fd);
	return;
}

tcp_channel::~tcp_channel()
{
	mclosed || close();
	return;
}

int tcp_channel::compact(int force)
{
	if (force == 0x01) {
		memmove(mbuf, mbuf + moff, mlen - moff);
		mlen -= moff;
		return 1;
	}

	return 0;
}

int tcp_channel::blocking(int code)
{
	int ret;
	int len;
	int error = ~code;

	len = sizeof(error);
#ifndef WSAEWOULDBLOCK
	ret = getsockopt(mfd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (error != code)
		fprintf(stderr, "error = %d %d\n", error, ret);
#else
	error = WSAGetLastError();
#endif

	return (error == code);
}

int tcp_channel::get(char **ptr)
{
	int c;

	assert(mlen < MAX_HTTP_HEAD_LEN);
	c = recv(mfd, mbuf, MAX_HTTP_HEAD_LEN - mlen, 0);
	*ptr = mbuf + moff;
	mlen += c;

	return mlen - moff;
}

int tcp_channel::ack(size_t len)
{
	moff += len;
	if (moff == mlen)
		moff = mlen = 0;
	return 0;
}

int tcp_channel::put(const char *buf, size_t len)
{
	return ::send(mfd, buf, len, 0);
}

int tcp_channel::close(void)
{
	assert(!mclosed);
	mclosed = 1;
	sock_detach(msockcb);
	return ::close(mfd);
}

int tcp_channel::waiti(struct waitcb *wait)
{
	return sock_read_wait(msockcb, wait);
}

int tcp_channel::waito(struct waitcb *wait)
{
	return sock_write_wait(msockcb, wait);
}

http_raw_channel::http_raw_channel(int fd)
	:tcp_channel(fd)
{
	t_len = 0;
	t_off = 0;
	mflags = HTTP_DISABLED;
	mdata_length = 0;
	mchunked_length = 0;
	mcontent_length = 0;
	return;
}

http_raw_channel::~http_raw_channel()
{
	return;
}

int http_raw_channel::compact(int force)
{
	return 0;
}

int http_raw_channel::handle_http_header_item(const char *buf, size_t len)
{
#define CL sizeof(content_length) - 1
#define TL sizeof(transfer_encoding) - 1

	int length = -1;
	const char content_length[] = "Content-Length:";
	const char transfer_encoding[] = "Transger-Encoding:";

	if (memcmp(buf, "\r\n", 2) == 0 && len == 2) {
		/* end of http header */
		mflags = HTTP_HEADER;
	} else if (strncasecmp(buf, content_length, CL) == 0) {
		if (sscanf(buf + CL, "%d", &length) == 0) {
			mcontent_length = length;
		}
	} else if (strncasecmp(buf, transfer_encoding, TL) == 0) {
		if (memmem(buf, len, "chunked", 7) != NULL) {
			mflags |= HTTP_CHUNKED_TRANSFER;
		}
	}

	return 0;
}

int http_raw_channel::handle_http_chunked_header(const char *buf, size_t len)
{
	int chunked_length;

	if (sscanf(buf, "%x", &chunked_length) == 1) {
		mchunked_length = chunked_length;
		return 0;
	}

	assert(0);
	return -1;
}

int http_raw_channel::ack(size_t len)
{
	t_off += len;
	if (t_len = t_off) {
		tcp_channel::ack(t_len);
		tcp_channel::compact(1);
		t_off = t_len = 0;
	}

	return 0;
}

int http_raw_channel::get(char **ptr)
{
	int count;
	char *data = 0;

	if (t_off < t_len) {
		*ptr = mbuf + t_off;
		return t_len - t_off;
	}

	if (mflags & HTTP_CONTENT_COMPLETE) {
		/* http stream is end. */
		return 0;
	}

	count = tcp_channel::get(&data);
	if (count > 0 && !(mflags & HTTP_DISABLED)) {
		return update_get(data, count);
	}

	return count;
}

int http_raw_channel::update_get(const char *buf, size_t count)
{
	int copy_len;
	const char *p, *base, *limit;

	base = (char *)buf;
	limit = (base + count);

	while (base < limit && !(mflags & HTTP_HEADER)) {
		p = (char *)memchr(base, limit - base, '\n');
		if (p == NULL) {
			t_len = (base - buf);
			return t_len;
		}

		handle_http_header_item(base, p - base);
		base = (p + 1);
	}

	int check_flags = HTTP_HEADER | HTTP_CHUNKED_TRANSFER;

	if ((mflags & check_flags) == check_flags) {
		int test_flags = CHUNKED_HEADER| HTTP_CONTENT_COMPLETE;
		do {
			while (base < limit && !(mflags & CHUNKED_HEADER)) {
				p = (char *)memchr(base, limit - base, '\n');
				if (p == NULL) {
					t_len = (base - buf);
					return t_len;
				}

				handle_http_chunked_header(base, p - base);
				base = (p + 1);
			}

			if (mflags & CHUNKED_HEADER) {
				assert(mdata_length < mchunked_length + 2);
				if (mdata_length + (limit - base) < mchunked_length + 2) {
					mdata_length += (limit - base);
					base = limit;
				} else {
					base = buf + (mchunked_length + 2 - mdata_length);
					mdata_length = mchunked_length + 2;
					if (mchunked_length == 0)
						mflags |= HTTP_CONTENT_COMPLETE;
					mflags &= ~CHUNKED_HEADER;
				}
			}
		} while ((mflags & test_flags) == 0 && (base < limit));
	} else if ((mflags & check_flags) == HTTP_HEADER) {
		assert(mdata_length < mcontent_length);
		if (mdata_length + (limit - base) >= mcontent_length) {
			base = buf + (mcontent_length - mdata_length);
			mdata_length = mcontent_length;
			mflags |= HTTP_CONTENT_COMPLETE;
		} else {
			mdata_length += (limit - base);
			base = limit;
		}
	}

	t_len = (base - buf);
	return t_len;
}

int http_raw_channel::open_next(void)
{
	mflags &= ~HTTP_CONTENT_COMPLETE;
	mflags &= ~HTTP_CHUNKED_TRANSFER;
	mflags &= ~CHUNKED_HEADER;
	mflags &= ~HTTP_HEADER;
	return -1;
}

int http_raw_channel::enable_http(void)
{
	mflags &= ~HTTP_DISABLED;
	return 0;
}

channel_forward::channel_forward(wait_call *call, void *up)
{
	waitcb_init(this, call, up);
	waitcb_init(&r_wait, cf_callback, this);
	waitcb_init(&w_wait, cf_callback, this);
}

channel_forward::~channel_forward()
{
	waitcb_clean(this);
	waitcb_clean(&r_wait);
	waitcb_clean(&w_wait);
}

void channel_forward::cf_callback(void *up)
{
	channel_forward *cf = (channel_forward *)up;
	cf->callback();
	return;
}

void channel_forward::callback(void)
{
	int len;
	int ack_;
	char *p;

	waitcb_clear(&r_wait);
	waitcb_clear(&w_wait);
	for ( ; ; ) {
		len = _src->get(&p);
		if (len <= 0 &&
				_src->blocking(EAGAIN)) {
			_src->waiti(&r_wait);
			return;
		}

		if (len <= 0) {
			waitcb_switch(this);
			break;
		}

		ack_ = _dst->put(p, len);
		if (ack_ <= 0 &&
				_dst->blocking(EAGAIN)) {
			_src->waiti(&w_wait);
			return;
		}

		if (len <= 0) {
			waitcb_switch(this);
			break;
		}

		_src->ack(ack_);
	}

	return;
}

void channel_forward::start(channel *src, channel *dst)
{
	_src = src;
	_dst = dst;
	_src->waiti(&r_wait);
	_dst->waito(&w_wait);
}

void channel_forward::cancel(void)
{
	waitcb_cancel(&r_wait);
	waitcb_cancel(&w_wait);
	return;
}


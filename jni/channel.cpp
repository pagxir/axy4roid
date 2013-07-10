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
	mclosed = 0;
	msockcb = sock_attach(fd);
	return;
}

tcp_channel::~tcp_channel()
{
	mclosed || close();
	return;
}

int tcp_channel::read(void *buf, size_t len)
{
	return recv(mfd, buf, len, 0);
}

int tcp_channel::write(void *buf, size_t len)
{
	return send(mfd, buf, len, 0);
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
	return;
}

http_raw_channel::~http_raw_channel()
{
	return;
}

int http_raw_channel::read(void *buf, size_t len)
{
	return -1;
}

int http_raw_channel::write(void *buf, size_t len)
{
	return -1;
}

int http_raw_channel::open_next(void)
{
	return -1;
}

int http_raw_channel::close(void)
{
	return -1;
}

int http_raw_channel::waiti(struct waitcb *wait)
{
	return -1;
}

int http_raw_channel::waito(struct waitcb *wait)
{
	return -1;
}

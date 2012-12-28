#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <wait/module.h>
#include <wait/platform.h>
#include <wait/callout.h>
#include <wait/slotwait.h>
#include <wait/slotsock.h>

static void tc_cleanup(void *context);
static void tc_callback(void *context);
static int  socksproto_run(struct socksproto *up);

#define NO_MORE_DATA 1
#define WRITE_BROKEN 2

static int link_count = 0;

struct sockspeer {
	int fd;
	int off;
	int len;
	int flags;
	char buf[8192];
	struct sockop *ops;
	struct sockcb *lwipcbp;
	struct waitcb rwait;
	struct waitcb wwait;

	int debug_read;
	int debug_write;
};

struct socksproto {
	int m_flags;
	int respo_len;
	int proto_flags;
	struct waitcb timer;
	struct waitcb stopper;
	struct sockspeer c, s;
	struct sockaddr_in addr_in1;
};

int error_equal(int fd, int code)
{
	int ret;
	int len;
	int error = ~code;

	len = sizeof(error);
#ifndef WSAEWOULDBLOCK
	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (error != code)
		fprintf(stderr, "error = %d %d\n", error, ret);
#else
	error = WSAGetLastError();
#endif

	return (error == code);
}

static void socksproto_init(struct socksproto *up, int sockfd, int lwipfd)
{
	up->m_flags = 0;
	up->respo_len = 0;
	up->proto_flags = 0;
	waitcb_init(&up->timer, tc_callback, up);
	waitcb_init(&up->stopper, tc_cleanup, up);

	up->c.fd = sockfd;
	up->c.flags = 0;
	up->c.off = up->c.len = 0;
	up->c.ops = &winsock_ops;
	up->c.debug_read = 0;
	up->c.debug_write = 0;
	up->c.lwipcbp = sock_attach(up->c.fd);
	waitcb_init(&up->c.rwait, tc_callback, up);
	waitcb_init(&up->c.wwait, tc_callback, up);

	up->s.fd = lwipfd;
	assert(up->s.fd != -1);
	slotwait_atstop(&up->stopper);

	setnonblock(up->s.fd);
	up->s.flags = 0;
	up->s.off = up->s.len = 0;
	up->s.ops = &winsock_ops;
	up->s.debug_read = 0;
	up->s.debug_write = 0;
	up->s.lwipcbp = sock_attach(up->s.fd);
	waitcb_init(&up->s.rwait, tc_callback, up);
	waitcb_init(&up->s.wwait, tc_callback, up);
}

static void socksproto_fini(struct socksproto *up)
{
	fprintf(stderr, "pstcp_socks::~pstcp_socks, %d\n", link_count);
	waitcb_clean(&up->timer);
	waitcb_clean(&up->stopper);

	waitcb_clean(&up->s.rwait);
	waitcb_clean(&up->s.wwait);
	sock_detach(up->s.lwipcbp);
	closesocket(up->s.fd);

	waitcb_clean(&up->c.rwait);
	waitcb_clean(&up->c.wwait);
	sock_detach(up->c.lwipcbp);
	closesocket(up->c.fd);
}

static void tc_cleanup(void *context)
{
	struct socksproto *ctxp;

	ctxp = (struct socksproto *)context;

	link_count--;
	socksproto_fini(ctxp);
	free(context);

	return;
}

struct socksproto *_header = NULL;
static void tc_callback(void *context)
{
	struct socksproto *ctxp;

	ctxp = (struct socksproto *)context;
	if (waitcb_completed(&ctxp->timer) ||
		socksproto_run(ctxp) == 0) {
		link_count--;
		socksproto_fini(ctxp);
		free(context);
		return;
	}

	if (waitcb_active(&ctxp->c.rwait) ||
		waitcb_active(&ctxp->c.wwait) ||
		waitcb_active(&ctxp->s.rwait) ||
		waitcb_active(&ctxp->s.wwait))
		return;

	assert(0);
}

enum {
	NONE_PROTO = 0,
	UNKOWN_PROTO = (1 << 0),
	SOCKV4_PROTO = (1 << 1),
	SOCKV5_PROTO = (1 << 2),
	HTTPS_PROTO  = (1 << 3),
	FORWARD_PROTO= (1 << 4),
	DIRECT_PROTO = (1 << 5),
	DOCONNECTING = (1 << 6)
};

static const int SUPPORTED_PROTO = UNKOWN_PROTO| SOCKV4_PROTO| SOCKV5_PROTO| DIRECT_PROTO | FORWARD_PROTO;

static void fill_connect_buffer(struct socksproto *up)
{
	int len;
	int count;
	char *buf;

	if (waitcb_completed(&up->c.rwait) && up->c.len < sizeof(up->c.buf)) {
		buf = up->c.buf + up->c.off;
		len = sizeof(up->c.buf) - up->c.len;
		count = up->c.ops->op_read(up->c.fd, buf, len);
		switch (count) {
			case -1:
				up->c.flags |= NO_MORE_DATA;
				break;

			case 0:
				up->c.flags |= NO_MORE_DATA;
				break;

			default:
				waitcb_clear(&up->c.rwait);
				up->c.len += count;
				break;
		}
	}
}

struct buf_match {
	int flags;
	int limit;
	char *base;
};

const int BUF_OVERFLOW = 1;
static int buf_init(struct buf_match *m, void *buf, int limit)
{
	m->flags = 0;
	m->base  = (char *)buf;
	m->limit = limit;
	return 0;
}

static int buf_equal(struct buf_match *m, int off, int val)
{
	if (off < m->limit)
		return (val == m->base[off]);
	m->flags |= BUF_OVERFLOW;
	return 0;
}

static int buf_valid(struct buf_match *m, int off)
{
	if (off < m->limit)
		return (1);
	m->flags |= BUF_OVERFLOW;
	return 0;
}

static int buf_find(struct buf_match *m, int off, int val)
{
	const void *p = 0;
	m->flags |= BUF_OVERFLOW;
	if (off < m->limit)
		p = memchr(m->base + off, val, m->limit - off);
	return !(p == NULL);
}

static int buf_overflow(struct buf_match *m)
{
	/* XXXX */
	return (m->flags & BUF_OVERFLOW);
}

static void check_proxy_proto(struct socksproto *up)
{
	struct buf_match m;

	buf_init(&m, up->c.buf, up->c.len);
	if (buf_equal(&m, 0, 0x04) && buf_find(&m, 8, 0)) {
		up->m_flags |= SOCKV4_PROTO;
		return;
	}

	if (buf_equal(&m, 0, 0x05) && buf_valid(&m, 1)) {
		int len = (m.base[1] & 0xFF);
		if (memchr(&m.base[2], 0x0, len)) {
			up->m_flags |= SOCKV5_PROTO;
			return;
		}
	}

	if (buf_equal(&m, 0, 'C')) {
		int off = 0;
		const char *op = "CONNECT ";
		while (*++op != 0) {
			if (!buf_equal(&m, ++off, *op))
				break;
		}
		if (*op == 0) {
			up->m_flags |= HTTPS_PROTO;
			return;
		}
	}

	if (!buf_overflow(&m)) {
		up->m_flags |= UNKOWN_PROTO;
		return;
	}

	if (up->c.len == sizeof(up->c.buf)) {
		up->m_flags |= UNKOWN_PROTO;
		return;
	}

	if (up->c.flags & NO_MORE_DATA) {
		up->m_flags |= UNKOWN_PROTO;
		return;
	}
}

enum socksv5_proto_flags {
	AUTHED = (1 << 0)	
};

extern int get_addr_by_name(const char *name, struct in_addr *ipaddr);

static int forward_proto_input(struct socksproto *up)
{
	int error;
	struct in_addr in_addr1;

#if 0
	if (get_addr_by_name("chnpxy01.cn.ta-mp.com", &in_addr1)) {
		goto host_not_found;
	}
#endif
	if (get_addr_by_name("172.24.61.252", &in_addr1)) {
		goto host_not_found;
	}

	up->addr_in1.sin_family = AF_INET;
	up->addr_in1.sin_port   = htons(8080);
	up->addr_in1.sin_addr   = in_addr1;

	up->m_flags &= ~FORWARD_PROTO;
	fprintf(stderr, "connect to %d\n", link_count);
	error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
	if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
		up->m_flags |= DOCONNECTING;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

host_not_found:
	up->c.flags |= WRITE_BROKEN;
	up->s.flags |= WRITE_BROKEN;
	up->m_flags |= UNKOWN_PROTO;
	return 0;
}

/*
1.向服务器的1080端口建立tcp连接。   
2.向服务器发送   05   01   00   （此为16进制码，以下同）   
3.如果接到   05   00   则是可以代理   
4.发送   05   01   00   01   +   目的地址(4字节）   +   目的端口（2字节），目的地址和端口都是16进制码（不是字符串！！）。   例202.103.190.27   -   7201   则发送的信息为：05   01   00   01   CA   67   BE   1B   1C   21   (CA=202   67=103   BE=190   1B=27   1C21=7201)   
5.接受服务器返回的自身地址和端口，连接完成   
6.以后操作和直接与目的方进行TCP连接相同。   

如何用代理UDP连接   

1.向服务器的1080端口建立udp连接   
2.向服务器发送   05   01   00   
3.如果接到   05   00   则是可以代理   
4.发送   05   03   00   01   00   00   00   00   +   本地UDP端口（2字节）   
5.服务器返回   05   00   00   01   +服务器地址+端口   
6.需要申请方发送   00   00   00   01   +目的地址IP（4字节）+目的端口   +所要发送的信息   
7.当有数据报返回时   向需要代理方发出00   00   00   01   +来源地址IP（4字节）+来源端口   +接受的信息   
*/

static int sockv5_proto_input(struct socksproto *up)
{
	int error;
	int ret, pat;
	char buf[8192];
	char *p, *limit;
	struct buf_match m;
	struct sockspeer *t = &up->c;
	static u_char resp_v5[] = {
		0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	if ((up->proto_flags & AUTHED) != AUTHED) {
		buf[0] = 0x05;
		buf[1] = 0x00;
		ret = t->ops->op_write(t->fd, buf, 2);
		if (ret != 2)
			goto host_not_found;
		limit = up->c.buf + up->c.len;
		p = up->c.buf + (up->c.buf[1] & 0xFF) + 2;
		memmove(up->c.buf, p, limit - p);
		up->c.len = (limit - p);
		up->proto_flags |= AUTHED;
	}

	buf_init(&m, up->c.buf, up->c.len);
	if (up->proto_flags & AUTHED) {
		if (buf_equal(&m, 0, 0x05) && 
			buf_equal(&m, 2, 0x00) && buf_valid(&m, 9)) {
			char *end = 0;
			u_short in_port1;
			char domain[256];
			struct in_addr in_addr1;
			limit = up->c.buf + up->c.len;

			switch (up->c.buf[3]) {
				case 0x01:
					end = (up->c.buf + 4);
					memcpy(&in_addr1, end, sizeof(in_addr1));
					end += sizeof(in_addr1);
					memcpy(&in_port1, end, sizeof(in_port1));
					end += sizeof(in_port1);
					break;

				case 0x03:
					pat = (up->c.buf[4] & 0xFF);
					if (buf_valid(&m, 4 + pat + 2)) {
						memcpy(domain, up->c.buf + 5, pat);
						domain[pat] = 0;
						if (get_addr_by_name(domain, &in_addr1))
							goto host_not_found;
						end = up->c.buf + 5 + pat;
						memcpy(&in_port1, end, sizeof(in_port1));
						end += sizeof(in_port1);
						break;
					} 
					goto check_protocol;

				default:
					fprintf(stderr, "socksv5 bad host type!\n");
					goto host_not_found;
			}

			up->addr_in1.sin_family = AF_INET;
			up->addr_in1.sin_port   = in_port1;
			up->addr_in1.sin_addr   = in_addr1;

			switch (up->c.buf[1]) {
				case 0x00:
					fprintf(stderr, "socksv5 command udp ass not supported yet!\n");
					break;

				case 0x01:
					up->c.len = (limit - end);
					memmove(up->c.buf, end, up->c.len);

					up->m_flags &= ~SOCKV5_PROTO;
					fprintf(stderr, "connect to %d\n", link_count);
					error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
					if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
						memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
						up->respo_len = sizeof(resp_v5);
						up->s.len = up->s.off = 0;
						up->m_flags |= DOCONNECTING;
						up->m_flags |= DIRECT_PROTO;
						return 0;
					}

				case 0x03:
					fprintf(stderr, "socksv5 command bind not supported yet!\n");
				default:
					fprintf(stderr, "socksv5 command unkown!\n");
					goto host_not_found;
			}

			return 0;
		}
		goto check_protocol;
	}
	
	return 0;

check_protocol:
	if (!buf_overflow(&m)) {
		goto host_not_found;
	} else if (up->c.len == sizeof(up->c.buf)) {
		goto host_not_found;
	} else if (up->c.flags & NO_MORE_DATA) {
		goto host_not_found;
	}
	return 0;

host_not_found:
	up->c.flags |= WRITE_BROKEN;
	up->s.flags |= WRITE_BROKEN;
	up->m_flags |= UNKOWN_PROTO;
	return 0;
}

static int https_proto_input(struct socksproto *up)
{
	int error;
	struct in_addr in_addr1;
	char buf[sizeof(up->c.buf)];
	char *bound, *limit, *port, *end, *p;
	const char resp_https[] = "HTTP/1.1 200 Connection established\r\n\r\n";

	limit = up->c.buf + up->c.len;
	for (p = up->c.buf; p < limit; p++) {
		p = (char *)memchr(p, '\r', limit - p);
		if (p == NULL)
			return 0;
		end = p + 4;
		if (strncmp(p, "\r\n\r\n", 4) == 0)
			break;
	}

	if (end > limit) {
		return 0;
	}

	if (sscanf(up->c.buf, "CONNECT %s HTTP/1.%*d\r\n", buf) != 1) {
		goto host_not_found;
	}

	port = (char *)"80";
	bound = strchr(buf, ':');
	if (bound != NULL) {
		*bound++ = 0;
		port = bound;
	}

	if (get_addr_by_name(buf, &in_addr1)) {
		goto host_not_found;
	}

	up->c.len = (limit - end);
	memmove(up->c.buf, end, up->c.len);

	up->addr_in1.sin_family = AF_INET;
	up->addr_in1.sin_port   = htons(atoi(port));
	up->addr_in1.sin_addr   = in_addr1;
	
	up->m_flags &= ~HTTPS_PROTO;
	fprintf(stderr, "connect to %d\n", link_count);
	error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
	if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
		memcpy(up->s.buf, resp_https, sizeof(resp_https));
		up->respo_len = sizeof(resp_https) - 1;
		up->s.len = up->s.off = 0;
		up->m_flags |= DOCONNECTING;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

host_not_found:
	up->c.flags |= WRITE_BROKEN;
	up->s.flags |= WRITE_BROKEN;
	up->m_flags |= UNKOWN_PROTO;
	return 0;
}

static int sockv4_proto_input(struct socksproto *up)
{
	int error;
	u_short in_port1;	
	struct in_addr in_addr1;
	
	const char *buf = up->c.buf;
	const char *limit = up->c.buf + up->c.len;
	u_char resp_v4[] = {0x00, 0x5A, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};

	assert(*buf == 0x4);
	buf++;
	assert(*buf == 0x01);
	buf++;
	memcpy(&in_port1, buf, sizeof(in_port1));
	buf += 2;
	memcpy(&in_addr1, buf, sizeof(in_addr1));
	buf += 4;

	buf = (char *)memchr(buf, 0, limit - buf);
	/* use for sockv4 support */
	if (ntohl(in_addr1.s_addr) < 256) {
		if (get_addr_by_name(buf + 1, &in_addr1))
			goto host_not_found;
		buf = (char *)memchr(buf + 1, 0, limit - buf - 1);
	}

	up->c.len = (limit - buf - 1);
	memcpy(up->c.buf, buf + 1, up->c.len);

	up->addr_in1.sin_family = AF_INET;
	up->addr_in1.sin_port   = in_port1;
	up->addr_in1.sin_addr   = in_addr1;
	
	up->m_flags &= ~SOCKV4_PROTO;
	fprintf(stderr, "connect to %d\n", link_count);
	error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
	if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
		memmove(up->s.buf, resp_v4, sizeof(resp_v4));
		up->respo_len = sizeof(resp_v4);
		up->s.len = up->s.off = 0;
		up->m_flags |= DOCONNECTING;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

host_not_found:
	up->c.flags |= WRITE_BROKEN;
	up->s.flags |= WRITE_BROKEN;
	up->m_flags |= UNKOWN_PROTO;
	return 0;
}

static void DO_SHUTDOWN(struct sockspeer *p, int cond)
{
	if (cond != 0) {
		p->ops->do_shutdown(p->fd, 1);
		p->flags |= WRITE_BROKEN;
		return;
	}

	return;
}

static int do_data_forward(struct socksproto *up, 
		struct sockspeer *f, struct sockspeer *t)
{
	int ret;
	int mask;
	int changed;

	do {
		changed = 0;
		mask = WRITE_BROKEN;
		if (!(t->flags & mask) && f->off < f->len) {
			if (waitcb_completed(&t->wwait)) {
				ret = t->ops->op_write(t->fd, f->buf + f->off, (f->len - f->off));
				if (ret == -1 && !t->ops->blocking(t->fd)) {
					t->flags |= NO_MORE_DATA;
					t->flags |= WRITE_BROKEN;
					f->flags |= NO_MORE_DATA;
					DO_SHUTDOWN(f, t->off == t->len);
				} else if (ret < f->len - f->off) {
					t->debug_write += (ret > 0? ret: 0);
					f->off += (ret > 0? ret: 0);
					waitcb_clear(&t->wwait);
					changed = (ret > 0);
				} else {
					DO_SHUTDOWN(t, f->flags & NO_MORE_DATA);
					t->debug_write +=  ret;
					f->off = f->len = 0;
					changed = 1;
				}
			}
		}

		mask = NO_MORE_DATA;
		if (!(f->flags & mask) && f->len < sizeof(f->buf)) {
			if (waitcb_completed(&f->rwait)) {
				ret = f->ops->op_read(f->fd, f->buf + f->len, sizeof(f->buf) - f->len);
				if (ret == -1 && !f->ops->blocking(f->fd)) {
					f->flags |= NO_MORE_DATA;
					f->flags |= WRITE_BROKEN;
					t->flags |= NO_MORE_DATA;
					DO_SHUTDOWN(t, f->off == f->len);
				} else if (ret > 0) {
					f->debug_read += ret;
					waitcb_clear(&f->rwait);
					f->len += ret;
					changed = 1;
				} else if (ret == 0) {
					DO_SHUTDOWN(t, f->off == f->len);
					f->flags |= NO_MORE_DATA;
				} else {
					waitcb_clear(&f->rwait);
					/* EWOULDBLOCK */
				}
			}
		}

	} while (changed);

	return 0;
}

static int try_reconnect(struct socksproto *up)
{
	int newfd;
	int ret, error;
	socklen_t len = 0;

	if (waitcb_completed(&up->s.wwait)) {
		len = sizeof(error);
		up->m_flags &= ~DOCONNECTING;
		ret = getsockopt(up->s.fd, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
		if (error != ECONNABORTED) {
			up->s.len = up->respo_len;
			return 0;
		}
#if 0
		up->m_flags &= ~DIRECT_PROTO;
		waitcb_clear(&up->s.wwait);
		newfd = socket(AF_INET, SOCK_STREAM, 0);
		if (newfd == -1)
			goto host_not_found;

		closesocket(up->s.fd);
		up->s.fd = newfd;
		error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
		if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
			up->m_flags |= DOCONNECTING;
			up->m_flags |= DIRECT_PROTO;
			return 0;
		}
#endif

host_not_found:
		up->c.flags |= WRITE_BROKEN;
		up->s.flags |= WRITE_BROKEN;
		up->m_flags |= UNKOWN_PROTO;
	}

	return 0;
}

static int socksproto_run(struct socksproto *up)
{
	int len = 0;
	int mask = 0;
	int error = -1;
	
	u_char pro_seq[] = {0x05, 0x01, 0x00, 0x01};
	u_char pro_seq_nam[] = {0x05, 0x01, 0x00, 0x03};
	u_char resp[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	callout_reset(&up->timer, 60000);
	if ((up->m_flags & SUPPORTED_PROTO) == NONE_PROTO) {
		fill_connect_buffer(up);
		check_proxy_proto(up);
	}

	if (up->m_flags & FORWARD_PROTO) {
		fill_connect_buffer(up);
		forward_proto_input(up);
	}

	if (up->m_flags & SOCKV4_PROTO) {
		fill_connect_buffer(up);
		sockv4_proto_input(up);
	}

	if (up->m_flags & SOCKV5_PROTO) {
		fill_connect_buffer(up);
		sockv5_proto_input(up);
	}

	if (up->m_flags & HTTPS_PROTO) {
		fill_connect_buffer(up);
		https_proto_input(up);
	}

	if (up->m_flags & UNKOWN_PROTO) {
		return 0;
	}

	if (up->m_flags & DIRECT_PROTO) {
		if (up->m_flags & DOCONNECTING)
			try_reconnect(up);
		do_data_forward(up, &up->s, &up->c);
		do_data_forward(up, &up->c, &up->s);

		mask = NO_MORE_DATA;
		if ((up->s.flags & mask) != NO_MORE_DATA) {
			if (!waitcb_active(&up->s.rwait) &&
				!waitcb_completed(&up->s.rwait))
				up->s.ops->read_wait(up->s.lwipcbp, &up->s.rwait);
		}

		mask = WRITE_BROKEN;
		if ((up->s.flags & mask) != WRITE_BROKEN) {
			if (!waitcb_active(&up->s.wwait) &&
				!waitcb_completed(&up->s.wwait))
				up->s.ops->write_wait(up->s.lwipcbp, &up->s.wwait);
		}
	}

	mask = NO_MORE_DATA;
	if ((up->c.flags & mask) != NO_MORE_DATA) {
		if (!waitcb_active(&up->c.rwait) &&
			!waitcb_completed(&up->c.rwait))
			up->c.ops->read_wait(up->c.lwipcbp, &up->c.rwait);
	}

	mask = WRITE_BROKEN;
	if ((up->c.flags & mask) != WRITE_BROKEN) {
		if (!waitcb_active(&up->c.wwait) &&
			!waitcb_completed(&up->c.wwait))
			up->c.ops->write_wait(up->c.lwipcbp, &up->c.wwait);
	}

	mask = WRITE_BROKEN;
	return !((up->c.flags & mask) && (up->s.flags & mask));
}

void new_tcp_socks(int sockfd)
{
	int lwipfd;
	struct socksproto *ctxp;

	lwipfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lwipfd == -1) {
		fprintf(stderr, "lwip create socket failure\n");
		closesocket(sockfd);
		return;
	}

	ctxp = (struct socksproto *)malloc(sizeof(*ctxp));
	if (ctxp == NULL) {
		closesocket(sockfd);
		closesocket(lwipfd);
		return;
	}

	link_count++;
	socksproto_init(ctxp, sockfd, lwipfd);
	tc_callback(ctxp);
	return;
}

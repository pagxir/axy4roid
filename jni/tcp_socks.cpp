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
static int socksproto_run(struct socksproto *up);
static int renew_http_socks(struct socksproto *up);

#define NO_MORE_DATA 1
#define WRITE_BROKEN 2
#define GET_LENGTHED 4
#define FLAG_CHUNKED 8
#define LASR_CHUNKED 0x10

static int link_count = 0;
static char http_maigc_url[512] = {
	""
};

static char http_authorization[512] = {
	"" //dXNlcjpwYXNzd29yZA=="
};

static char socks5_user_password[514] = {
	"\005proxy\014AdZnGWTM0dLT"
};

static const char www_authentication_required[] = {
	"HTTP/1.0 401 Unauthorized\r\n"
	"WWW-Authenticate: Basic realm=\"myfield.com\"\r\n\r\n"
};

static const char proxy_authentication_required[] = {
	"HTTP/1.0 407 Proxy authentication required\r\n"
	"Proxy-Authenticate: Basic realm=\"pagxir@gmail.com\"\r\n\r\n"
};

static const char proxy_failure_request[] = {
	"HTTP/1.0 407 Proxy authentication required\r\n"
	"\r\n"
};

struct sockspeer {
	int fd;
	int off;
	int len;
	int flags;
	int limit;
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
	int op_cmd;
	int proto_flags;
	int use_http;
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

extern "C" int set_http_magic_url(const char *info)
{
	size_t size;

	size = sizeof(http_maigc_url);
	if (strlen(info) < size) {
		strncpy(http_maigc_url, info, size);
		return 0;
	}

	return -1;
}

extern "C" int set_http_authentication(const char *info)
{
	size_t size;

	size = sizeof(http_authorization);
	if (strlen(info) < size) {
		strncpy(http_authorization, info, size);
		return 0;
	}

	return -1;
}

extern "C" int set_socks5_user_password(const char *info)
{
	size_t size;

	size = sizeof(socks5_user_password);
	if (strlen(info) < size) {
		strncpy(socks5_user_password, info, size);
		return 0;
	}

	return -1;
}

static const char *get_line_by_key(const char *buf, size_t len, const char *key)
{
	const char *p;
	size_t l = strlen(key);
	const char *limit = buf + len -l;

	if (l >= len) {
		return 0;
	}

	p = (char *)memchr(buf, '\n', len);
	for (p = buf; p < limit; p++) {
		if (*p == '\n' &&
			strncasecmp(p + 1, key, l) == 0) {
			return (p + 1);
		}
	}

	return 0;
}

static int check_transfer_encoding(const char *buf, size_t len, const char *encoding)
{
	const char *p;
	const char *line = get_line_by_key(buf, len, "Transfer-Encoding:");
	if (line == NULL) {
		return 0;
	}

	int le = strlen(encoding);
	const int LN = sizeof("Transfer-Encoding:") - 1;

	p = line + LN;
	while (*p == ' ')p++;
	return strncmp(encoding, p, le) == 0;
}

static int get_content_length(const char *buf, size_t len, int def)
{
	size_t content_length = 0;
	const char *line = get_line_by_key(buf, len, "Content-Length: ");

	if (line == NULL) {
		return def;
	}

	const int LN = sizeof("Content-Length:") - 1;
	if (sscanf(line + LN, "%u", &content_length) == 1) {
		fprintf(stderr, "Content-Length: %d\n", content_length);
		return content_length;
	}

	return def;
}

static void socksproto_init(struct socksproto *up, int sockfd, int lwipfd)
{
	up->m_flags = 0;
	up->use_http = 0;
	up->proto_flags = 0;
	waitcb_init(&up->timer, tc_callback, up);
	waitcb_init(&up->stopper, tc_cleanup, up);
	slotwait_atstop(&up->stopper);

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

	fprintf(stderr, "encounter proxy connect error, close this connection!\n");
	socksproto_fini(ctxp);
	free(context);
	link_count--;
	return;
}

enum {
	NONE_PROTO = 0,
	UNKOWN_PROTO = (1 << 0),
	SOCKV4_PROTO = (1 << 1),
	SOCKV5_PROTO = (1 << 2),
	HTTPS_PROTO  = (1 << 3),
	HTTP_PROTO   = (1 << 4),
	FORWARD_PROTO= (1 << 5),
	DIRECT_PROTO = (1 << 6),
	DOCONNECTING = (1 << 7)
};

static const int SUPPORTED_PROTO = UNKOWN_PROTO| SOCKV4_PROTO| SOCKV5_PROTO| DIRECT_PROTO | FORWARD_PROTO| HTTP_PROTO;

static void fill_connect_buffer(struct sockspeer *p)
{
	int len;
	int count;
	char *buf;

	if (waitcb_completed(&p->rwait) && p->len < (int)sizeof(p->buf)) {
		buf = p->buf + p->len;
		len = sizeof(p->buf) - p->len;
		count = p->ops->op_read(p->fd, buf, len);
		switch (count) {
			case -1:
				p->flags |= NO_MORE_DATA;
				break;

			case 0:
				p->flags |= NO_MORE_DATA;
				break;

			default:
				waitcb_clear(&p->rwait);
				p->len += count;
				break;
		}
	} else if (p->len == (int)sizeof(p->buf)) {
		fprintf(stderr, "buffer is full\n");
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

static int check_proxy_authentication(const char *buf)
{
	int len;
	const char *realm, *limit; 

#if 0
	"Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
#endif

	limit = strstr(buf, "\r\n\r\n");
	realm = strstr(buf, "Proxy-Authorization:");

	if (*http_authorization == 0)
		return 1;

	if (limit == NULL ||
		realm == NULL || limit < realm)
		return 0;

	realm += strlen("Proxy-Authorization:");
	while (*realm == ' ') realm++;

	if (strncmp(realm, "Basic", 5) != 0)
		return 0;

	realm += strlen("Basic");
	while (*realm == ' ') realm++;

	len = strlen(http_authorization);
	return strncmp(realm, http_authorization, len) == 0;
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

		if (memchr(&m.base[2], 0x2, len)) {
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

	if (buf_equal(&m, 0, 'G')) {
		int off = 0;
		const char *op = "GET ";
		while (*++op != 0) {
			if (!buf_equal(&m, ++off, *op))
				break;
		}
		if (*op == 0) {
			up->m_flags |= HTTP_PROTO;
			return;
		}
	}

	if (buf_equal(&m, 0, 'P')) {
		int off = 0;
		const char *op = "POST ";
		while (*++op != 0) {
			if (!buf_equal(&m, ++off, *op))
				break;
		}
		if (*op == 0) {
			up->m_flags |= HTTP_PROTO;
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
	AUTHED_0 = (1 << 0),
	AUTHED_1 = (1 << 1),
	AUTHED_Z = (1 << 2)
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
	struct buf_match m, n;
	struct sockspeer *t = &up->c;
	static u_char resp_v5[] = {
		0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	buf_init(&m, up->c.buf, up->c.len);
	if ((up->m_flags & DOCONNECTING) == 0 &&
			(up->proto_flags & AUTHED_0) != AUTHED_0) {

		if (buf_equal(&m, 0, 0x05) && buf_valid(&m, 1)) {
			int nmethod = (up->c.buf[1] & 0xFF);

			if (buf_valid(&m, nmethod + 1)) {
				buf[0] = 0x05;
				buf_init(&n, up->c.buf, nmethod + 2);
				if (buf_find(&n, 2, 0x02) && socks5_user_password[0]) {
					buf[1] = 0x02;
					ret = t->ops->op_write(t->fd, buf, 2);
					if (ret != 2)
						goto host_not_found;
					up->proto_flags |= AUTHED_1;
				} else if (socks5_user_password[0] == 0) {
					buf[1] = 0x00;
					ret = t->ops->op_write(t->fd, buf, 2);
					if (ret != 2)
						goto host_not_found;
					up->proto_flags |= AUTHED_Z;
				} else {
					fprintf(stderr, "authorization method not support!\n");
					buf[1] = 0xFF;
					t->ops->op_write(t->fd, buf, 2);
					goto host_not_found;
				}
				limit = up->c.buf + up->c.len;
				p = up->c.buf + nmethod + 2;
				memmove(up->c.buf, p, limit - p);
				up->c.len = (limit - p);
				up->proto_flags |= AUTHED_0;
				buf_init(&m, up->c.buf, up->c.len);
			}
		}
	}

	if ((up->m_flags & DOCONNECTING) == 0 &&
			(up->proto_flags & (AUTHED_1| AUTHED_0)) == (AUTHED_1| AUTHED_0)) {
		int l1 = (up->c.buf[1] & 0xFF);

		if (buf_equal(&m, 0, 0x01) && buf_valid(&m, l1 + 2)) {
			int l2 = (up->c.buf[l1 + 2] & 0xFF);
			if (buf_valid(&m, l1 + l2 + 2)) {
				int l3;
				char err[2] = {0x1, 0x02};
				l3 = strlen(socks5_user_password);

				fprintf(stderr, "user: %.*s\n", l1, up->c.buf + 2);
				fprintf(stderr, "password: %.*s\n", l2, up->c.buf + 3 + l1);
				if (memcmp(socks5_user_password, up->c.buf + 1, l3)) {
					t->ops->op_write(t->fd, err, 2);
					goto host_not_found;
				}

				err[1] = 0x0;
				ret = t->ops->op_write(t->fd, err, 2);
				if (ret != 2) {
					fprintf(stderr, "authorizating failure\n");
					goto host_not_found;
				}
				up->c.len -= (l1 + l2 + 3);
				memmove(up->c.buf, up->c.buf + l1 + l2 + 3, up->c.len);
				up->proto_flags &= ~AUTHED_1;
				up->proto_flags |= AUTHED_Z;
				buf_init(&m, up->c.buf, up->c.len);
			}
		}
	}

	if ((up->proto_flags & AUTHED_Z) &&
			(up->m_flags & DOCONNECTING) == 0) {
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
					memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
					up->s.buf[1] = 0x08;
					t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
					goto host_not_found;
			}

			up->addr_in1.sin_family = AF_INET;
			up->addr_in1.sin_port   = in_port1;
			up->addr_in1.sin_addr   = in_addr1;

			up->op_cmd = up->c.buf[1];
			switch (up->c.buf[1]) {
				case 0x00:
					fprintf(stderr, "socksv5 command udp ass not supported yet!\n");
					memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
					up->s.buf[1] = 0x07;
					t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
					goto host_not_found;

				case 0x01:
					up->c.len = (limit - end);
					memmove(up->c.buf, end, up->c.len);

					fprintf(stderr, "connect to %d\n", link_count);
					error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
					if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
						up->m_flags |= (error == 0? DIRECT_PROTO: DOCONNECTING);
						goto check_connecting;
					}
					goto host_not_found;

				case 0x03:
					if (listen(up->s.fd, 5) == 0) {
						int err1, err2;
						socklen_t alen;
						struct sockaddr_in addr1, addr2;
						alen = sizeof(addr1);
						err1 = getsockname(t->fd, (struct sockaddr *)&addr1, &alen);
						alen = sizeof(addr2);
						err2 = getsockname(up->s.fd, (struct sockaddr *)&addr2, &alen);

						if (0 == err1 && 0 == err2) {
							memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
							memcpy(&up->s.buf[4], &addr1.sin_addr, sizeof(addr1.sin_addr));
							memcpy(&up->s.buf[6], &addr2.sin_port, sizeof(addr2.sin_port));
							ret = t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
							if (ret != sizeof(resp_v5))
								goto host_not_found;
							up->m_flags |= DOCONNECTING;
							goto check_connecting;
						}
					}
					
					fprintf(stderr, "socksv5 command bind not supported yet!\n");
					memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
					up->s.buf[1] = 0x01;
					t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
					goto host_not_found;

				default:
					fprintf(stderr, "socksv5 command unkown!\n");
					memcpy(up->s.buf, resp_v5, sizeof(resp_v5));
					up->s.buf[1] = 0x07;
					t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
					goto host_not_found;
			}
			goto check_connecting;
		}
		goto check_protocol;
	}
	

check_connecting:
	if ((up->m_flags & DIRECT_PROTO) ||
			((up->m_flags & DOCONNECTING) && 
			 waitcb_completed(&up->s.wwait))) {

		int ret;
		socklen_t slen;
		struct sockspeer *t = &up->c;
		up->m_flags |= DIRECT_PROTO;
		up->m_flags &= ~(SOCKV5_PROTO| DOCONNECTING);
		memcpy(up->s.buf, resp_v5, sizeof(resp_v5));

		error = 0;
		if (up->op_cmd == 0x01) {
			memcpy(&up->s.buf[4], &up->addr_in1.sin_addr, 4);
			memcpy(&up->s.buf[8], &up->addr_in1.sin_port, 2);
			slen = sizeof(error);
			ret = getsockopt(up->s.fd, SOL_SOCKET, SO_ERROR, (char *)&error, &slen);
			if (ret != 0 || error != 0) {
				up->s.buf[1] = 0x05;
				error = -1;
			}
		} else if (up->op_cmd == 0x03) {
			int fd;
			struct sockcb *sockcbp;

			error = -1;
			sockcbp = NULL;
			slen = sizeof(up->addr_in1);
			fd = accept(up->s.fd, (struct sockaddr *)&up->addr_in1, &slen);
			up->s.buf[1] = 0x05;

			if (fd != -1) {
				sockcbp = sock_attach(fd);
				memcpy(&up->s.buf[4], &up->addr_in1.sin_addr, 4);
				memcpy(&up->s.buf[8], &up->addr_in1.sin_port, 2);
				if (sockcbp != NULL) {
					sock_detach(up->s.lwipcbp); 
					closesocket(up->s.fd);
					up->s.lwipcbp = sockcbp;
					up->s.fd = fd;
					up->s.buf[1] = 0x00;
					error = 0;
				}
			}

			if (error != 0) {
				if (sockcbp != NULL)
					sock_detach(sockcbp);
				if (fd != -1)
					closesocket(fd);
			}
		}

		ret = t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v5));
		if (ret != sizeof(resp_v5) || error == -1)
			goto host_not_found;

		return 0;
	}

	if ((up->m_flags & DOCONNECTING) == DOCONNECTING) {
		struct sockspeer *t = &up->c;

		if ((t->flags & NO_MORE_DATA) ||
				t->len == sizeof(t->buf)) {
			fprintf(stderr, "peer connection is close!\n");
			goto host_not_found;
		}

		switch (up->op_cmd) { 
			case 0x03:
				if (!waitcb_active(&up->s.wwait))
					t->ops->read_wait(up->s.lwipcbp, &up->s.wwait);
				break;

			case 0x01:
				if (!waitcb_active(&up->s.wwait))
					t->ops->write_wait(up->s.lwipcbp, &up->s.wwait);
				break;

			default:
				assert(0);
				break;
		}

		return 0;
	}

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

static int http_proto_input(struct socksproto *up)
{
	int error;
	int cutlen = 0;
	int use_post = 0;
	int is_magic_url = 1;
	struct in_addr in_addr1;
	char buf[sizeof(up->c.buf)] = "";
	char *bound, *port, *line, *p;

	p = (char *)memmem(up->c.buf, up->c.len, "\r\n\r\n", 4);
	if (p == NULL)
		p = (char *)memmem(up->c.buf, up->c.len, "\n\n", 2);

	if (p == NULL) {
		return 0;
	}

	if (sscanf(up->c.buf, "GET %s HTTP/1.%*d\r\n", buf) != 1) {
		if (sscanf(up->c.buf, "POST %s HTTP/1.%*d\r\n", buf) != 1) {
			fprintf(stderr, "4x HTTP ERRROR\n");
			goto host_not_found;
		}
		use_post = 1;
	}

	if (*http_maigc_url != 0) {
		is_magic_url = !strcmp(buf, http_maigc_url);
	}

	if (strncmp(buf, "http://", 7) != 0) {
		fprintf(stderr, "3x HTTP ERRROR\n");
		goto host_not_found;
	}

	line = buf + 7;
	if ((p = strchr(line, '/')) != NULL) {
		cutlen = (p - buf);
		*p = 0;
	} else {
		/* illegal url found. */
		fprintf(stderr, "1x HTTP ERRROR\n");
		goto host_not_found;
	}

	port = (char *)"80";
	bound = strchr(line, ':');
	if (bound != NULL) {
		*bound++ = 0;
		port = bound;
	}

	if (-1 == get_addr_by_name(line, &in_addr1)) {
		fprintf(stderr, "2x xx HTTP ERRROR %s\n", line);
		goto host_not_found;
	}

	up->addr_in1.sin_family = AF_INET;
	up->addr_in1.sin_port   = htons(atoi(port));
	up->addr_in1.sin_addr   = in_addr1;

	up->m_flags &= ~HTTP_PROTO;
	if (!check_proxy_authentication(up->c.buf)) {
		up->s.off = 0;
		if (is_magic_url != 0) {
			strcpy(up->s.buf, proxy_authentication_required);
			up->s.len = strlen(up->s.buf);
		} else {
			strcpy(up->s.buf, proxy_failure_request);
			up->s.len = strlen(up->s.buf);
		}
		up->c.flags |= NO_MORE_DATA;
		up->s.flags |= WRITE_BROKEN;
		up->s.flags |= NO_MORE_DATA;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

	fprintf(stderr, "http connect to %d\n", link_count);
	error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
	if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
		assert(up->c.len > cutlen);
		up->c.off = 0;
		up->c.len -= cutlen;
		p = up->c.buf + 4 + use_post;
		memmove(p, p + cutlen, up->c.len + 1);

		//{ strip Proxy Authorization information.
		p = (char *)get_line_by_key(up->c.buf, up->c.len, "Proxy-Authorization:");
		if (p != NULL) {
			char *limit;
			char *bound = up->c.buf + up->c.len;
			limit = (char *)memchr(p, '\n', bound - p);
			if (limit != NULL) {
				memmove(p, limit + 1, bound - limit - 1);
				up->c.len -= (limit + 1 - p);
			}
		}
		//}
		
		up->use_http = 1;
		up->s.len = up->s.off = 0;
		up->m_flags |= DOCONNECTING;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

host_not_found:
	fprintf(stderr, "HTTP ERRROR\n");
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
		fprintf(stderr, "1x HTTP ERRROR\n");
		goto host_not_found;
	}

	port = (char *)"80";
	bound = strchr(buf, ':');
	if (bound != NULL) {
		*bound++ = 0;
		port = bound;
	}

	if (get_addr_by_name(buf, &in_addr1)) {
		fprintf(stderr, "2x HTTP ERRROR\n");
		goto host_not_found;
	}

	up->c.len = (limit - end);
	memmove(up->c.buf, end, up->c.len);

	up->addr_in1.sin_family = AF_INET;
	up->addr_in1.sin_port   = htons(atoi(port));
	up->addr_in1.sin_addr   = in_addr1;

	up->m_flags &= ~HTTPS_PROTO;
	if (!check_proxy_authentication(up->c.buf)) {
		strcpy(up->s.buf, proxy_authentication_required);
		up->s.off = 0;
		up->s.len = strlen(up->s.buf);
		up->s.flags |= WRITE_BROKEN;
		up->s.flags |= NO_MORE_DATA;
		up->m_flags |= DIRECT_PROTO;
		return 0;
	}

	fprintf(stderr, "connect to %d\n", link_count);
	error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
	if (error == 0 || error_equal(up->s.fd, EINPROGRESS)) {
		memcpy(up->s.buf, resp_https, sizeof(resp_https));
		up->s.off = 0;
		up->s.len = sizeof(resp_https) - 1;
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

	if ((up->m_flags & DOCONNECTING) == 0) {
		if (buf[0] != 0x4 || buf[1] != 0x01)
			goto host_not_found;
		buf += 2;
		memcpy(&in_port1, buf, sizeof(in_port1));
		buf += 2;
		memcpy(&in_addr1, buf, sizeof(in_addr1));
		buf += 4;

		if (http_authorization[0] &&
				strcmp(http_authorization, buf)) {
			struct sockspeer *t = &up->c;
			memcpy(up->s.buf, resp_v4, sizeof(resp_v4));
			memcpy(&up->s.buf[4], &up->addr_in1.sin_addr, 4);
			memcpy(&up->s.buf[4], &up->addr_in1.sin_port, 2);
			up->s.buf[1] = 93;
			t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v4));
			goto host_not_found;
		}

		buf = (char *)memchr(buf, 0, limit - buf);
		/* use for sockv4a support */
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

		fprintf(stderr, "connect to %d\n", link_count);
		error = connect(up->s.fd, (struct sockaddr *)&up->addr_in1, sizeof(up->addr_in1));
		if (error == 0 || error_equal(up->s.fd, EINPROGRESS))
			up->m_flags |= (error == 0? DIRECT_PROTO: DOCONNECTING);
	}

	if ((up->m_flags & DIRECT_PROTO) ||
			((up->m_flags & DOCONNECTING) && 
			 waitcb_completed(&up->s.wwait))) {

		int ret;
		socklen_t slen;
		struct sockspeer *t = &up->c;
		up->m_flags |= DIRECT_PROTO;
		up->m_flags &= ~(SOCKV4_PROTO| DOCONNECTING);
		memcpy(up->s.buf, resp_v4, sizeof(resp_v4));
		memcpy(&up->s.buf[4], &up->addr_in1.sin_addr, 4);
		memcpy(&up->s.buf[2], &up->addr_in1.sin_port, 2);

		slen = sizeof(error);
		ret = getsockopt(up->s.fd, SOL_SOCKET, SO_ERROR, (char *)&error, &slen);
		if (ret != 0 || error != 0) {
			up->s.buf[1] = 91;
			error = -1;
		}

		ret = t->ops->op_write(t->fd, up->s.buf, sizeof(resp_v4));
		if (ret != sizeof(resp_v4) || error == -1)
			goto host_not_found;

		return 0;
	}

	if ((up->m_flags & DOCONNECTING) == DOCONNECTING) {
		struct sockspeer *t = &up->c;

		if ((t->flags & NO_MORE_DATA) ||
				t->len == sizeof(t->buf))
			goto host_not_found;

		if (!waitcb_active(&up->s.wwait))
			t->ops->write_wait(up->s.lwipcbp, &up->s.wwait);

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

static int do_http_forward(struct socksproto *up, 
		struct sockspeer *f, struct sockspeer *t, int def)
{
	int ret;
	int mask;
	int changed;
	const char *p;

	if ((f->flags & GET_LENGTHED) == 0x0) {
		fill_connect_buffer(f);
		p = (char *)memmem(f->buf, f->len, "\r\n\r\n", 4);
		if (p == NULL) {
			return 0;
		}

		f->flags |= GET_LENGTHED;
		if (check_transfer_encoding(f->buf, f->len, "chunked")) {
			f->flags |= FLAG_CHUNKED;
			f->limit = (p + 4 - f->buf);
			fprintf(stderr, "header length: %d\n", f->limit);
		} else {
			f->limit  = get_content_length(f->buf, f->len, def);
			if (f->limit != -1)
				f->limit += (p + 4 - f->buf);
			f->flags |= LASR_CHUNKED;
		}
	}

	do {
		changed = 0;
		mask = WRITE_BROKEN;
		if (f->limit != 0 || (f->flags & FLAG_CHUNKED) == 0) {
			int ll = (f->limit != -1 && f->limit < f->len - f->off)? f->limit: f->len - f->off;
			if (!(t->flags & mask) && ll > 0) {
				if (waitcb_completed(&t->wwait)) {
					ret = t->ops->op_write(t->fd, f->buf + f->off, ll);
					if (ret == -1 && !t->ops->blocking(t->fd)) {
						t->flags |= NO_MORE_DATA;
						t->flags |= WRITE_BROKEN;
						f->flags |= NO_MORE_DATA;
						DO_SHUTDOWN(f, t->off == t->len);
					} else if (ret < ll) {
						t->debug_write += (ret > 0? ret: 0);
						if (f->limit != -1)
							f->limit -= (ret > 0? ret: 0);
						f->off += (ret > 0? ret: 0);
						waitcb_clear(&t->wwait);
						changed = (ret > 0);
					} else {
						if (f->limit != -1)
							f->limit -= (ret > 0? ret: 0);
						DO_SHUTDOWN(t, (f->flags & NO_MORE_DATA));
						t->debug_write +=  (ret > 0? ret: 0);
						f->off += (ret > 0? ret: 0);
						if (f->off == f->len)
							f->off = f->len = 0;
						changed = 1;
					}
				}
			}
		} else if ((f->flags & (FLAG_CHUNKED|LASR_CHUNKED)) == FLAG_CHUNKED) {
			int lc;
			char *p;

			if (f->off > 0) {
				f->len -= f->off;
				memmove(f->buf, f->buf + f->off, f->len);
				f->off = 0;
			}

			fill_connect_buffer(f);
			assert(f->limit == 0);
			p = (char *)memmem(f->buf, f->len, "\r\n", 2);
			if (p != NULL && 1 == sscanf(f->buf, "%x", &lc)) {
				f->limit = (p + 4 - f->buf);
				f->limit += lc;
				if (lc == 0) {
					f->flags &= ~FLAG_CHUNKED;
					f->flags |= LASR_CHUNKED;
				}
				changed = 1;
			} else {
				waitcb_clear(&f->rwait);
				if (p != NULL) abort();
				break;
			}
		}

		mask = NO_MORE_DATA;
		if (!(f->flags & mask) && f->len < (int)sizeof(f->buf)) {
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
		if (!(f->flags & mask) && f->len < (int)sizeof(f->buf)) {
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
		fill_connect_buffer(&up->c);
		check_proxy_proto(up);
	}

	if (up->m_flags & FORWARD_PROTO) {
		fill_connect_buffer(&up->c);
		forward_proto_input(up);
	}

	if (up->m_flags & SOCKV4_PROTO) {
		fill_connect_buffer(&up->c);
		sockv4_proto_input(up);
	}

	if (up->m_flags & SOCKV5_PROTO) {
		fill_connect_buffer(&up->c);
		sockv5_proto_input(up);
	}

	if (up->m_flags & HTTPS_PROTO) {
		fill_connect_buffer(&up->c);
		https_proto_input(up);
	}

	if (up->m_flags & HTTP_PROTO) {
		fill_connect_buffer(&up->c);
		http_proto_input(up);
	}

	if (up->m_flags & UNKOWN_PROTO) {
		return 0;
	}

	if (up->m_flags & DIRECT_PROTO) {
		if (up->use_http) {
			do_http_forward(up, &up->c, &up->s, 0);
			do_http_forward(up, &up->s, &up->c, -1);
		} else  {
			do_data_forward(up, &up->s, &up->c);
			do_data_forward(up, &up->c, &up->s);
		}

		if (up->use_http) {
			if (up->s.limit == 0 && (up->s.flags & LASR_CHUNKED) && (up->s.flags & GET_LENGTHED)) {
				fprintf(stderr, "HTTP PIPELING: %d %d\n", up->c.off, up->c.len);

#if 0
				if ((up->c.flags & NO_MORE_DATA) == 0 && renew_http_socks(up)) {
					return socksproto_run(up);
				}
#endif

				return 0;
			}
		}

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

static int renew_http_socks(struct socksproto *up)
{
	int lwipfd;
	struct sockcb *sockcbp;

	lwipfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lwipfd == -1) {
		fprintf(stderr, "lwip create socket failure\n");
		return 0;
	}

	sockcbp = sock_attach(lwipfd);
	if (sockcbp == NULL) {
		closesocket(lwipfd);
		return 0;
	}

	waitcb_clear(&up->s.wwait);
	waitcb_clear(&up->s.rwait);
	sock_detach(up->s.lwipcbp);
	closesocket(up->s.fd);

	up->s.fd = lwipfd;
	setnonblock(up->s.fd);
	up->s.flags = 0;
	up->s.off = up->s.len = 0;
	up->s.ops = &winsock_ops;
	up->s.debug_read = 0;
	up->s.debug_write = 0;
	up->s.lwipcbp = sockcbp;

	up->c.flags = 0;
	return 1;
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

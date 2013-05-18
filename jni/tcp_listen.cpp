#include <stdio.h>
#include <assert.h>

#include <wait/module.h>
#include <wait/platform.h>
#include <wait/slotwait.h>
#include <wait/slotsock.h>

#include "pstcp_http.h"
#include "tcp_channel.h"

static int _port = 1080;
static int _lenfile = -1;
static struct sockcb *_sockcbp = 0;
static struct sockaddr_in _lenaddr;
static struct waitcb _event, _runstart, _runstop;

extern void new_tcp_socks(int tcpfd);
extern void clean_tcp_socks(void);
static void listen_statecb(void *ignore);
static void listen_callback(void *context);

extern "C" int proxy_setport(int port)
{
	_port = port;
	return 0;
}

void new_pstcp_channel(int fd);

static void module_init(void)
{
	int error;
	int optval = 1;

	_lenaddr.sin_family = AF_INET;
	_lenaddr.sin_port   = htons(_port);
	_lenaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	waitcb_init(&_event, listen_callback, NULL);
	waitcb_init(&_runstop, listen_statecb, (void *)0);
	waitcb_init(&_runstart, listen_statecb, (void *)1);

	_lenfile = socket(AF_INET, SOCK_STREAM, 0);
	assert(_lenfile != -1);

	error = setsockopt(_lenfile, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	assert(error == 0);

	error = bind(_lenfile, (struct sockaddr *)&_lenaddr, sizeof(_lenaddr));
	assert(error == 0);

	error = listen(_lenfile, 5);
	assert(error == 0);

	_sockcbp = sock_attach(_lenfile);
	slotwait_atstart(&_runstart);
	slotwait_atstop(&_runstop);
}

static void module_clean(void)
{
	sock_detach(_sockcbp);
	closesocket(_lenfile);
	waitcb_clean(&_event);
	waitcb_clean(&_runstop);
	waitcb_clean(&_runstart);
	fprintf(stderr, "tcp_listen: exiting\n");
}

void listen_statecb(void *ignore)
{
	int state;
	int error = -1;

	state = (int)(long)ignore;
	if (state == 0) {
		fprintf(stderr, "listen_stop\n");
		waitcb_cancel(&_event);
		return;
	}

	if (state == 1) {
		fprintf(stderr, "listen_start\n");
		error = sock_read_wait(_sockcbp, &_event);
		assert(error == 0);
	}
}

static void (*_on_accept_complete)(int fd) = new_tcp_socks;

extern "C" void set_server_type(int type)
{
	switch (type) {
		case 0:
			_on_accept_complete = new_tcp_socks;
			break;

		case 1:
			_on_accept_complete = new_pstcp_channel;
			break;

		default:
			_on_accept_complete = new_tcp_socks;
			break;
	}

	return;
}

void listen_callback(void *context)
{
	int newfd;
	int error;
	struct sockaddr_in newaddr;
	socklen_t newlen = sizeof(newaddr);

	newfd = accept(_lenfile, (struct sockaddr *)&newaddr, &newlen);
	if (newfd != -1) {
		fprintf(stderr, "new client: %s:%u\n",
				inet_ntoa(newaddr.sin_addr), ntohs(newaddr.sin_port));
		(*_on_accept_complete)(newfd);
	}

	error = sock_read_wait(_sockcbp, &_event);
	assert(error == 0);
}

struct module_stub tcp_listen_mod = {
	module_init, module_clean
};


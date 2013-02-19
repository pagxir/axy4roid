#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <wait/module.h>
#include <wait/platform.h>
#include <wait/slotwait.h>
#include <wait/callout.h>

int set_proxy_host(const char *name);
extern "C" int proxy_setport(int port);

static struct waitcb _jni_timer;
extern struct module_stub timer_mod;
extern struct module_stub slotsock_mod;
extern struct module_stub tcp_listen_mod;
struct module_stub *modules_list[] = {
	&timer_mod, &slotsock_mod, &tcp_listen_mod,
	NULL
};

static void flush_delack(void *up)
{
	callout_reset(&_jni_timer, 200);
}

extern "C" int start_proxy(void)
{
	signal(SIGPIPE, SIG_IGN);
	slotwait_held(0);
	initialize_modules(modules_list);
	slotwait_start();

	waitcb_init(&_jni_timer, flush_delack, NULL);
	callout_reset(&_jni_timer, 200);
	return 0;
}

extern "C" int loop_proxy(void)
{
	int result = 0;
	result = slotwait_step();
	return result;
}

extern "C" int stop_proxy(void)
{
	slotwait_stop();
	while(loop_proxy());

	waitcb_clean(&_jni_timer);
	cleanup_modules(modules_list);
	return 0;
}

static void init_proxy(int argc, char *argv[])
{
	int port;
	int optidx = 1;
	const char *proxy_host;

	proxy_host = argv[argc - 1];
	if (argc == 2 && strcmp(proxy_host, "-h") == 0) {
		fprintf(stderr, "%s [-l <port>] <proxy>\n", argv[0]);
		exit(0);
	}

	while (optidx++ < argc) {
		if (optidx  < argc &&
			strcmp(argv[optidx - 1], "-l") == 0) {
			port = atoi(argv[optidx]);
			fprintf(stderr, "listen port on %d\n", port);
			proxy_setport(port);
			break;
		}
	}

	if (*proxy_host != '-' && argc > optidx + 1) {
		fprintf(stderr, "use arg proxy: %s\n", proxy_host);
		set_proxy_host(proxy_host);
		return;
	}

	proxy_host = getenv("http_proxy");
	if (NULL != proxy_host) {
		fprintf(stderr, "use env proxy: %s\n", proxy_host);
		set_proxy_host(proxy_host);
		return;
	}

	proxy_host = "58.247.248.89:9418";
	fprintf(stderr, "use default internal proxy: %s\n", proxy_host);
	set_proxy_host(proxy_host);
	return;
}

int main(int argc, char *argv[])
{
	init_proxy(argc, argv);

	start_proxy();
	for ( ;loop_proxy(); );
	printf("EXITING\n");
	stop_proxy();
	printf("EXIT\n");
	return 0;
}


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "platform.h"

#include "module.h"
#include "slotwait.h"
#include "callout.h"

#include "tcp_device.h"
#include "tcp_channel.h"

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

int main(int argc, char *argv[])
{
    start_proxy();
    for ( ;loop_proxy(); );
	printf("EXITING\n");
    stop_proxy();
	printf("EXIT\n");
    return 0;
}


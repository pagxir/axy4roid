#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "platform.h"

#include "module.h"
#include "slotwait.h"

#include "tcp_device.h"
#include "tcp_channel.h"

extern struct module_stub timer_mod;
extern struct module_stub slotsock_mod;
extern struct module_stub tcp_listen_mod;
struct module_stub *modules_list[] = {
	&timer_mod, &slotsock_mod, &tcp_listen_mod, NULL
};

extern "C" int start_proxy(void)
{
	slotwait_held(0);
	initialize_modules(modules_list);
	slotwait_start();
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
	cleanup_modules(modules_list);
	return 0;
}


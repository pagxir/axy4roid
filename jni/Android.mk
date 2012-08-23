LOCAL_PATH := $(call my-dir)

OBJECTS = usertcp/rgnbuf.cpp usertcp/slotwait.cpp usertcp/slotsock.cpp usertcp/module.cpp usertcp/tcp_device.cpp \
		  usertcp/tcp_input.cpp usertcp/tcp_output.cpp usertcp/tcp_timer.cpp usertcp/slotipc.cpp \
		  usertcp/tcp_usrreq.cpp usertcp/callout.cpp usertcp/platform.cpp
SRV_OBJ = usertcp/server.cpp usertcp/pstcp_channel.cpp usertcp/pstcp_listen.cpp 
CLT_OBJ = usertcp/client.cpp usertcp/tcp_listen.cpp usertcp/tcp_channel.cpp
USRV_OBJ = usertcp/server.cpp usertcp/pstcp_listen.cpp usertcp/pstcp_http.cpp 
SOCKS_OBJ = usertcp/pstcp_socks.cpp 

#include $(CLEAR_VARS)
#LOCAL_MODULE := uhttpd
#LOCAL_SRC_FILES := $(USRV_OBJ) $(OBJECTS) 
#include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := proxy5
LOCAL_SRC_FILES := $(OBJECTS) appface.c proxy5.cpp tcp_listen.cpp tcp_socks.cpp
LOCAL_LDLIBS    := -llog
LOCAL_CXXFLAGS  := -Iusertcp
include $(BUILD_SHARED_LIBRARY)


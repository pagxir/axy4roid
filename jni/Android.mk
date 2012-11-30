STASH_PATH := $(call my-dir)

#include $(CLEAR_VARS)
#LOCAL_MODULE := uhttpd
#LOCAL_SRC_FILES := $(USRV_OBJ) $(OBJECTS) 
#include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(STASH_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE := proxy5
LOCAL_SRC_FILES := appface.c proxy5.cpp tcp_listen.cpp tcp_socks.cpp
LOCAL_LDLIBS    := -llog -lwait
LOCAL_LDFLAGS	:= -Llibwait
LOCAL_CXXFLAGS	:= -Ilibwait/include
include $(BUILD_SHARED_LIBRARY)


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libproxy5
LOCAL_SRC_FILES := appface.c proxy5.cpp tcp_listen.cpp tcp_socks.cpp pstcp_http.cpp
LOCAL_C_INCLUDES +=  $(JNI_H_INCLUDE) $(LOCAL_PATH)/libwait/include
LOCAL_CXX_INCLUDES +=  $(JNI_H_INCLUDE) $(LOCAL_PATH)/libwait/include
LOCAL_STATIC_LIBRARIES = libwait liblog
LOCAL_LDFLAGS += -llog
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
include $(call all-makefiles-under,$(LOCAL_PATH))

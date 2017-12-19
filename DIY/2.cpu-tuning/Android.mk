#
# Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by Fingerprint Cards AB.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := timer-test
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
                    $(ANDROID_ROOT)/hardware/libhardware/include \
                    $(ANDROID_ROOT)/system/core/include \
                    $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := timer.c

LOCAL_CFLAGS := -Wall -Wextra -std=c99 -fstack-protector-all -fstack-protector-strong
LOCAL_SHARED_LIBRARIES := liblog libdl

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := cpu-test
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
                    $(ANDROID_ROOT)/hardware/libhardware/include \
                    $(ANDROID_ROOT)/system/core/include \
                    $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := cpu.c

LOCAL_CFLAGS := -Wall -Wextra -std=c99 -fstack-protector-all -fstack-protector-strong
LOCAL_SHARED_LIBRARIES := liblog libdl

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := thread-test
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
                    $(ANDROID_ROOT)/hardware/libhardware/include \
                    $(ANDROID_ROOT)/system/core/include \
                    $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := thread.c

LOCAL_CFLAGS := -Wall -Wextra -std=c99 -fstack-protector-all -fstack-protector-strong
LOCAL_SHARED_LIBRARIES := liblog libdl

include $(BUILD_EXECUTABLE)

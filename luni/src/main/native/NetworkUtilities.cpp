/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "NetworkUtilities"

#include "NetworkUtilities.h"
#include "JNIHelp.h"
#include "JniConstants.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static bool byteArrayToSocketAddress(JNIEnv* env, jbyteArray byteArray, int port, sockaddr_storage* ss) {
    if (byteArray == NULL) {
        jniThrowNullPointerException(env, NULL);
        return false;
    }

    // Convert the IP address bytes to the proper IP address type.
    size_t addressLength = env->GetArrayLength(byteArray);
    memset(ss, 0, sizeof(*ss));
    if (addressLength == 4) {
        // IPv4 address.
        sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ss);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port);
        jbyte* dst = reinterpret_cast<jbyte*>(&sin->sin_addr.s_addr);
        env->GetByteArrayRegion(byteArray, 0, 4, dst);
    } else if (addressLength == 16) {
        // IPv6 address.
        sockaddr_in6* sin6 = reinterpret_cast<sockaddr_in6*>(ss);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        jbyte* dst = reinterpret_cast<jbyte*>(&sin6->sin6_addr.s6_addr);
        env->GetByteArrayRegion(byteArray, 0, 16, dst);
    } else {
        // We can't throw SocketException. We aren't meant to see bad addresses, so seeing one
        // really does imply an internal error.
        // TODO: fix the code (native and Java) so we don't paint ourselves into this corner.
        jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
                "byteArrayToSocketAddress bad array length (%i)", addressLength);
        return false;
    }
    return true;
}

jbyteArray socketAddressToByteArray(JNIEnv* env, const sockaddr_storage* ss) {
    const void* rawAddress;
    size_t addressLength;
    if (ss->ss_family == AF_INET) {
        const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(ss);
        rawAddress = &sin->sin_addr.s_addr;
        addressLength = 4;
    } else if (ss->ss_family == AF_INET6) {
        const sockaddr_in6* sin6 = reinterpret_cast<const sockaddr_in6*>(ss);
        rawAddress = &sin6->sin6_addr.s6_addr;
        addressLength = 16;
    } else {
        // We can't throw SocketException. We aren't meant to see bad addresses, so seeing one
        // really does imply an internal error.
        // TODO: fix the code (native and Java) so we don't paint ourselves into this corner.
        jniThrowExceptionFmt(env, "java/lang/IllegalArgumentException",
                "socketAddressToByteArray bad ss_family (%i)", ss->ss_family);
        return NULL;
    }

    jbyteArray byteArray = env->NewByteArray(addressLength);
    if (byteArray == NULL) {
        return NULL;
    }
    env->SetByteArrayRegion(byteArray, 0, addressLength, reinterpret_cast<const jbyte*>(rawAddress));
    return byteArray;
}

jobject byteArrayToInetAddress(JNIEnv* env, jbyteArray byteArray) {
    if (byteArray == NULL) {
        return NULL;
    }
    jmethodID getByAddressMethod = env->GetStaticMethodID(JniConstants::inetAddressClass,
            "getByAddress", "([B)Ljava/net/InetAddress;");
    if (getByAddressMethod == NULL) {
        return NULL;
    }
    return env->CallStaticObjectMethod(JniConstants::inetAddressClass, getByAddressMethod, byteArray);
}

jobject socketAddressToInetAddress(JNIEnv* env, const sockaddr_storage* ss) {
    jbyteArray byteArray = socketAddressToByteArray(env, ss);
    return byteArrayToInetAddress(env, byteArray);
}

bool inetAddressToSocketAddress(JNIEnv* env, jobject inetAddress, int port, sockaddr_storage* ss) {
    // Get the byte array that stores the IP address bytes in the InetAddress.
    if (inetAddress == NULL) {
        jniThrowNullPointerException(env, NULL);
        return false;
    }
    static jfieldID fid = env->GetFieldID(JniConstants::inetAddressClass, "ipaddress", "[B");
    jbyteArray addressBytes = reinterpret_cast<jbyteArray>(env->GetObjectField(inetAddress, fid));
    return byteArrayToSocketAddress(env, addressBytes, port, ss);
}

bool setBlocking(int fd, bool blocking) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return false;
    }

    if (!blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    int rc = fcntl(fd, F_SETFL, flags);
    return (rc != -1);
}

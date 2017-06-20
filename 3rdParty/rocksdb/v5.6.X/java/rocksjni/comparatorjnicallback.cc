// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
// This file implements the callback "bridge" between Java and C++ for
// rocksdb::Comparator.

#include "rocksjni/comparatorjnicallback.h"
#include "rocksjni/portal.h"

namespace rocksdb {
BaseComparatorJniCallback::BaseComparatorJniCallback(
    JNIEnv* env, jobject jComparator,
    const ComparatorJniCallbackOptions* copt)
    : mtx_compare(new port::Mutex(copt->use_adaptive_mutex)),
    mtx_findShortestSeparator(new port::Mutex(copt->use_adaptive_mutex)) {
  // Note: Comparator methods may be accessed by multiple threads,
  // so we ref the jvm not the env
  const jint rs = env->GetJavaVM(&m_jvm);
  if(rs != JNI_OK) {
    // exception thrown
    return;
  }

  // Note: we want to access the Java Comparator instance
  // across multiple method calls, so we create a global ref
  assert(jComparator != nullptr);
  m_jComparator = env->NewGlobalRef(jComparator);
  if(m_jComparator == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  // Note: The name of a Comparator will not change during it's lifetime,
  // so we cache it in a global var
  jmethodID jNameMethodId = AbstractComparatorJni::getNameMethodId(env);
  if(jNameMethodId == nullptr) {
    // exception thrown: NoSuchMethodException or OutOfMemoryError
    return;
  }
  jstring jsName = (jstring)env->CallObjectMethod(m_jComparator, jNameMethodId);
  if(env->ExceptionCheck()) {
    // exception thrown
    return;
  }
  jboolean has_exception = JNI_FALSE;
  m_name = JniUtil::copyString(env, jsName,
      &has_exception);  // also releases jsName
  if (has_exception == JNI_TRUE) {
    // exception thrown
    return;
  }

  m_jCompareMethodId = AbstractComparatorJni::getCompareMethodId(env);
  if(m_jCompareMethodId == nullptr) {
    // exception thrown: NoSuchMethodException or OutOfMemoryError
    return;
  }

  m_jFindShortestSeparatorMethodId =
    AbstractComparatorJni::getFindShortestSeparatorMethodId(env);
  if(m_jFindShortestSeparatorMethodId == nullptr) {
    // exception thrown: NoSuchMethodException or OutOfMemoryError
    return;
  }

  m_jFindShortSuccessorMethodId =
    AbstractComparatorJni::getFindShortSuccessorMethodId(env);
  if(m_jFindShortSuccessorMethodId == nullptr) {
    // exception thrown: NoSuchMethodException or OutOfMemoryError
    return;
  }
}

const char* BaseComparatorJniCallback::Name() const {
  return m_name.c_str();
}

int BaseComparatorJniCallback::Compare(const Slice& a, const Slice& b) const {
  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  // TODO(adamretter): slice objects can potentially be cached using thread
  // local variables to avoid locking. Could make this configurable depending on
  // performance.
  mtx_compare->Lock();

  bool pending_exception =
      AbstractSliceJni::setHandle(env, m_jSliceA, &a, JNI_FALSE);
  if(pending_exception) {
    if(env->ExceptionCheck()) {
      // exception thrown from setHandle or descendant
      env->ExceptionDescribe(); // print out exception to stderr
    }
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return 0;
  }

  pending_exception =
      AbstractSliceJni::setHandle(env, m_jSliceB, &b, JNI_FALSE);
  if(pending_exception) {
    if(env->ExceptionCheck()) {
      // exception thrown from setHandle or descendant
      env->ExceptionDescribe(); // print out exception to stderr
    }
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return 0;
  }
  
  jint result =
    env->CallIntMethod(m_jComparator, m_jCompareMethodId, m_jSliceA,
      m_jSliceB);

  mtx_compare->Unlock();

  if(env->ExceptionCheck()) {
    // exception thrown from CallIntMethod
    env->ExceptionDescribe(); // print out exception to stderr
    result = 0; // we could not get a result from java callback so use 0
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);

  return result;
}

void BaseComparatorJniCallback::FindShortestSeparator(
  std::string* start, const Slice& limit) const {
  if (start == nullptr) {
    return;
  }

  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  const char* startUtf = start->c_str();
  jstring jsStart = env->NewStringUTF(startUtf);
  if(jsStart == nullptr) {
    // unable to construct string
    if(env->ExceptionCheck()) {
      env->ExceptionDescribe(); // print out exception to stderr
    }
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }
  if(env->ExceptionCheck()) {
    // exception thrown: OutOfMemoryError
    env->ExceptionDescribe(); // print out exception to stderr
    env->DeleteLocalRef(jsStart);
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }

  // TODO(adamretter): slice object can potentially be cached using thread local
  // variable to avoid locking. Could make this configurable depending on
  // performance.
  mtx_findShortestSeparator->Lock();

  bool pending_exception =
      AbstractSliceJni::setHandle(env, m_jSliceLimit, &limit, JNI_FALSE);
  if(pending_exception) {
    if(env->ExceptionCheck()) {
      // exception thrown from setHandle or descendant
      env->ExceptionDescribe(); // print out exception to stderr
    }
    if(jsStart != nullptr) {
      env->DeleteLocalRef(jsStart);
    }
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }

  jstring jsResultStart =
    (jstring)env->CallObjectMethod(m_jComparator,
      m_jFindShortestSeparatorMethodId, jsStart, m_jSliceLimit);

  mtx_findShortestSeparator->Unlock();

  if(env->ExceptionCheck()) {
    // exception thrown from CallObjectMethod
    env->ExceptionDescribe();  // print out exception to stderr
    env->DeleteLocalRef(jsStart);
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }

  env->DeleteLocalRef(jsStart);

  if (jsResultStart != nullptr) {
    // update start with result
    jboolean has_exception = JNI_FALSE;
    std::string result = JniUtil::copyString(env, jsResultStart,
        &has_exception);  // also releases jsResultStart
    if (has_exception == JNI_TRUE) {
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();  // print out exception to stderr
      }
      JniUtil::releaseJniEnv(m_jvm, attached_thread);
      return;
    }

    *start = result;
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);
}

void BaseComparatorJniCallback::FindShortSuccessor(std::string* key) const {
  if (key == nullptr) {
    return;
  }

  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  const char* keyUtf = key->c_str();
  jstring jsKey = env->NewStringUTF(keyUtf);
  if(jsKey == nullptr) {
    // unable to construct string
    if(env->ExceptionCheck()) {
      env->ExceptionDescribe(); // print out exception to stderr
    }
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  } else if(env->ExceptionCheck()) {
    // exception thrown: OutOfMemoryError
    env->ExceptionDescribe(); // print out exception to stderr
    env->DeleteLocalRef(jsKey);
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }

  jstring jsResultKey =
    (jstring)env->CallObjectMethod(m_jComparator,
      m_jFindShortSuccessorMethodId, jsKey);

  if(env->ExceptionCheck()) {
    // exception thrown from CallObjectMethod
    env->ExceptionDescribe(); // print out exception to stderr
    env->DeleteLocalRef(jsKey);
    JniUtil::releaseJniEnv(m_jvm, attached_thread);
    return;
  }

  env->DeleteLocalRef(jsKey);

  if (jsResultKey != nullptr) {
    // updates key with result, also releases jsResultKey.
    jboolean has_exception = JNI_FALSE;
    std::string result = JniUtil::copyString(env, jsResultKey, &has_exception);
    if (has_exception == JNI_TRUE) {
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();  // print out exception to stderr
      }
      JniUtil::releaseJniEnv(m_jvm, attached_thread);
      return;
    }

    *key = result;
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);
}

BaseComparatorJniCallback::~BaseComparatorJniCallback() {
  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  if(m_jComparator != nullptr) {
    env->DeleteGlobalRef(m_jComparator);
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);
}

ComparatorJniCallback::ComparatorJniCallback(
    JNIEnv* env, jobject jComparator,
    const ComparatorJniCallbackOptions* copt) :
    BaseComparatorJniCallback(env, jComparator, copt) {
  m_jSliceA = env->NewGlobalRef(SliceJni::construct0(env));
  if(m_jSliceA == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  m_jSliceB = env->NewGlobalRef(SliceJni::construct0(env));
  if(m_jSliceB == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  m_jSliceLimit = env->NewGlobalRef(SliceJni::construct0(env));
  if(m_jSliceLimit == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }
}

ComparatorJniCallback::~ComparatorJniCallback() {
  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  if(m_jSliceA != nullptr) {
    env->DeleteGlobalRef(m_jSliceA);
  }

  if(m_jSliceB != nullptr) {
    env->DeleteGlobalRef(m_jSliceB);
  }

  if(m_jSliceLimit != nullptr) {
    env->DeleteGlobalRef(m_jSliceLimit);
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);
}

DirectComparatorJniCallback::DirectComparatorJniCallback(
    JNIEnv* env, jobject jComparator,
    const ComparatorJniCallbackOptions* copt) :
    BaseComparatorJniCallback(env, jComparator, copt) {
  m_jSliceA = env->NewGlobalRef(DirectSliceJni::construct0(env));
  if(m_jSliceA == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  m_jSliceB = env->NewGlobalRef(DirectSliceJni::construct0(env));
  if(m_jSliceB == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  m_jSliceLimit = env->NewGlobalRef(DirectSliceJni::construct0(env));
  if(m_jSliceLimit == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }
}

DirectComparatorJniCallback::~DirectComparatorJniCallback() {
  jboolean attached_thread = JNI_FALSE;
  JNIEnv* env = JniUtil::getJniEnv(m_jvm, &attached_thread);
  assert(env != nullptr);

  if(m_jSliceA != nullptr) {
    env->DeleteGlobalRef(m_jSliceA);
  }

  if(m_jSliceB != nullptr) {
    env->DeleteGlobalRef(m_jSliceB);
  }

  if(m_jSliceLimit != nullptr) {
    env->DeleteGlobalRef(m_jSliceLimit);
  }

  JniUtil::releaseJniEnv(m_jvm, attached_thread);
}
}  // namespace rocksdb

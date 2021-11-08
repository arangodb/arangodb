// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// This file implements the "bridge" between Java and C++ and enables
// calling c++ ROCKSDB_NAMESPACE::Iterator methods from Java side.

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "include/org_rocksdb_RocksIterator.h"
#include "rocksdb/iterator.h"
#include "rocksjni/portal.h"

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    disposeInternal
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_disposeInternal(JNIEnv* /*env*/,
                                                    jobject /*jobj*/,
                                                    jlong handle) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  assert(it != nullptr);
  delete it;
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    isValid0
 * Signature: (J)Z
 */
jboolean Java_org_rocksdb_RocksIterator_isValid0(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong handle) {
  return reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle)->Valid();
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seekToFirst0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_seekToFirst0(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong handle) {
  reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle)->SeekToFirst();
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seekToLast0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_seekToLast0(JNIEnv* /*env*/,
                                                jobject /*jobj*/,
                                                jlong handle) {
  reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle)->SeekToLast();
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    next0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_next0(JNIEnv* /*env*/, jobject /*jobj*/,
                                          jlong handle) {
  reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle)->Next();
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    prev0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_prev0(JNIEnv* /*env*/, jobject /*jobj*/,
                                          jlong handle) {
  reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle)->Prev();
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    refresh0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_refresh0(JNIEnv* env, jobject /*jobj*/,
                                            jlong handle) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Status s = it->Refresh();

  if (s.ok()) {
    return;
  }

  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seek0
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_RocksIterator_seek0(JNIEnv* env, jobject /*jobj*/,
                                          jlong handle, jbyteArray jtarget,
                                          jint jtarget_len) {
  jbyte* target = env->GetByteArrayElements(jtarget, nullptr);
  if (target == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Slice target_slice(reinterpret_cast<char*>(target),
                                        jtarget_len);

  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  it->Seek(target_slice);

  env->ReleaseByteArrayElements(jtarget, target, JNI_ABORT);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seekDirect0
 * Signature: (JLjava/nio/ByteBuffer;II)V
 */
void Java_org_rocksdb_RocksIterator_seekDirect0(JNIEnv* env, jobject /*jobj*/,
                                                jlong handle, jobject jtarget,
                                                jint jtarget_off,
                                                jint jtarget_len) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  auto seek = [&it](ROCKSDB_NAMESPACE::Slice& target_slice) {
    it->Seek(target_slice);
  };
  ROCKSDB_NAMESPACE::JniUtil::k_op_direct(seek, env, jtarget, jtarget_off,
                                          jtarget_len);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seekForPrevDirect0
 * Signature: (JLjava/nio/ByteBuffer;II)V
 */
void Java_org_rocksdb_RocksIterator_seekForPrevDirect0(
    JNIEnv* env, jobject /*jobj*/, jlong handle, jobject jtarget,
    jint jtarget_off, jint jtarget_len) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  auto seekPrev = [&it](ROCKSDB_NAMESPACE::Slice& target_slice) {
    it->SeekForPrev(target_slice);
  };
  ROCKSDB_NAMESPACE::JniUtil::k_op_direct(seekPrev, env, jtarget, jtarget_off,
                                          jtarget_len);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    seekForPrev0
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_RocksIterator_seekForPrev0(JNIEnv* env, jobject /*jobj*/,
                                                 jlong handle,
                                                 jbyteArray jtarget,
                                                 jint jtarget_len) {
  jbyte* target = env->GetByteArrayElements(jtarget, nullptr);
  if (target == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Slice target_slice(reinterpret_cast<char*>(target),
                                        jtarget_len);

  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  it->SeekForPrev(target_slice);

  env->ReleaseByteArrayElements(jtarget, target, JNI_ABORT);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    status0
 * Signature: (J)V
 */
void Java_org_rocksdb_RocksIterator_status0(JNIEnv* env, jobject /*jobj*/,
                                            jlong handle) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Status s = it->status();

  if (s.ok()) {
    return;
  }

  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    key0
 * Signature: (J)[B
 */
jbyteArray Java_org_rocksdb_RocksIterator_key0(JNIEnv* env, jobject /*jobj*/,
                                               jlong handle) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Slice key_slice = it->key();

  jbyteArray jkey = env->NewByteArray(static_cast<jsize>(key_slice.size()));
  if (jkey == nullptr) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }
  env->SetByteArrayRegion(
      jkey, 0, static_cast<jsize>(key_slice.size()),
      const_cast<jbyte*>(reinterpret_cast<const jbyte*>(key_slice.data())));
  return jkey;
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    keyDirect0
 * Signature: (JLjava/nio/ByteBuffer;II)I
 */
jint Java_org_rocksdb_RocksIterator_keyDirect0(JNIEnv* env, jobject /*jobj*/,
                                               jlong handle, jobject jtarget,
                                               jint jtarget_off,
                                               jint jtarget_len) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Slice key_slice = it->key();
  return ROCKSDB_NAMESPACE::JniUtil::copyToDirect(env, key_slice, jtarget,
                                                  jtarget_off, jtarget_len);
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    value0
 * Signature: (J)[B
 */
jbyteArray Java_org_rocksdb_RocksIterator_value0(JNIEnv* env, jobject /*jobj*/,
                                                 jlong handle) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Slice value_slice = it->value();

  jbyteArray jkeyValue =
      env->NewByteArray(static_cast<jsize>(value_slice.size()));
  if (jkeyValue == nullptr) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }
  env->SetByteArrayRegion(
      jkeyValue, 0, static_cast<jsize>(value_slice.size()),
      const_cast<jbyte*>(reinterpret_cast<const jbyte*>(value_slice.data())));
  return jkeyValue;
}

/*
 * Class:     org_rocksdb_RocksIterator
 * Method:    valueDirect0
 * Signature: (JLjava/nio/ByteBuffer;II)I
 */
jint Java_org_rocksdb_RocksIterator_valueDirect0(JNIEnv* env, jobject /*jobj*/,
                                                 jlong handle, jobject jtarget,
                                                 jint jtarget_off,
                                                 jint jtarget_len) {
  auto* it = reinterpret_cast<ROCKSDB_NAMESPACE::Iterator*>(handle);
  ROCKSDB_NAMESPACE::Slice value_slice = it->value();
  return ROCKSDB_NAMESPACE::JniUtil::copyToDirect(env, value_slice, jtarget,
                                                  jtarget_off, jtarget_len);
}

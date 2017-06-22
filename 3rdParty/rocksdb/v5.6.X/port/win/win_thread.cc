//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/win/win_thread.h"

#include <assert.h>
#include <process.h> // __beginthreadex
#include <windows.h>

#include <stdexcept>
#include <system_error>
#include <thread>

namespace rocksdb {
namespace port {

struct WindowsThread::Data {

  std::function<void()> func_;
  uintptr_t             handle_;

  Data(std::function<void()>&& func) :
    func_(std::move(func)),
    handle_(0) {
  }

  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;

  static unsigned int __stdcall ThreadProc(void* arg);
};


void WindowsThread::Init(std::function<void()>&& func) {

  data_.reset(new Data(std::move(func)));

  data_->handle_ = _beginthreadex(NULL,
    0,    // stack size
    &Data::ThreadProc,
    data_.get(),
    0,   // init flag
    &th_id_);

  if (data_->handle_ == 0) {
    throw std::system_error(std::make_error_code(
      std::errc::resource_unavailable_try_again),
      "Unable to create a thread");
  }
}

WindowsThread::WindowsThread() :
  data_(nullptr),
  th_id_(0)
{}


WindowsThread::~WindowsThread() {
  // Must be joined or detached
  // before destruction.
  // This is the same as std::thread
  if (data_) {
    if (joinable()) {
      assert(false);
      std::terminate();
    }
    data_.reset();
  }
}

WindowsThread::WindowsThread(WindowsThread&& o) noexcept :
  WindowsThread() {
  *this = std::move(o);
}

WindowsThread& WindowsThread::operator=(WindowsThread&& o) noexcept {

  if (joinable()) {
    assert(false);
    std::terminate();
  }

  data_ = std::move(o.data_);

  // Per spec both instances will have the same id
  th_id_ = o.th_id_;

  return *this;
}

bool WindowsThread::joinable() const {
  return (data_ && data_->handle_ != 0);
}

WindowsThread::native_handle_type WindowsThread::native_handle() const {
  return reinterpret_cast<native_handle_type>(data_->handle_);
}

unsigned WindowsThread::hardware_concurrency() {
  return std::thread::hardware_concurrency();
}

void WindowsThread::join() {

  if (!joinable()) {
    assert(false);
    throw std::system_error(
      std::make_error_code(std::errc::invalid_argument),
      "Thread is no longer joinable");
  }

  if (GetThreadId(GetCurrentThread()) == th_id_) {
    assert(false);
    throw std::system_error(
      std::make_error_code(std::errc::resource_deadlock_would_occur),
      "Can not join itself");
  }

  auto ret = WaitForSingleObject(reinterpret_cast<HANDLE>(data_->handle_),
    INFINITE);
  if (ret != WAIT_OBJECT_0) {
    auto lastError = GetLastError();
    assert(false);
    throw std::system_error(static_cast<int>(lastError),
      std::system_category(),
      "WaitForSingleObjectFailed");
  }

  CloseHandle(reinterpret_cast<HANDLE>(data_->handle_));
  data_->handle_ = 0;
}

bool WindowsThread::detach() {

  if (!joinable()) {
    assert(false);
    throw std::system_error(
      std::make_error_code(std::errc::invalid_argument),
      "Thread is no longer available");
  }

  BOOL ret = CloseHandle(reinterpret_cast<HANDLE>(data_->handle_));
  data_->handle_ = 0;

  return (ret == TRUE);
}

void  WindowsThread::swap(WindowsThread& o) {
  data_.swap(o.data_);
  std::swap(th_id_, o.th_id_);
}

unsigned int __stdcall  WindowsThread::Data::ThreadProc(void* arg) {
  auto data = reinterpret_cast<WindowsThread::Data*>(arg);
  data->func_();
  _endthreadex(0);
  return 0;
}
} // namespace port
} // namespace rocksdb

//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
#pragma once

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {
namespace port {

// Install a signal handler to print callstack on the following signals:
// SIGILL SIGSEGV SIGBUS SIGABRT
// Currently supports linux only. No-op otherwise.
void InstallStackTraceHandler();

// Prints stack, skips skip_first_frames frames
void PrintStack(int first_frames_to_skip = 0);

// Prints the given callstack
void PrintAndFreeStack(void* callstack, int num_frames);

// Save the current callstack
void* SaveStack(int* num_frame, int first_frames_to_skip = 0);

}  // namespace port
}  // namespace ROCKSDB_NAMESPACE

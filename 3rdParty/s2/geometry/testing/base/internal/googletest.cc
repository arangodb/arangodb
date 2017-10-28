// Copyright 2010-2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "testing/base/public/googletest.h"

#ifdef OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif  // OS_WIN

#include <climits>
#include <string>

#include "base/file_util.h"
#include "base/init.h"
#include "base/logging.h"
#include "base/util.h"

DEFINE_string(test_srcdir, "",
              "A directory that contains the input data files for a test.");

DEFINE_string(test_tmpdir, "",
              "Directory for all temporary testing files.");

DECLARE_string(program_invocation_name);

namespace mozc {
namespace {

#include "testing/mozc_data_dir.h"

#ifdef OS_WIN
string GetProgramPath() {
  wchar_t w_path[MAX_PATH];
  const DWORD char_size = GetModuleFileNameW(NULL, w_path, arraysize(w_path));
  if (char_size == 0) {
    LOG(ERROR) << "GetModuleFileNameW failed.  error = " << ::GetLastError();
    return "";
  } else if (char_size >= arraysize(w_path)) {
    LOG(ERROR) << "The result of GetModuleFileNameW was truncated.";
    return "";
  }
  string path;
  Util::WideToUTF8(w_path, &path);
  return path;
}

string GetTestSrcdir() {
  const string srcdir(kMozcDataDir);
  CHECK(FileUtil::DirectoryExists(srcdir)) << srcdir << " is not a directory.";
  return srcdir;
}

string GetTestTmpdir() {
  const string tmpdir = GetProgramPath() + ".tmp";

  if (!FileUtil::DirectoryExists(tmpdir)) {
    CHECK(FileUtil::CreateDirectory(tmpdir));
  }
  return tmpdir;
}

#else  // OS_WIN

#ifndef MOZC_USE_PEPPER_FILE_IO
// Get absolute path to this executable. Corresponds to argv[0] plus
// directory information. E.g like "/spam/eggs/foo_unittest".
string GetProgramPath() {
  const string &program_invocation_name = FLAGS_program_invocation_name;
  if (program_invocation_name.empty() || program_invocation_name[0] == '/') {
    return program_invocation_name;
  }

  // Turn relative filename into absolute
  char cwd_buf[PATH_MAX+1];
  CHECK_GT(getcwd(cwd_buf, PATH_MAX), 0);
  cwd_buf[PATH_MAX] = '\0';  // make sure it's terminated
  return FileUtil::JoinPath(cwd_buf, program_invocation_name);
}
#endif  // MOZC_USE_PEPPER_FILE_IO

string GetTestSrcdir() {
  const string srcdir(kMozcDataDir);

#ifndef MOZC_USE_PEPPER_FILE_IO
  // TestSrcdir is not supported in NaCl.
  // TODO(horo): Consider how to implement TestSrcdir in NaCl.
  // FIXME(komatsu): We should implement "genrule" and "exports_files"
  // in build.py to install the data files into srcdir.
  CHECK_EQ(access(srcdir.c_str(), R_OK|X_OK), 0)
      << "Access failure: " << srcdir;
#endif  // MOZC_USE_PEPPER_FILE_IO
  return srcdir;
}

#ifndef MOZC_USE_PEPPER_FILE_IO
string GetTestTmpdir() {
  const string tmpdir = GetProgramPath() + ".tmp";

  // GetTestTmpdir is not supported in NaCl.
  // TODO(horo): Consider how to implement TestTmpdir in NaCl.
  if (access(tmpdir.c_str(), R_OK|X_OK) != 0) {
    CHECK(FileUtil::CreateDirectory(tmpdir));
  }
  return tmpdir;
}
#endif  // MOZC_USE_PEPPER_FILE_IO
#endif  // OS_WIN

}  // namespace

void InitTestFlags() {
  if (FLAGS_test_srcdir.empty()) {
    FLAGS_test_srcdir = GetTestSrcdir();
  }
  if (FLAGS_test_tmpdir.empty()) {
#ifndef MOZC_USE_PEPPER_FILE_IO
    FLAGS_test_tmpdir = GetTestTmpdir();
#else  // MOZC_USE_PEPPER_FILE_IO
    FLAGS_test_tmpdir = "/";
#endif  // MOZC_USE_PEPPER_FILE_IO
  }
}

}  // namespace mozc

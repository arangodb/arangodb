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

#include "base/base.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

#ifdef __native_client__
#include "base/pepper_file_system_mock.h"
#include "base/pepper_file_util.h"
#endif  // __native_client__

int main(int argc, char **argv) {
  // TODO(yukawa, team): Implement b/2805528 so that you can specify any option
  // given by gunit.
  InitGoogle(argv[0], &argc, &argv, false);
  mozc::InitTestFlags();
  testing::InitGoogleTest(&argc, argv);

#ifdef OS_WINDOWS
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif  // OS_WINDOWS

  // Without this flag, ::RaiseException makes the job stuck.
  // See b/2805521 for details.
  testing::GTEST_FLAG(catch_exceptions) = true;

#ifdef __native_client__
  // Sets Pepper file system mock.
  mozc::PepperFileSystemMock pepper_file_system_mock;
  mozc::PepperFileUtil::SetPepperFileSystemInterfaceForTest(
      &pepper_file_system_mock);
#endif  // __native_client__

  return RUN_ALL_TESTS();
}

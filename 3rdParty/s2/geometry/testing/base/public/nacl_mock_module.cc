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

#include <errno.h>
#include <pthread.h>
#include <sys/types.h>

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/url_loader.h>
#include <ppapi/cpp/url_request_info.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <cstring>
#include <memory>

#include "base/port.h"
#include "base/logging.h"
#include "base/pepper_file_util.h"
#include "net/http_client_pepper.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace pp {
namespace {

class NaclTestInstance : public pp::Instance {
 public:
  explicit NaclTestInstance(PP_Instance instance)
      : pp::Instance(instance) {
    cc_factory_.Initialize(this);
    pthread_create(&thread_handle_,
                   0,
                   &NaclTestInstance::ThreadFunc,
                   static_cast<void *>(this));
  }
  virtual ~NaclTestInstance() {}
  virtual void HandleMessage(const pp::Var &var_message) {}

 private:
  static void *ThreadFunc(void *ptr);
  void TestFinish(int32_t result);
  void OnUrlLoaderOpen(int32_t result);
  pthread_t thread_handle_;
  pp::CompletionCallbackFactory<NaclTestInstance> cc_factory_;
  std::unique_ptr<pp::URLRequestInfo> url_request_;
  std::unique_ptr<pp::URLLoader> url_loader_;

  DISALLOW_COPY_AND_ASSIGN(NaclTestInstance);
};

void *NaclTestInstance::ThreadFunc(void *ptr) {
  mozc::InitTestFlags();
  NaclTestInstance *self = static_cast<NaclTestInstance *>(ptr);
  mozc::RegisterPepperInstanceForHTTPClient(self);
  mozc::PepperFileUtil::Initialize(self, 1024);
  const int ret = RUN_ALL_TESTS();
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      self->cc_factory_.NewCallback(&NaclTestInstance::TestFinish),
      ret);
  return NULL;
}

void NaclTestInstance::TestFinish(int32_t result) {
  url_request_.reset(new pp::URLRequestInfo(this));
  url_loader_.reset(new pp::URLLoader(this));
  if (result == 0) {
    url_request_->SetURL("http://127.0.0.1:9999/TEST_FIN?result=success");
  } else {
    url_request_->SetURL("http://127.0.0.1:9999/TEST_FIN?result=failed");
  }
  url_request_->SetMethod("GET");
  url_loader_->Open(
      *url_request_,
      cc_factory_.NewCallback(&NaclTestInstance::OnUrlLoaderOpen));
}

void NaclTestInstance::OnUrlLoaderOpen(int32_t result) {
  // Do Nothing
}

class NaclTestModule : public pp::Module {
 public:
  NaclTestModule() : pp::Module() {}
  virtual ~NaclTestModule() {}

 protected:
  virtual pp::Instance *CreateInstance(PP_Instance instance) {
    return new NaclTestInstance(instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaclTestModule);
};

}  // namespace

Module* CreateModule() {
  int argc = 1;
  char argv0[] = "NaclModule";
  char *argv_body[] = {argv0, NULL};
  char **argv = argv_body;
  InitGoogle(argv[0], &argc, &argv, true);
  testing::InitGoogleTest(&argc, argv);

  return new NaclTestModule();
}

}  // namespace pp

extern "C" {
// The following functions are not implemented in NaCl environment.
// But the gtest library requires these functions in link phase.
// So We implement these dummy functions.

char *getcwd(char *buf, size_t size) {
  LOG(WARNING) << "dummy getcwd called";
  if (size < 5) {
    errno = ENAMETOOLONG;
    return NULL;
  }
  memcpy(buf, "/tmp", size);
  return buf;
}

int access(const char *pathname, int mode) {
  LOG(WARNING) << "dummy access called pathname: \"" << pathname
               << "\" mode: " << mode;
  return -1;
}

int unlink(const char *pathname) {
  LOG(WARNING) << "dummy unlink called pathname: \"" << pathname << "\"";
  return -1;
}

int mkdir(const char *pathname, mode_t mode) {
  LOG(WARNING) << "dummy mkdir called pathname: \"" << pathname << "\""
               << " mode: " << mode;
  return -1;
}
}  // extern "C"

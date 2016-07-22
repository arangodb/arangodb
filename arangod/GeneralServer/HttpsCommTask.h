////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
//  @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTPS_COMM_TASK_H
#define ARANGOD_HTTP_SERVER_HTTPS_COMM_TASK_H 1

#include "GeneralServer/HttpCommTask.h"

#include <openssl/ssl.h>

namespace arangodb {
namespace rest {
class GeneralServer;

class HttpsCommTask : public HttpCommTask {
  HttpsCommTask(HttpsCommTask const&) = delete;
  HttpsCommTask const& operator=(HttpsCommTask const&) = delete;

 private:
  static size_t const READ_BLOCK_SIZE = 10000;

 public:
  HttpsCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&,
                double keepAliveTimeout, SSL_CTX* ctx, int verificationMode,
                int (*verificationCallback)(int, X509_STORE_CTX*));

 protected:
  ~HttpsCommTask();

 protected:
  bool setup(Scheduler*, EventLoop) override;
  bool handleEvent(EventToken, EventType) override;
  bool fillReadBuffer() override;
  bool handleWrite() override;

 private:
  bool trySSLAccept();
  bool trySSLRead();
  bool trySSLWrite();
  void shutdownSsl(bool initShutdown);

 private:
  bool _accepted;
  bool _readBlockedOnWrite;
  bool _writeBlockedOnRead;
  char* _tmpReadBuffer;
  SSL* _ssl;
  SSL_CTX* _ctx;
  int _verificationMode;
  int (*_verificationCallback)(int, X509_STORE_CTX*);
};
}  // rest
}  // arango

#endif

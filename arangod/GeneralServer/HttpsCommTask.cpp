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

#include "HttpsCommTask.h"
#include <openssl/err.h>

#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "GeneralServer/GeneralServer.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Ssl/ssl-helper.h"

using namespace arangodb;
using namespace arangodb::rest;

HttpsCommTask::HttpsCommTask(GeneralServer* server, TRI_socket_t socket,
                             ConnectionInfo&& info, double keepAliveTimeout,
                             SSL_CTX* ctx, int verificationMode,
                             int (*verificationCallback)(int, X509_STORE_CTX*))
    : Task("HttpsCommTask"),
      HttpCommTask(server, socket, std::move(info), keepAliveTimeout),
      _accepted(false),
      _readBlockedOnWrite(false),
      _writeBlockedOnRead(false),
      _tmpReadBuffer(nullptr),
      _ssl(nullptr),
      _ctx(ctx),
      _verificationMode(verificationMode),
      _verificationCallback(verificationCallback) {
  _tmpReadBuffer = new char[READ_BLOCK_SIZE];
}

HttpsCommTask::~HttpsCommTask() {
  shutdownSsl(true);

  delete[] _tmpReadBuffer;
}

bool HttpsCommTask::setup(Scheduler* scheduler, EventLoop loop) {
  // setup base class
  bool ok = HttpCommTask::setup(scheduler, loop);

  if (!ok) {
    return false;
  }

  // build a new connection
  TRI_ASSERT(_ssl == nullptr);

  ERR_clear_error();
  _ssl = SSL_new(_ctx);

  _connectionInfo.sslContext = _ssl;

  if (_ssl == nullptr) {
    LOG(DEBUG) << "cannot build new SSL connection: " << lastSSLError();

    shutdownSsl(false);
    return false;  // terminate ourselves, ssl is nullptr
  }

  // enforce verification
  ERR_clear_error();
  SSL_set_verify(_ssl, _verificationMode, _verificationCallback);

  // with the file descriptor
  ERR_clear_error();
  SSL_set_fd(_ssl, (int)TRI_get_fd_or_handle_of_socket(_commSocket));

  return true;
}

bool HttpsCommTask::handleEvent(EventToken token, EventType revents) {
  // try to accept the SSL connection
  if (!_accepted) {
    bool result = false;  // be pessimistic

    if ((token == _readWatcher && (revents & EVENT_SOCKET_READ)) ||
        (token == _writeWatcher && (revents & EVENT_SOCKET_WRITE))) {
      // must do the SSL handshake first
      result = trySSLAccept();
    }

    if (result) {
      _scheduler->startSocketEvents(_readWatcher);
      _scheduler->stopSocketEvents(_writeWatcher);
    } else {
      // status is somehow invalid. we got here even though no accept was ever
      // successful
      _clientClosed = true;
      _scheduler->destroyTask(this);
    }

    return result;
  }

  // if we blocked on write, read can be called when the socket is writeable
  if (_readBlockedOnWrite && token == _writeWatcher &&
      (revents & EVENT_SOCKET_WRITE)) {
    _readBlockedOnWrite = false;
    revents &= ~EVENT_SOCKET_WRITE;
    revents |= EVENT_SOCKET_READ;
    token = _readWatcher;
  }

  // handle normal socket operation
  bool result = HttpCommTask::handleEvent(token, revents);

  // warning: if _clientClosed is true here, the task (this) is already deleted!

  // we might need to start listing for writes (even we only want to READ)
  if (result && !_clientClosed) {
    if (_readBlockedOnWrite || _writeBlockedOnRead) {
      _scheduler->startSocketEvents(_writeWatcher);
    }
  }

  return result;
}

bool HttpsCommTask::fillReadBuffer() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  // is the handshake already done?
  if (!_accepted) {
    return false;
  }

  return trySSLRead();
}

bool HttpsCommTask::handleWrite() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  // is the handshake already done?
  if (!_accepted) {
    return false;
  }

  return trySSLWrite();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief accepts SSL connection
////////////////////////////////////////////////////////////////////////////////

bool HttpsCommTask::trySSLAccept() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  ERR_clear_error();
  int res = SSL_accept(_ssl);

  // accept successful
  if (res == 1) {
    LOG(DEBUG) << "established SSL connection";
    _accepted = true;
    return true;
  }

  // shutdown of connection
  if (res == 0) {
    LOG(DEBUG) << "SSL_accept failed: " << lastSSLError();

    shutdownSsl(false);
    return false;
  }

  // maybe we need more data
  int err = SSL_get_error(_ssl, res);

  if (err == SSL_ERROR_WANT_READ) {
    _scheduler->startSocketEvents(_readWatcher);
    _scheduler->stopSocketEvents(_writeWatcher);
    return true;
  } else if (err == SSL_ERROR_WANT_WRITE) {
    _scheduler->stopSocketEvents(_readWatcher);
    _scheduler->startSocketEvents(_writeWatcher);
    return true;
  }

  LOG(TRACE) << "error in SSL handshake: " << lastSSLError();

  shutdownSsl(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads from SSL connection
////////////////////////////////////////////////////////////////////////////////

bool HttpsCommTask::trySSLRead() {
  _readBlockedOnWrite = false;

again:
  ERR_clear_error();
  int nr = SSL_read(_ssl, _tmpReadBuffer, READ_BLOCK_SIZE);

  if (nr <= 0) {
    int res = SSL_get_error(_ssl, nr);

    switch (res) {
      case SSL_ERROR_NONE:
        return true;

      case SSL_ERROR_SSL:
        LOG(DEBUG) << "received SSL error (bytes read " << nr << ", socket "
                   << TRI_get_fd_or_handle_of_socket(_commSocket)
                   << "): " << lastSSLError();

        shutdownSsl(false);
        return false;

      case SSL_ERROR_ZERO_RETURN:
        shutdownSsl(true);
        _clientClosed = true;
        return false;

      case SSL_ERROR_WANT_READ:
        // we must retry with the EXACT same parameters later
        return true;

      case SSL_ERROR_WANT_WRITE:
        _readBlockedOnWrite = true;
        return true;

      case SSL_ERROR_WANT_CONNECT:
        LOG(DEBUG) << "received SSL_ERROR_WANT_CONNECT";
        break;

      case SSL_ERROR_WANT_ACCEPT:
        LOG(DEBUG) << "received SSL_ERROR_WANT_ACCEPT";
        break;

      case SSL_ERROR_SYSCALL:
        if (res != 0) {
          LOG(DEBUG) << "SSL_read returned syscall error with: "
                     << lastSSLError();
        } else if (nr == 0) {
          LOG(DEBUG)
              << "SSL_read returned syscall error because an EOF was received";
        } else {
          LOG(DEBUG) << "SSL_read return syscall error: " << errno << ": "
                     << strerror(errno);
        }

        shutdownSsl(false);
        return false;

      default:
        LOG(DEBUG) << "received error with " << res << " and " << nr << ": "
                   << lastSSLError();

        shutdownSsl(false);
        return false;
    }
  } else {
    _readBuffer.appendText(_tmpReadBuffer, nr);
    _readBuffer.ensureNullTerminated();

    // we might have more data to read
    // if we do not iterate again, the reading process would stop
    goto again;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes from SSL connection
////////////////////////////////////////////////////////////////////////////////

bool HttpsCommTask::trySSLWrite() {
  _writeBlockedOnRead = false;

  size_t len = 0;

  if (nullptr != _writeBuffer) {
    TRI_ASSERT(_writeBuffer->length() >= _writeLength);

    // size_t is unsigned, should never get < 0
    len = _writeBuffer->length() - _writeLength;
  }

  // write buffer to SSL connection
  int nr = 0;

  if (0 < len) {
    ERR_clear_error();

    nr = SSL_write(_ssl, _writeBuffer->begin() + _writeLength, (int)len);

    if (nr <= 0) {
      int res = SSL_get_error(_ssl, nr);

      switch (res) {
        case SSL_ERROR_NONE:
          return true;

        case SSL_ERROR_ZERO_RETURN:
          shutdownSsl(true);
          _clientClosed = true;
          return false;

        case SSL_ERROR_WANT_CONNECT:
          LOG(DEBUG) << "received SSL_ERROR_WANT_CONNECT";
          break;

        case SSL_ERROR_WANT_ACCEPT:
          LOG(DEBUG) << "received SSL_ERROR_WANT_ACCEPT";
          break;

        case SSL_ERROR_WANT_WRITE:
          // we must retry with the EXACT same parameters later
          return true;

        case SSL_ERROR_WANT_READ:
          _writeBlockedOnRead = true;
          return true;

        case SSL_ERROR_SYSCALL:
          if (res != 0) {
            LOG(DEBUG) << "SSL_write returned syscall error with: "
                       << lastSSLError();
          } else if (nr == 0) {
            LOG(DEBUG) << "SSL_write returned syscall error because an EOF was "
                          "received";
          } else {
            LOG(DEBUG) << "SSL_write return syscall error: " << errno << ": "
                       << strerror(errno);
          }

          shutdownSsl(false);
          return false;

        default:
          LOG(DEBUG) << "received error with " << res << " and " << nr << ": "
                     << lastSSLError();

          shutdownSsl(false);
          return false;
      }
    } else {
      len -= nr;
    }
  }

  if (len == 0) {
    completedWriteBuffer();
  } else if (nr > 0) {
    // nr might have been negative here
    _writeLength += nr;
  }

  // return immediately, everything is closed down
  if (_clientClosed) {
    return false;
  }

  // we might have a new write buffer or none at all
  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  } else {
    _scheduler->startSocketEvents(_writeWatcher);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the SSL connection
////////////////////////////////////////////////////////////////////////////////

void HttpsCommTask::shutdownSsl(bool initShutdown) {
  static int const SHUTDOWN_ITERATIONS = 10;

  if (nullptr != _ssl) {
    if (initShutdown) {
      bool ok = false;

      for (int i = 0; i < SHUTDOWN_ITERATIONS; ++i) {
        ERR_clear_error();
        int res = SSL_shutdown(_ssl);

        if (res == 1) {
          ok = true;
          break;
        }

        if (res == -1) {
          int err = SSL_get_error(_ssl, res);

          if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            LOG(DEBUG) << "received shutdown error with " << res << ", " << err
                       << ": " << lastSSLError();
            break;
          }
        }
      }

      if (!ok) {
        LOG(DEBUG) << "cannot complete SSL shutdown in socket "
                   << TRI_get_fd_or_handle_of_socket(_commSocket);
      }
    } else {
      ERR_clear_error();
      SSL_clear(_ssl);
    }

    ERR_clear_error();
    SSL_free(_ssl);  // this will free bio as well

    _ssl = nullptr;
  }

  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }
}

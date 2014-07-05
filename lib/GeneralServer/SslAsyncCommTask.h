////////////////////////////////////////////////////////////////////////////////
/// @brief task for ssl communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GENERAL_SERVER_SSL_ASYNC_COMM_TASK_H
#define ARANGODB_GENERAL_SERVER_SSL_ASYNC_COMM_TASK_H 1

#include "Basics/Common.h"

#include "GeneralServer/GeneralAsyncCommTask.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "Basics/ssl-helper.h"
#include "BasicsC/socket-utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "BasicsC/logging.h"


namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief task for ssl communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class SslAsyncCommTask : public GeneralAsyncCommTask<S, HF, CT> {
      private:
        static size_t const READ_BLOCK_SIZE = 10000;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

        SslAsyncCommTask (S* server,
                          TRI_socket_t& socket,
                          ConnectionInfo& info,
                          double keepAliveTimeout,
                          SSL_CTX* ctx,
                          int verificationMode,
                          int (*verificationCallback)(int, X509_STORE_CTX*))
        : Task("SslAsyncCommTask"),
          GeneralAsyncCommTask<S, HF, CT>(server, socket, info, keepAliveTimeout),
          _accepted(false),
          _readBlocked(false),
          _readBlockedOnWrite(false),
          _writeBlockedOnRead(false),
          _ssl(nullptr),
          _ctx(ctx),
          _verificationMode(verificationMode),
          _verificationCallback(verificationCallback) {
          tmpReadBuffer = new char[READ_BLOCK_SIZE];
        }

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

        ~SslAsyncCommTask () {
          shutdownSsl();

          delete[] tmpReadBuffer;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief setup SSL stuff
////////////////////////////////////////////////////////////////////////////////

        bool setup (Scheduler* scheduler, EventLoop loop) {

          // setup base class
          bool ok = GeneralAsyncCommTask<S, HF, CT>::setup(scheduler, loop);

          if (! ok) {
            return false;
          }

          // build a new connection
          _ssl = SSL_new(_ctx);

          this->_connectionInfo.sslContext = _ssl;

          if (_ssl == nullptr) {
            LOG_WARNING("cannot build new SSL connection: %s", triagens::basics::lastSSLError().c_str());

            shutdownSsl();
            return false;   // terminate ourselves, ssl is nullptr
          }

          // enforce verification
          SSL_set_verify(_ssl, _verificationMode, _verificationCallback);

          // with the file descriptor
          SSL_set_fd(_ssl, (int) TRI_get_fd_or_handle_of_socket(this->_commSocket));

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleEvent (EventToken token, EventType revents) {
          bool result = GeneralAsyncCommTask<S, HF, CT>::handleEvent(token, revents);

          if (result) {
            if (_readBlockedOnWrite) {
              this->_scheduler->startSocketEvents(this->writeWatcher);
            }
          }

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool fillReadBuffer (bool& closed) {
          if (nullptr == _ssl) {
            closed = true;
            return false;
          }

          closed = false;

          // is the handshake already done?
          if (! _accepted) {
            if (! trySSLAccept()) {
              LOG_DEBUG("failed to establish SSL connection");
              return false;
            }

            return true;
          }

          // check if read is blocked by write
          if (_writeBlockedOnRead) {
            return trySSLWrite(closed, ! this->hasWriteBuffer());
          }

          return trySSLRead(closed);
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleWrite (bool& closed, bool noWrite) {
          if (nullptr == _ssl) {
            closed = true;
            return false;
          }

          // is the handshake already done?
          if (! _accepted) {
            if (! trySSLAccept()) {
              LOG_DEBUG("failed to establish SSL connection");
              return false;
            }

            return true;
          }

          // check if write is blocked by read
          if (_readBlockedOnWrite) {
            if (! trySSLRead(closed)) {
              return false;
            }

            return this->handleRead(closed);
          }

          return trySSLWrite(closed, noWrite);
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief accepts SSL connection
////////////////////////////////////////////////////////////////////////////////

        bool trySSLAccept () {
          int res = SSL_accept(_ssl);

          // accept successful
          if (res == 1) {
            LOG_DEBUG("established SSL connection");
            _accepted = true;
            return true;
          }

          // shutdown of connection
          else if (res == 0) {
            LOG_DEBUG("SSL_accept failed: %s", triagens::basics::lastSSLError().c_str());

            shutdownSsl();
            return false;
          }

          // maybe we need more data
          else {
            int err = SSL_get_error(_ssl, res);

            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
              return true;
            }
            else {
              LOG_TRACE("error in SSL handshake: %s", triagens::basics::lastSSLError().c_str());

              shutdownSsl();
              return false;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief reads from SSL connection
////////////////////////////////////////////////////////////////////////////////

        bool trySSLRead (bool& closed) {
          closed = false;
          _readBlocked = false;
          _readBlockedOnWrite = false;

again:
          int nr = SSL_read(_ssl, tmpReadBuffer, READ_BLOCK_SIZE);

          if (nr <= 0) {
            int res = SSL_get_error(_ssl, nr);

            switch (res) {
              case SSL_ERROR_NONE:
                LOG_WARNING("unknown error in SSL_read");

                shutdownSsl();
                return false;

              case SSL_ERROR_SSL:
                LOG_WARNING("received SSL error (bytes read %d, socket %d): %s",
                            nr,
                            (int) TRI_get_fd_or_handle_of_socket(this->_commSocket),
                            triagens::basics::lastSSLError().c_str());

                shutdownSsl();
                return false;

              case SSL_ERROR_ZERO_RETURN:
                LOG_WARNING("received SSL_ERROR_ZERO_RETURN");

                shutdownSsl();
                closed = true;
                return false;

              case SSL_ERROR_WANT_READ:
                _readBlocked = true;
                break;

              case SSL_ERROR_WANT_WRITE:
                _readBlockedOnWrite = true;
                break;

              case SSL_ERROR_WANT_CONNECT:
                LOG_WARNING("received SSL_ERROR_WANT_CONNECT");
                break;

              case SSL_ERROR_WANT_ACCEPT:
                LOG_WARNING("received SSL_ERROR_WANT_ACCEPT");
                break;

              case SSL_ERROR_SYSCALL:
                {
                  unsigned long err = ERR_peek_error();

                  if (err != 0) {
                    LOG_WARNING("SSL_read returned syscall error with: %s", triagens::basics::lastSSLError().c_str());
                  }
                  else if (nr == 0) {
                    LOG_WARNING("SSL_read returned syscall error because an EOF was received");
                  }
                  else {
                    LOG_WARNING("SSL_read return syscall error: %d: %s", (int) errno, strerror(errno));
                  }

                  shutdownSsl();
                  return false;
                }

              default:
                LOG_WARNING("received error with %d and %d: %s", res, nr, triagens::basics::lastSSLError().c_str());

                shutdownSsl();
                return false;
            }
          }
          else {
            this->_readBuffer->appendText(tmpReadBuffer, nr);

            // we might have more data to read
            // if we do not iterate again, the reading process would stop
            goto again;
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief writes from SSL connection
////////////////////////////////////////////////////////////////////////////////

        bool trySSLWrite (bool& closed, bool noWrite) {
          closed = false;

          // if no write buffer is left, return
          if (noWrite) {
            return true;
          }

          bool callCompletedWriteBuffer = false;

          {
            MUTEX_LOCKER(this->writeBufferLock);

            // size_t is unsigned, should never get < 0
            TRI_ASSERT(this->_writeBuffer->length() >= this->writeLength);

            // write buffer to SSL connection
            size_t len = this->_writeBuffer->length() - this->writeLength;
            int nr = 0;

            if (0 < len) {
              _writeBlockedOnRead = false;
              nr = SSL_write(_ssl, this->_writeBuffer->begin() + this->writeLength, (int) len);

              if (nr <= 0) {
                int res = SSL_get_error(_ssl, nr);

                switch (res) {
                  case SSL_ERROR_NONE:
                    LOG_WARNING("unknown error in SSL_write");
                    break;

                  case SSL_ERROR_ZERO_RETURN:
                    shutdownSsl();
                    closed = true;
                    return false;

                  case SSL_ERROR_WANT_CONNECT:
                    LOG_WARNING("received SSL_ERROR_WANT_CONNECT");
                    break;

                  case SSL_ERROR_WANT_ACCEPT:
                    LOG_WARNING("received SSL_ERROR_WANT_ACCEPT");
                    break;

                  case SSL_ERROR_WANT_WRITE:
                    shutdownSsl();
                    return false;

                  case SSL_ERROR_WANT_READ:
                    _writeBlockedOnRead = true;
                    return true;

                  case SSL_ERROR_SYSCALL:
                    {
                      unsigned long err = ERR_peek_error();

                      if (err != 0) {
                        LOG_DEBUG("SSL_read returned syscall error with: %s", triagens::basics::lastSSLError().c_str());
                      }
                      else if (nr == 0) {
                        LOG_DEBUG("SSL_read returned syscall error because an EOF was received");
                      }
                      else {
                        LOG_DEBUG("SSL_read return syscall error: %d: %s", errno, strerror(errno));
                      }

                      shutdownSsl();
                      return false;
                    }

                  default:
                    LOG_DEBUG("received error with %d and %d: %s", res, nr, triagens::basics::lastSSLError().c_str());

                    shutdownSsl();
                    return false;
                }
              }
              else {
                len -= nr;
              }
            }

            if (len == 0) {
              if (this->ownBuffer) {
                delete this->_writeBuffer;
              }

              callCompletedWriteBuffer = true;
            }
            else {
              this->writeLength += nr;
            }
          }

          // we have to release the lock, before calling completedWriteBuffer
          if (callCompletedWriteBuffer) {
            this->completedWriteBuffer(closed);

            if (closed) {
              shutdownSsl();
              return false;
            }
          }

          // we might have a new write buffer
          this->_scheduler->sendAsync(this->SocketTask::watcher);

          return true;
        }

      private:
        void shutdownSsl () {
          static int const SHUTDOWN_ITERATIONS = 10;
          bool ok = false;

          if (nullptr != _ssl) {
            for (int i = 0;  i < SHUTDOWN_ITERATIONS;  ++i) {
              if (SSL_shutdown(_ssl) != 0) {
                ok = true;
                break;
              }
            }

            if (! ok) {
              LOG_WARNING("cannot complete SSL shutdown");
            }

            SSL_free(_ssl); // this will free bio as well

            _ssl= nullptr;
          }

          TRI_CLOSE_SOCKET(this->_commSocket);
          TRI_invalidatesocket(&this->_commSocket);
        }

      private:
        bool _accepted;
        bool _readBlocked;
        bool _readBlockedOnWrite;
        bool _writeBlockedOnRead;

        char * tmpReadBuffer;

        SSL* _ssl;
        SSL_CTX* _ctx;
        int _verificationMode;
        int (*_verificationCallback)(int, X509_STORE_CTX*);
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

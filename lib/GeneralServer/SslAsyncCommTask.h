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
          // if we blocked on write, read can be called when the socket is writeable
          if (_readBlockedOnWrite && token == this->writeWatcher && (revents & EVENT_SOCKET_WRITE)) {
            _readBlockedOnWrite = false;
            revents &= ~EVENT_SOCKET_WRITE;
            revents |= EVENT_SOCKET_READ;
            token = this->readWatcher;
          }

          if (_readBlockedOnWrite && token == this->readWatcher && (revents & EVENT_SOCKET_READ)) {
            return true;
          }

          // if we blocked on read, write can be called when the socket is readable
          if (_writeBlockedOnRead && token == this->readWatcher && (revents & EVENT_SOCKET_READ)) {
            _writeBlockedOnRead = false;
            revents &= ~EVENT_SOCKET_READ;
            revents |= EVENT_SOCKET_WRITE;
            token = this->writeWatcher;
          }

          if (_writeBlockedOnRead && token == this->writeWatcher && (revents & EVENT_SOCKET_WRITE)) {
            return true;
          }

          if (! _accepted && token == this->readWatcher && (revents & EVENT_SOCKET_READ)) {
            return trySSLAccept();
          }

          if (! _accepted && token == this->writeWatcher && (revents & EVENT_SOCKET_WRITE)) {
            return trySSLAccept();
          }

          // handle normal socket operation
          bool result = GeneralAsyncCommTask<S, HF, CT>::handleEvent(token, revents);

          // we might need to start listing for writes (even we only want to READ)
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
            return false;
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
            return false;
          }

          return trySSLWrite(closed, noWrite);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief accepts SSL connection
////////////////////////////////////////////////////////////////////////////////

        bool trySSLAccept () {
          if (nullptr == _ssl) {
            return false;
          }

          _readBlockedOnWrite = false;
          _writeBlockedOnRead = false;

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

            if (err == SSL_ERROR_WANT_READ) {
              _readBlockedOnWrite = true;
              this->_scheduler->startSocketEvents(this->writeWatcher);
              return true;
            }
            else if (err == SSL_ERROR_WANT_WRITE) {
              _writeBlockedOnRead = true;
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
                // we must retry with the EXCAT same parameters later
                return true;

              case SSL_ERROR_WANT_WRITE:
                _readBlockedOnWrite = true;
                return true;

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
          _writeBlockedOnRead = false;

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
                    // we must retry with the EXACT same parameters later
                    return true;

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

            // return immediately, everything is closed down
            if (closed) {
              return false;
            }
          }

          // we might have a new write buffer
          this->_scheduler->sendAsync(this->SocketTask::watcher);

          return true;
        }

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
              LOG_WARNING("cannot complete SSL shutdown in socket %d",
                          (int) TRI_get_fd_or_handle_of_socket(this->_commSocket));
            }

            SSL_free(_ssl); // this will free bio as well

            _ssl = nullptr;
          }

          if (TRI_isvalidsocket(this->_commSocket)) {
            TRI_CLOSE_SOCKET(this->_commSocket);
            TRI_invalidatesocket(&this->_commSocket);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:
        bool _accepted;
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

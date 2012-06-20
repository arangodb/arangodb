////////////////////////////////////////////////////////////////////////////////
/// @brief task for https communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpsAsyncCommTask.h"

#include <openssl/err.h>

#include <Logger/Logger.h>
#include <Basics/MutexLocker.h>
#include <Basics/StringBuffer.h>
#include <Rest/HttpRequest.h>

#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // SSL helper functions
    // -----------------------------------------------------------------------------

    namespace {
      string lastSSLError () {
        char buf[122];
        memset(buf, 0, sizeof(buf));

        unsigned long err = ERR_get_error();
        ERR_error_string_n(err, buf, sizeof(buf) - 1);

        return string(buf);
      }
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpsAsyncCommTask::HttpsAsyncCommTask (HttpServer* server,
                                            socket_t fd,
                                            ConnectionInfo const& info,
                                            BIO* bio)
      : Task("HttpsAsyncCommTask"),
        GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>(server, fd, info),
        accepted(false),
        readBlocked(false),
        readBlockedOnWrite(false),
        writeBlockedOnRead(false),
        ssl((SSL*) info.sslContext),
        bio(bio) {
      tmpReadBuffer = new char[READ_BLOCK_SIZE];
    }



    HttpsAsyncCommTask::~HttpsAsyncCommTask () {
      static int const SHUTDOWN_ITERATIONS = 10;
      bool ok = false;

      for (int i = 0;  i < SHUTDOWN_ITERATIONS;  ++i) {
        if (SSL_shutdown(ssl) != 0) {
          ok = true;
          break;
        }
      }

      if (! ok) {
        LOGGER_WARNING << "cannot complete SSL shutdown";
      }

      SSL_free(ssl); // this will free bio as well
      delete[] tmpReadBuffer;
    }

    // -----------------------------------------------------------------------------
    // Task methods
    // -----------------------------------------------------------------------------

    bool HttpsAsyncCommTask::handleEvent (EventToken token, EventType revents) {
      bool result = GeneralAsyncCommTask<HttpServer, HttpHandlerFactory, HttpCommTask>::handleEvent(token, revents);

      if (result) {
        if (readBlockedOnWrite) {
          scheduler->startSocketEvents(writeWatcher);
        }
      }

      return result;
    }

    // -----------------------------------------------------------------------------
    // SocketTask methods
    // -----------------------------------------------------------------------------

    bool HttpsAsyncCommTask::fillReadBuffer (bool& closed) {
      closed = false;

      // is the handshake already done?
      if (! accepted) {
        if (! trySSLAccept()) {
          LOGGER_DEBUG << "failed to established SSL connection";
          return false;
        }

        return true;
      }

      // check if read is blocked by write
      if (writeBlockedOnRead) {
        return trySSLWrite(closed, ! hasWriteBuffer());
      }

      return trySSLRead(closed);
    }



    bool HttpsAsyncCommTask::handleWrite (bool& closed, bool noWrite) {

      // is the handshake already done?
      if (! accepted) {
        if (! trySSLAccept()) {
          LOGGER_DEBUG << "failed to established SSL connection";
          return false;
        }

        return true;
      }

      // check if write is blocked by read
      if (readBlockedOnWrite) {
        if (! trySSLRead(closed)) {
          return false;
        }

        return handleRead(closed);
      }

      return trySSLWrite(closed, noWrite);
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    bool HttpsAsyncCommTask::trySSLAccept () {
      int res = SSL_accept(ssl);

      // accept successful
      if (res == 1) {
        LOGGER_DEBUG << "established SSL connection";
        accepted = true;
        return true;
      }

      // shutdown of connection
      else if (res == 0) {
        LOGGER_DEBUG << "SSL_accept failed";
        LOGGER_DEBUG << lastSSLError();
        return false;
      }

      // maybe we need more data
      else {
        int err = SSL_get_error(ssl, res);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
          return true;
        }
        else {
          LOGGER_INFO << "error in SSL handshake";
          LOGGER_INFO << lastSSLError();
          return false;
        }
      }
    }



    bool HttpsAsyncCommTask::trySSLRead (bool& closed) {
      closed = false;
      readBlocked = false;
      readBlockedOnWrite = false;

      int nr = SSL_read(ssl, tmpReadBuffer, READ_BLOCK_SIZE);

      if (nr <= 0) {
        int res = SSL_get_error(ssl, nr);

        switch (res) {
          case SSL_ERROR_NONE:
            LOGGER_INFO << "unknown error in SSL_read";
            return false;

          case SSL_ERROR_ZERO_RETURN:
            closed = true;
            SSL_shutdown(ssl);
            return false;

          case SSL_ERROR_WANT_READ:
            readBlocked = true;
            break;

          case SSL_ERROR_WANT_WRITE:
            readBlockedOnWrite = true;
            break;

          case SSL_ERROR_WANT_CONNECT:
            LOGGER_INFO << "received SSL_ERROR_WANT_CONNECT";
            break;

          case SSL_ERROR_WANT_ACCEPT:
            LOGGER_INFO << "received SSL_ERROR_WANT_ACCEPT";
            break;

          case SSL_ERROR_SYSCALL: {
            unsigned long err = ERR_peek_error();

            if (err != 0) {
              LOGGER_DEBUG << "SSL_read returned syscall error with: " << lastSSLError();
            }
            else if (nr == 0) {
              LOGGER_DEBUG << "SSL_read returned syscall error because an EOF was received";
            }
            else {
              LOGGER_DEBUG << "SSL_read return syscall error: " << errno << ", " << strerror(errno);
            }

            return false;
          }

          default:
            LOGGER_DEBUG << "received error with " << res << " and " << nr << ": " << lastSSLError();
            return false;
        }
      }
      else {
        readBuffer->appendText(tmpReadBuffer, nr);
      }

      return true;
    }



    bool HttpsAsyncCommTask::trySSLWrite (bool& closed, bool noWrite) {
      closed = false;

      // if no write buffer is left, return
      if (noWrite) {
        return true;
      }

      bool callCompletedWriteBuffer = false;

      {
        MUTEX_LOCKER(writeBufferLock);

        // write buffer to SSL connection
        size_t len = writeBuffer->length() - writeLength;
        int nr = 0;

        if (0 < len) {
          writeBlockedOnRead = false;
          nr = SSL_write(ssl, writeBuffer->begin() + writeLength, (int) len);

          if (nr <= 0) {
            int res = SSL_get_error(ssl, nr);

            switch (res) {
              case SSL_ERROR_NONE:
                LOGGER_INFO << "unknown error in SSL_write";
                break;

              case SSL_ERROR_ZERO_RETURN:
                closed = true;
                SSL_shutdown(ssl);
                return false;
                break;

              case SSL_ERROR_WANT_CONNECT:
                LOGGER_INFO << "received SSL_ERROR_WANT_CONNECT";
                break;

              case SSL_ERROR_WANT_ACCEPT:
                LOGGER_INFO << "received SSL_ERROR_WANT_ACCEPT";
                break;

              case SSL_ERROR_WANT_WRITE:
                return false;

              case SSL_ERROR_WANT_READ:
                writeBlockedOnRead = true;
                return true;

              case SSL_ERROR_SYSCALL: {
                unsigned long err = ERR_peek_error();

                if (err != 0) {
                  LOGGER_DEBUG << "SSL_read returned syscall error with: " << lastSSLError();
                }
                else if (nr == 0) {
                  LOGGER_DEBUG << "SSL_read returned syscall error because an EOF was received";
                }
                else {
                  LOGGER_DEBUG << "SSL_read return syscall error: " << errno << ", " << strerror(errno);
                }

                return false;
              }

              default:
                LOGGER_DEBUG << "received error with " << res << " and " << nr << ": " << lastSSLError();
                return false;
            }
          }
          else {
            len -= nr;
          }
        }

        if (len == 0) {
          if (ownBuffer) {
            delete writeBuffer;
          }

          writeBuffer = 0;
          callCompletedWriteBuffer = true;
        }
        else {
          writeLength += nr;
        }
      }

      // we have to release the lock, before calling completedWriteBuffer
      if (callCompletedWriteBuffer) {
        completedWriteBuffer(closed);

        if (closed) {
          return false;
        }
      }

      // we might have a new write buffer
      scheduler->sendAsync(SocketTask::watcher);

      return true;
    }
  }
}

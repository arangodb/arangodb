////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Cellar
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "FileHandler.hpp"

using namespace arangodb;

FileHandler::FileHandler() : _feature{nullptr} {}

FileHandler::~FileHandler() {}

void FileHandler::initializeEncryption() {
  #ifdef USE_ENTERPRISE
    _feature =
        application_features::ApplicationServer::getFeature<EncryptionFeature>(
            "Encryption");
  #endif
}

bool FileHandler::writeData(int fd, char const* data, size_t len) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    return _feature->writeData(fd, data, len);
  } else {
    return TRI_WritePointer(fd, data, len);
  }
#else
  return TRI_WritePointer(fd, data, len);
#endif
}

void FileHandler::beginEncryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    bool result = _feature->beginEncryption(fd);

    if (!result) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "cannot write prefix, giving up!";
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

void FileHandler::endEncryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    _feature->endEncryption(fd);
  }
#endif
}

ssize_t FileHandler::readData(int fd, char* data, size_t len) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    return _feature->readData(fd, data, len);
  }
#endif
  return TRI_READ(fd, data, static_cast<TRI_read_t>(len));
}

void FileHandler::beginDecryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    bool result = _feature->beginDecryption(fd);

    if (!result) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "cannot write prefix, giving up!";
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

void FileHandler::endDecryption(int fd) {
#ifdef USE_ENTERPRISE
  if (_feature) {
    _feature->endDecryption(fd);
  }
#endif
}

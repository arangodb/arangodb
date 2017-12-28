////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOSH_UTILS_FILE_HANDLER_H
#define ARANGOSH_UTILS_FILE_HANDLER_H 1

#include "Basics/files.h"

namespace arangodb {
class EncryptionFeature;

class FileHandler {
 public:
  FileHandler();
  virtual ~FileHandler();

  /**
   * @brief Prepares handler for encryption if enabled
   */
  void initializeEncryption();

  /**
   * @brief Writes data to the given file, using encryption if enabled
   * @param  fd   File to write to
   * @param  data Beginning of data to write
   * @param  len  Length of data to write
   * @return      `true` if successful
   */
  bool writeData(int fd, char const* data, size_t len);

  /**
   * @brief Prepares given file for encryption if enabled
   * @param fd File to prepare
   */
  void beginEncryption(int fd);

  /**
   * @brief Finalizes encryption of given file, if enabled
   * @param fd File to finalize
   */
  void endEncryption(int fd);

  /**
   * @brief Reads data from the given file, using decryption if enabled
   * @param  fd   File to read from
   * @param  data Buffer to store the read data
   * @param  len  Maximum amount of data to read (no more than buffer length)
   * @return      `true` if successful
   */
  ssize_t readData(int fd, char* data, size_t len);

  /**
   * @brief Prepares to read from decrypted file, if enabled
   * @param fd File to prepare
   */
  void beginDecryption(int fd);

  /**
   * @brief Finalizes decryption of a given file, if enabled
   * @param fd File to finalize
   */
  void endDecryption(int fd);

 private:
  EncryptionFeature* _feature;
};
}  // namespace arangodb

#endif

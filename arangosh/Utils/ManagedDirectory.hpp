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

#ifndef ARANGOSH_UTILS_MANAGED_DIRECTORY_H
#define ARANGOSH_UTILS_MANAGED_DIRECTORY_H 1

#include "Basics/Result.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace arangodb {
class ManagedDirectory {
 public:
  static constexpr int DefaultReadFlags = O_RDONLY | TRI_O_CLOEXEC;
  static constexpr int DefaultWriteFlags =
      O_WRONLY | TRI_O_CLOEXEC | O_CREAT | O_EXCL;

 public:
  class File {
   public:
    File(ManagedDirectory const& directory, std::string const& filename,
         int flags);
    ~File();

   public:
    /**
     * @brief Returns a reference to the status of the file
     * @return Reference to result object describing file status
     */
    Result const& status() const noexcept;

    /**
     * @brief Returns a reference to the path to the file
     * @return Reference to file path
     */
    std::string const& path() const noexcept;

    /**
     * @brief Writes data to the given file, using encryption if enabled
     * @param  data   Beginning of data to write
     * @param  length Length of data to write
     * @return        Reference to file status
     */
    Result const& write(char const* data, size_t length) noexcept;

    /**
     * @brief Reads data from the given file, using decryption if enabled
     * @param  buffer Buffer to store the read data
     * @param  length Maximum amount of data to read (no more than buffer
     *                length)
     * @return        Reference to file status
     */
    std::pair<Result const&, ssize_t> read(char* buffer, size_t length) noexcept;

    /**
     * @brief Closes file (now, as opposed to when the object is destroyed)
     * @return Reference to file status
     */
    Result const& close() noexcept;

   private:
    ManagedDirectory const& _directory;
    std::string _path;
    int _flags;
    int _fd;
#ifdef USE_ENTERPRISE
    std::unique_ptr<EncryptionFeature::Context> _context;
#endif
    Result _status;
  };

 public:
  ManagedDirectory(std::string const& path, bool requireEmpty, bool create);
  ~ManagedDirectory();

 public:
#ifdef USE_ENTERPRISE
  /**
   * @brief Returns a pointer to the `EncryptionFeature` instance
   * @return A pointer to the feature
   */
  EncryptionFeature const* encryptionFeature() const noexcept;
#endif

  /**
   * @brief Returns a reference to the status of the directory
   *
   * Status must be `.ok()` in order to open any new files.
   *
   * @return Reference to result object describing directory status
   */
  Result const& status() const noexcept;

  /**
   * @brief Resets the status to allow for further operation after error
   */
  void resetStatus() noexcept;

  /**
   * @brief Determines if encryption is enabled on the directory
   * @return `true` if encryption is enabled
   */
  bool encryptionEnabled() const noexcept;

  /**
   * @brief Returns the path to the directory under management
   * @return Path under management
   */
  std::string const& path() const noexcept;

  /**
   * @brief Opens a readable file
   * @param  filename The filename, relative to the directory
   * @param  flags    Flags (will be XORed with `DefaultReadFlags`
   * @return          Unique pointer to file, if opened
   */
  std::unique_ptr<File> readableFile(std::string const& filename,
                                     int flags = 0) noexcept;

  /**
   * @brief Opens a writable file
   * @param  name      The filename, relative to the directory
   * @param  overwrite Whether to overwrite file if it exists (otherwise fail)
   * @param  flags     Flags (will be XORed with `DefaultWriteFlags`
   * @return           Unique pointer to file, if opened
   */
  std::unique_ptr<File> writableFile(std::string const& filename,
                                      bool overwrite, int flags = 0) noexcept;

 private:
#ifdef USE_ENTERPRISE
  EncryptionFeature* const _encryptionFeature;
#endif
  std::string const _path;
  Result _status;
};
}  // namespace arangodb

#endif

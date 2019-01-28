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

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Result.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace arangodb {
/**
 * Manages a single directory in the file system, transparently handling
 * encryption and decryption. Opens/creates and manages file using RAII-style
 * management.
 *
 * If encryption is enabled on the server, new files will be encrypted, and
 * existing files will be decrypted if applicable.
 */
class ManagedDirectory {
 public:
  static constexpr int DefaultReadFlags = O_RDONLY | TRI_O_CLOEXEC;
  static constexpr int DefaultWriteFlags = O_WRONLY | TRI_O_CLOEXEC | O_CREAT | O_EXCL;

 public:
  class File {
   public:
    /**
     * @brief Opens or creates a file within the directory
     *
     * Should not be called directly. See `ManagedDirectory::readableFile` and
     * `ManagedDirectory::writableFile`.
     *
     * @param directory A reference to the containing directory
     * @param filename  The name of the file within the directory
     * @param flags     The flags to pass to the OS to open the file
     */
    File(ManagedDirectory const& directory, std::string const& filename, int flags);
    /**
     * @brief Closes the file if it is still open
     */
    ~File();

   public:
    /**
     * @brief Returns a reference to the status of the file
     * @return Reference to result object describing file status
     */
    Result const& status() const;

    /**
     * @brief Returns a reference to the path to the file
     * @return Reference to file path
     */
    std::string const& path() const;

    /**
     * @brief Writes data to the given file, using encryption if enabled
     * @param  data   Beginning of data to write
     * @param  length Length of data to write
     */
    void write(char const* data, size_t length);

    /**
     * @brief Reads data from the given file, using decryption if enabled
     * @param  buffer Buffer to store the read data
     * @param  length Maximum amount of data to read (no more than buffer
     *                length)
     */
    ssize_t read(char* buffer, size_t length);

    /**
     * @brief Read file contents into string
     * @return File contents
     */
    std::string slurp();

    /**
     * @brief Write a string to file
     * @param content String to write
     */
    void spit(std::string const& content);

    /**
     * @brief Closes file (now, as opposed to when the object is destroyed)
     * @return Reference to file status
     */
    Result const& close();

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
  /**
   * Open a new managed directory, handling en/decryption transparently.
   *
   * If the directory exists, the encryption type will be detected. If not, and
   * the directory is created, the encryption file will be written, enabling
   * encryption if the `EncryptionFeature` is enabled.
   *
   * Be sure to check the `status()` after construction to verify that the
   * directory was opened successfully. If `status().fail()`, the directory
   * cannot be used safely.
   *
   * @param path         The path to the directory
   * @param requireEmpty If `true`, opening a non-empty directory will fail
   * @param create       If `true` and directory does not exist, create it
   */
  ManagedDirectory(std::string const& path, bool requireEmpty, bool create);
  ~ManagedDirectory();

 public:
  /**
   * @brief Returns a reference to the status of the directory
   *
   * Status must be `.ok()` in order to open any new files.
   *
   * @return Reference to result object describing directory status
   */
  Result const& status() const;

  /**
   * @brief Resets the status to allow for further operation after error
   */
  void resetStatus();

  /**
   * @brief Returns the path to the directory under management
   * @return Path under management
   */
  std::string const& path() const;

  /**
   * @brief Build the fully-qualified file name
   * @param  filename File to retrieve path for
   * @return          Fully qualified filename
   */
  std::string pathToFile(std::string const& filename) const;

  /**
   * @brief Determines if encryption is enabled on the directory
   * @return `true` if directory is encrypted
   */
  bool isEncrypted() const;

  /**
   * @brief Returns the type of encryption used for the directory
   * @return The type of encryption used (may be "none")
   */
  std::string const& encryptionType() const;

#ifdef USE_ENTERPRISE
  /**
   * @brief Returns a pointer to the `EncryptionFeature` instance
   * @return A pointer to the feature
   */
  EncryptionFeature const* encryptionFeature() const;
#endif

  /**
   * @brief Opens a readable file
   * @param  filename The filename, relative to the directory
   * @param  flags    Flags (will be XORed with `DefaultReadFlags`
   * @return          Unique pointer to file, if opened
   */
  std::unique_ptr<File> readableFile(std::string const& filename, int flags = 0);

  /**
   * @brief Opens a writable file
   * @param  name      The filename, relative to the directory
   * @param  overwrite Whether to overwrite file if it exists (otherwise fail)
   * @param  flags     Flags (will be XORed with `DefaultWriteFlags`
   * @return           Unique pointer to file, if opened
   */
  std::unique_ptr<File> writableFile(std::string const& filename,
                                     bool overwrite, int flags = 0);

  /**
   * @brief Write a string to file
   * @param filename Name of file to write to
   * @param content  String to write to file
   */
  void spitFile(std::string const& filename, std::string const& content);

  /**
   * @brief Read file content into string
   * @param  filename File to read from
   * @return          Contents of file as string
   */
  std::string slurpFile(std::string const& filename);

  /**
   * @brief Read file content and parse into `VPackBuilder`
   * @param  filename File to read from
   * @return          Parsed vpack contents of file
   */
  VPackBuilder vpackFromJsonFile(std::string const& filename);

 private:
#ifdef USE_ENTERPRISE
  EncryptionFeature* const _encryptionFeature;
#endif
  std::string const _path;
  std::string _encryptionType;
  Result _status;
};
}  // namespace arangodb

#endif

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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "tri-zip.h"

#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/tri-strings.h"
#include "Zip/unzip.h"
#include "Zip/zip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "Zip/iowin32.h"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the current file
////////////////////////////////////////////////////////////////////////////////

static int ExtractCurrentFile(unzFile uf, void* buffer, size_t const bufferSize,
                              char const* outPath, bool const skipPaths,
                              bool const overwrite, char const* password,
                              std::string& errorMessage) {
  char filenameInZip[256];
  char* filenameWithoutPath;
  char* fullPath;
  char* p;
  unz_file_info64 fileInfo;
  long systemError;
  int err;

  // cppcheck-suppress *
  FILE* fout;

  filenameInZip[0] = '\0';
  err = unzGetCurrentFileInfo64(uf, &fileInfo, filenameInZip,
                                sizeof(filenameInZip), NULL, 0, NULL, 0);
  if (err != UNZ_OK) {
    errorMessage = std::string("Failed to get file info for ") + filenameInZip +
                   ": " + std::to_string(err);
    return TRI_ERROR_INTERNAL;
  }

  // adjust file name
  p = filenameInZip;
  while (*p != '\0') {
#ifdef WIN32
    if (*p == '/') {
      *p = '\\';
    }
#else
    if (*p == '\\') {
      *p = '/';
    }
#endif
    ++p;
  }

  p = filenameWithoutPath = filenameInZip;

  // get the file name without any path prefix
  while (*p != '\0') {
    if (*p == '/' || *p == '\\' || *p == ':') {
      filenameWithoutPath = p + 1;
    }

    p++;
  }

  // found a directory
  if (*filenameWithoutPath == '\0') {
    if (!skipPaths) {
      fullPath = TRI_Concatenate2File(outPath, filenameInZip);
      int res =
          TRI_CreateRecursiveDirectory(fullPath, systemError, errorMessage);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
        return res;
      }

      TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
    }
  }

  // found a file
  else {
    char const* writeFilename;

    if (!skipPaths) {
      writeFilename = filenameInZip;
    } else {
      writeFilename = filenameWithoutPath;
    }

    err = unzOpenCurrentFilePassword(uf, password);
    if (err != UNZ_OK) {
      errorMessage = "failed to authenticate the password in the zip: " +
                     std::to_string(err);
      return TRI_ERROR_INTERNAL;
    }

    // prefix the name from the zip file with the path specified
    fullPath = TRI_Concatenate2File(outPath, writeFilename);

    if (!overwrite && TRI_ExistsFile(fullPath)) {
      TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
      errorMessage = std::string("not allowed to overwrite file ") + fullPath;
      return TRI_ERROR_CANNOT_OVERWRITE_FILE;
    }

    // try to write the outfile
    fout = fopen(fullPath, "wb");

    // cannot write to outfile. this may be due to the target directory missing
    if (fout == nullptr && !skipPaths &&
        filenameWithoutPath != (char*)filenameInZip) {
      char c = *(filenameWithoutPath - 1);
      *(filenameWithoutPath - 1) = '\0';

      // create target directory recursively
      char* d = TRI_Concatenate2File(outPath, filenameInZip);
      int res = TRI_CreateRecursiveDirectory(d, systemError, errorMessage);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_Free(TRI_CORE_MEM_ZONE, d);
        return res;
      }

      TRI_Free(TRI_CORE_MEM_ZONE, d);

      *(filenameWithoutPath - 1) = c;

      // try again
      fout = fopen(fullPath, "wb");
    } else if (fout == nullptr) {
      // try to create the target directory recursively
      char* d = TRI_Concatenate2File(outPath, filenameInZip);
      // strip filename so we only have the directory name
      char* dir = TRI_Dirname(d);
      int res = TRI_CreateRecursiveDirectory(dir, systemError, errorMessage);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_Free(TRI_CORE_MEM_ZONE, d);
        TRI_Free(TRI_CORE_MEM_ZONE, dir);
        return res;
      }

      TRI_Free(TRI_CORE_MEM_ZONE, d);
      TRI_Free(TRI_CORE_MEM_ZONE, dir);
      // try again
      fout = fopen(fullPath, "wb");
    }

    if (fout == NULL) {
      errorMessage = std::string("failed to open file for writing: ") +
                     fullPath + " - " + strerror(errno);
      TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    while (true) {
      int result = unzReadCurrentFile(uf, buffer, (unsigned int)bufferSize);

      if (result < 0) {
        errorMessage = std::string("failed to write file ") + fullPath + " - " +
                       strerror(errno);
        fclose(fout);
        TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
        return TRI_ERROR_CANNOT_WRITE_FILE;
      }

      if (result > 0) {
        if (fwrite(buffer, result, 1, fout) != 1) {
          errorMessage = std::string("failed to write file ") + fullPath +
                         " - " + strerror(errno);
          TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
          fclose(fout);
          return TRI_set_errno(TRI_ERROR_SYS_ERROR);
        }
      } else {
        TRI_ASSERT(result == 0);
        break;
      }
    }
    TRI_Free(TRI_CORE_MEM_ZONE, fullPath);

    fclose(fout);
  }

  unzCloseCurrentFile(uf);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a single file
////////////////////////////////////////////////////////////////////////////////

static int UnzipFile(unzFile uf, void* buffer, size_t const bufferSize,
                     char const* outPath, bool const skipPaths,
                     bool const overwrite, char const* password,
                     std::string& errorMessage) {
  unz_global_info64 gi;
  uLong i;
  int res = TRI_ERROR_NO_ERROR;
  int err;

  err = unzGetGlobalInfo64(uf, &gi);
  if (err != UNZ_OK) {
    errorMessage = "Failed to get info: " + std::to_string(err);
    return TRI_ERROR_INTERNAL;
  }

  for (i = 0; i < gi.number_entry; i++) {
    res = ExtractCurrentFile(uf, buffer, bufferSize, outPath, skipPaths,
                             overwrite, password, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    if ((i + 1) < gi.number_entry) {
      err = unzGoToNextFile(uf);
      if (err == UNZ_END_OF_LIST_OF_FILE) {
        break;
      } else if (err != UNZ_OK) {
        errorMessage = "Failed to jump to next file: " + std::to_string(err);
        res = TRI_ERROR_INTERNAL;
        break;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief zips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_ZipFile(char const* filename, char const* dir,
                std::vector<std::string> const& files, char const* password) {
  void* buffer;
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
#endif

  if (TRI_ExistsFile(filename)) {
    return TRI_ERROR_CANNOT_OVERWRITE_FILE;
  }

  int bufferSize = 16384;
  buffer = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t)bufferSize, false);

  if (buffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

#ifdef USEWIN32IOAPI
  fill_win32_filefunc64A(&ffunc);
  zipFile zf = zipOpen2_64(filename, 0, NULL, &ffunc);
#else
  zipFile zf = zipOpen64(filename, 0);
#endif

  if (zf == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

    return ZIP_ERRNO;
  }

  int res = TRI_ERROR_NO_ERROR;

  size_t n = files.size();
  for (size_t i = 0; i < n; ++i) {
    std::string fullfile;

    if (*dir == '\0') {
      fullfile = files[i];
    } else {
      fullfile = arangodb::basics::FileUtils::buildFilename(dir, files[i]);
    }

    zip_fileinfo zi;
    memset(&zi, 0, sizeof(zi));

    uint32_t crc;
    res = TRI_Crc32File(fullfile.c_str(), &crc);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    int isLarge = (TRI_SizeFile(files[i].c_str()) > 0xFFFFFFFFLL);

    char const* saveName = files[i].c_str();

    while (*saveName == '\\' || *saveName == '/') {
      ++saveName;
    }

    if (zipOpenNewFileInZip3_64(
            zf, saveName, &zi, NULL, 0, NULL, 0, NULL, /* comment*/
            Z_DEFLATED, Z_DEFAULT_COMPRESSION, 0, -MAX_WBITS, DEF_MEM_LEVEL,
            Z_DEFAULT_STRATEGY, password, (unsigned long)crc,
            isLarge) != ZIP_OK) {
      res = TRI_ERROR_INTERNAL;
      break;
    }

    FILE* fin = fopen(fullfile.c_str(), "rb");

    if (fin == nullptr) {
      break;
    }

    while (true) {
      int sizeRead = (int)fread(buffer, 1, bufferSize, fin);
      if (sizeRead < bufferSize) {
        if (feof(fin) == 0) {
          res = TRI_set_errno(TRI_ERROR_SYS_ERROR);
          break;
        }
      }

      if (sizeRead > 0) {
        res = zipWriteInFileInZip(zf, buffer, sizeRead);
        if (res != 0) {
          break;
        }
      } else /* if (sizeRead <= 0) */ {
        break;
      }
    }

    fclose(fin);

    zipCloseFileInZip(zf);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }
  }

  zipClose(zf, NULL);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_UnzipFile(char const* filename, char const* outPath,
                  bool skipPaths, bool overwrite,
                  char const* password, std::string& errorMessage) {
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
#endif
  size_t bufferSize = 16384;
  void* buffer = (void*)TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

  if (buffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

#ifdef USEWIN32IOAPI
  fill_win32_filefunc64A(&ffunc);
  unzFile uf = unzOpen2_64(filename, &ffunc);
#else
  unzFile uf = unzOpen64(filename);
#endif
  if (uf == nullptr) {
    errorMessage = std::string("unable to open zip file ") + filename;
    return TRI_ERROR_INTERNAL;
  }

  int res = UnzipFile(uf, buffer, bufferSize, outPath, skipPaths, overwrite,
                      password, errorMessage);

  unzClose(uf);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

  return res;
}

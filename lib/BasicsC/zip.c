////////////////////////////////////////////////////////////////////////////////
/// @brief zip file functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "zip.h"

#include "BasicsC/files.h"
#include "BasicsC/tri-strings.h"
#include "Zip/unzip.h"
#include "Zip/zip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the current file
////////////////////////////////////////////////////////////////////////////////

static int ExtractCurrentFile (unzFile uf,
                               void* buffer,
                               const size_t bufferSize,
                               const char* outPath,
                               const bool skipPaths,
                               const bool overwrite,
                               const char* password) {
  char filenameInZip[256];
  char* filenameWithoutPath;
  char* fullPath;
  char* p;
  FILE *fout;
  unz_file_info64 fileInfo;

  if (unzGetCurrentFileInfo64(uf, &fileInfo, filenameInZip, sizeof(filenameInZip), NULL, 0, NULL, 0) != UNZ_OK) {
    return TRI_ERROR_INTERNAL;
  }

  p = filenameWithoutPath = filenameInZip;

  // get the file name without any path prefix
  while (*p != '\0') {
    if (*p == '/' || *p == '\\') {
      filenameWithoutPath = p + 1;
    }

    p++;
  }
  
  // found a directory
  if (*filenameWithoutPath == '\0') {
    if (! skipPaths) {
      fullPath = TRI_Concatenate2File(outPath, filenameInZip);
      TRI_CreateRecursiveDirectory(fullPath);
      TRI_Free(TRI_CORE_MEM_ZONE, fullPath);
    }
  }

  // found a file
  else {
    const char* writeFilename;

    if (! skipPaths) {
      writeFilename = filenameInZip;
    }
    else {
      writeFilename = filenameWithoutPath;
    }

    if (unzOpenCurrentFilePassword(uf, password) != UNZ_OK) {
      return TRI_ERROR_INTERNAL;
    }

    // prefix the name from the zip file with the path specified
    fullPath = TRI_Concatenate2File(outPath, writeFilename);
    
    if (! overwrite && TRI_ExistsFile(fullPath)) {
      return TRI_ERROR_INTERNAL;
    }

    // try to write the outfile
    fout = fopen(fullPath, "wb");

    // cannot write to outfile. this may be due to the target directory missing
    if (fout == NULL && ! skipPaths && filenameWithoutPath != (char*) filenameInZip) {
      char* d;

      char c = *(filenameWithoutPath - 1);
      *(filenameWithoutPath - 1) = '\0';

      // create target directory recursively
      d = TRI_Concatenate2File(outPath, filenameInZip);
      TRI_CreateRecursiveDirectory(d);
      TRI_Free(TRI_CORE_MEM_ZONE, d);

      *(filenameWithoutPath - 1) = c;

      // try again
      fout = fopen(fullPath, "wb");
    }
    
    TRI_Free(TRI_CORE_MEM_ZONE, fullPath);

    if (fout == NULL) {
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    while (true) {
      int result = unzReadCurrentFile(uf, buffer, bufferSize);

      if (result < 0) {
        fclose(fout);

        return TRI_ERROR_INTERNAL;
      }

      if (result > 0) {
        if (fwrite(buffer, result, 1, fout) != 1) {
          fclose(fout);

          return TRI_set_errno(TRI_ERROR_SYS_ERROR);
        }
      }
      else {
        assert(result == 0);
        break;
      }
    }

    fclose(fout);
  }

  unzCloseCurrentFile(uf);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a single file
////////////////////////////////////////////////////////////////////////////////

static int UnzipFile (unzFile uf,
                      void* buffer,
                      const size_t bufferSize,
                      const char* outPath, 
                      const bool skipPaths,
                      const bool overwrite,
                      const char* password) {
  unz_global_info64 gi;
  uLong i;
  int res;

  if (unzGetGlobalInfo64(uf, &gi) != UNZ_OK) {
    return TRI_ERROR_INTERNAL;
  }
    
  for (i = 0; i < gi.number_entry; i++) {
    res = ExtractCurrentFile(uf, buffer, bufferSize, outPath, skipPaths, overwrite, password);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    if ((i + 1) < gi.number_entry) {
      if (unzGoToNextFile(uf) != UNZ_OK) {
        res = TRI_ERROR_INTERNAL;
        break;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief zips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_ZipFile (const char* filename, 
                 const char* dir,
                 TRI_vector_string_t const* files,
                 const char* password) {
  void* buffer;
  size_t bufferSize;
  zipFile zf;
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
#endif
  size_t i, n;
  int res;

  if (TRI_ExistsFile(filename)) {
    return TRI_ERROR_CANNOT_OVERWRITE_FILE;
  }

  bufferSize = 16384;
  buffer = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

  if (buffer == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

#ifdef USEWIN32IOAPI
  fill_win32_filefunc64A(&ffunc);
  zf = zipOpen2_64(filename, 0, NULL, &ffunc);
#else
  zf = zipOpen64(filename, 0);
#endif

  if (zf == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

    return ZIP_ERRNO;
  }

  res = TRI_ERROR_NO_ERROR;

  n = files->_length;
  for (i = 0; i < n; ++i) {
    FILE* fin;
    char* file;
    char* fullfile;
    char* saveName;
    zip_fileinfo zi;
    uint32_t crc;
    int isLarge;

    file = TRI_AtVectorString(files, i);

    if (*dir == '\0') {
      fullfile = TRI_DuplicateString(file);
    }
    else {
      fullfile = TRI_Concatenate2File(dir, file);
    }

    memset(&zi, 0, sizeof(zi));

    res = TRI_Crc32File(fullfile, &crc);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }

    isLarge = (TRI_SizeFile(file) > 0xFFFFFFFFLL);
              
    saveName = file;

    while (*saveName == '\\' || *saveName == '/') {
      ++saveName;
    }

    if (zipOpenNewFileInZip3_64(zf,
                                saveName,
                                &zi,
                                NULL,
                                0,
                                NULL,
                                0,
                                NULL, /* comment*/
                                Z_DEFLATED,
                                Z_DEFAULT_COMPRESSION,
                                0,
                                -MAX_WBITS, 
                                DEF_MEM_LEVEL, 
                                Z_DEFAULT_STRATEGY,
                                password,
                                (unsigned long) crc, 
                                isLarge) != ZIP_OK) {
    }

    fin = fopen(fullfile, "rb");
    TRI_FreeString(TRI_CORE_MEM_ZONE, fullfile);

    if (fin == NULL) {
      break;
    }

    while (true) {
      int sizeRead;

      sizeRead = (int) fread(buffer, 1, bufferSize, fin);
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
      }
      else if (sizeRead <= 0) {
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

int TRI_UnzipFile (const char* filename,
                   const char* outPath, 
                   const bool skipPaths,
                   const bool overwrite,
                   const char* password) {
  unzFile uf;
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
#endif
  void* buffer;
  size_t bufferSize;
  int res;
  
  bufferSize = 16384;
  buffer = (void*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

  if (buffer == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }


#ifdef USEWIN32IOAPI
  fill_win32_filefunc64A(&ffunc);
  uf = unzOpen2_64(filename, &ffunc);
#else
  uf = unzOpen64(filename);
#endif

  res = UnzipFile(uf, buffer, bufferSize, outPath, skipPaths, overwrite, password);

  unzClose(uf);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

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

#include "files.h"

#include "unzip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
////////////////////////////////////////////////////////////////////////////////

int do_extract_currentfile (unzFile uf,
                            const bool withoutPath,
                            const bool overwrite,
                            const char* password) {
  char filenameInZip[256];
  char* filenameWithoutPath;
  char* p;
  int err = UNZ_OK;
  FILE *fout=NULL;
  void* buf;
  size_t bufferSize;
  unz_file_info64 fileInfo;

  err = unzGetCurrentFileInfo64(uf, &fileInfo, filenameInZip, sizeof(filenameInZip), NULL, 0, NULL, 0);

  if (err != UNZ_OK) {
    return err;
  }

  bufferSize = 16384;
  buf = (void*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

  if (buf == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  p = filenameWithoutPath = filenameInZip;
  while (*p != '\0') {
    if (*p == '/' || *p == '\\') {
      filenameWithoutPath = p + 1;
    }
    p++;
  }

  if (*filenameWithoutPath == '\0') {
    if (! withoutPath) {
      if (! TRI_CreateDirectory(filenameInZip)) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);

        return TRI_ERROR_INTERNAL;
      }
    }
  }
  else {
    const char* writeFilename;

    if (! withoutPath) {
      writeFilename = filenameInZip;
    }
    else {
      writeFilename = filenameWithoutPath;
    }

    err = unzOpenCurrentFilePassword(uf, password);
    if (err != UNZ_OK) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);

      return TRI_ERROR_INTERNAL;
    }

    if (! overwrite && TRI_ExistsFile(writeFilename)) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);

      return TRI_ERROR_INTERNAL;
    }

    fout = fopen(writeFilename,"wb");
      /* some zipfile don't contain directory alone before file */
    if (fout == NULL && ! withoutPath && filenameWithoutPath != (char*) filenameInZip) {
      char c = *(filenameWithoutPath - 1);
      *(filenameWithoutPath - 1)='\0';
      TRI_CreateRecursiveDirectory(writeFilename);
      *(filenameWithoutPath - 1)=c;
      fout = fopen(writeFilename,"wb");
    }

    if (fout==NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);

      return TRI_ERROR_INTERNAL;
    }

    do {
      err = unzReadCurrentFile(uf, buf, bufferSize);
      if (err<0) {
        break;
      }
      if (err>0) {
        if (fwrite(buf, err, 1, fout) != 1) {
          err = UNZ_ERRNO;
          break;
        }
      }
    }
    while (err > 0);

    fclose(fout);
/*
      if (err==0)
        change_file_date(write_filename,file_info.dosDate,
            file_info.tmu_date);
        */
  }

  unzCloseCurrentFile(uf);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);
  
  return err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a single file
////////////////////////////////////////////////////////////////////////////////

static int UnzipFile (unzFile uf, 
                      const bool withoutPath,
                      const bool overwrite,
                      const char* password) {
  unz_global_info64 gi;
  uLong i;
  int res;

  if (unzGetGlobalInfo64(uf, &gi) != UNZ_OK) {
    return TRI_ERROR_INTERNAL;
  }
    
  for (i=0; i < gi.number_entry; i++) {
    if (do_extract_currentfile(uf, withoutPath, overwrite, password) != UNZ_OK) {
      res = TRI_ERROR_INTERNAL;
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
/// @brief unzips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_UnzipFile (const char* filename, 
                   const bool withoutPath,
                   const bool overwrite,
                   const char* password) {
  unzFile uf;
#ifdef USEWIN32IOAPI
  zlib_filefunc64_def ffunc;
#endif
  int res;

#ifdef USEWIN32IOAPI
  fill_win32_filefunc64A(&ffunc);
  uf = unzOpen2_64(filename ,&ffunc);
#else
  uf = unzOpen64(filename);
#endif
  res = UnzipFile(uf, withoutPath, overwrite, password);

  unzClose(uf);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief file utilities
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
/// @author Dr. Frank Celler
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "FileUtils.h"

#include <errno.h>
#include <fstream>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "BasicsC/files.h"
#include "BasicsC/tri-strings.h"

namespace triagens {
  namespace basics {
    namespace FileUtils {
      ifstream * createInput (string const& filename) {
        ifstream * s = new ifstream(filename.c_str());

        if (!*s) {
          delete s;
          return 0;
        }

        return s;
      }



      ofstream * createOutput (string const& filename) {
        ofstream * s = new ofstream(filename.c_str());

        if (!*s) {
          delete s;
          return 0;
        }

        return s;
      }



      string slurp (string const& filename) {
        int fd = TRI_OPEN(filename.c_str(), O_RDONLY);

        if (fd == -1) {
          THROW_FILE_OPEN_ERROR("open", filename, "O_RDONLY", errno);
        }

        char buffer[10240];
        StringBuffer result(TRI_CORE_MEM_ZONE);

        while (true) {
          ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

          if (n == 0) {
            break;
          }

          if (n < 0) {
            TRI_set_errno(TRI_ERROR_SYS_ERROR);

            LOGGER_TRACE("read failed for '" << filename
                         << "' with " << strerror(errno) << " and result " << n
                         << " on fd " << fd);

            TRI_CLOSE(fd);
            THROW_FILE_FUNC_ERROR("read", "", errno);
          }

          result.appendText(buffer, n);
        }

        TRI_CLOSE(fd);

        string r(result.c_str(), result.length());

        return r;
      }



      void slurp (string const& filename, StringBuffer& result) {
        int fd = TRI_OPEN(filename.c_str(), O_RDONLY);

        if (fd == -1) {
          THROW_FILE_OPEN_ERROR("open", filename, "O_RDONLY", errno);
        }

        char buffer[10240];

        while (true) {
          ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

          if (n == 0) {
            break;
          }

          if (n < 0) {
            TRI_CLOSE(fd);
            LOGGER_TRACE("read failed for '" << filename
                         << "' with " << strerror(errno) << " and result " << n
                         << " on fd " << fd);
            THROW_FILE_FUNC_ERROR("read", "", errno);
          }

          result.appendText(buffer, n);
        }

        TRI_CLOSE(fd);
      }

      
      void spit (string const& filename, const char* ptr, size_t len) {
        int fd = TRI_CREATE(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);

        if (fd == -1) {
          LOGGER_TRACE("open failed for '" << filename << "' with " << strerror(errno));
          THROW_FILE_OPEN_ERROR("open", filename, "O_RDONLY | O_CREAT | O_TRUNC", errno);
        }

        while (0 < len) {
          ssize_t n = TRI_WRITE(fd, ptr, len);

          if (n < 1) {
            TRI_CLOSE(fd);
            LOGGER_TRACE("write failed for '" << filename
                         << "' with " << strerror(errno) << " and result " << n
                         << " on fd " << fd);
            THROW_FILE_FUNC_ERROR("write", "", errno);
          }

          ptr += n;
          len -= n;
        }

        TRI_CLOSE(fd);
        return;
      }


      void spit (string const& filename, string const& content) {
        int fd = TRI_CREATE(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);

        if (fd == -1) {
          LOGGER_TRACE("open failed for '" << filename << "' with " << strerror(errno));
          THROW_FILE_OPEN_ERROR("open", filename, "O_RDONLY | O_CREAT | O_TRUNC", errno);
        }

        char const* ptr = content.c_str();
        size_t len = content.size();

        while (0 < len) {
          ssize_t n = TRI_WRITE(fd, ptr, len);

          if (n < 1) {
            TRI_CLOSE(fd);
            LOGGER_TRACE("write failed for '" << filename
                         << "' with " << strerror(errno) << " and result " << n
                         << " on fd " << fd);
            THROW_FILE_FUNC_ERROR("write", "", errno);
          }

          ptr += n;
          len -= n;
        }

        TRI_CLOSE(fd);
        return;
      }



      void spit (string const& filename, StringBuffer const& content) {
        int fd = TRI_CREATE(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);

        if (fd == -1) {
          LOGGER_TRACE("open failed for '" << filename << "' with " << strerror(errno));
          THROW_FILE_OPEN_ERROR("open", filename, "O_WRONLY | O_CREAT | O_TRUNC", errno);
        }

        char const* ptr = content.c_str();
        size_t len = content.length();

        while (0 < len) {
          ssize_t n = TRI_WRITE(fd, ptr, len);

          if (n < 1) {
            TRI_CLOSE(fd);
            LOGGER_TRACE("write failed for '" << filename
                         << "' with " << strerror(errno) << " and result " << n
                         << " on fd " << fd);
            THROW_FILE_FUNC_ERROR("write", "", errno);
          }

          ptr += n;
          len -= n;
        }

        TRI_CLOSE(fd);
        return;
      }



      bool remove (string const& fileName, int* errorNumber) {
        if (errorNumber != 0) {
          *errorNumber = 0;
        }

        int result = std::remove(fileName.c_str());

        if (errorNumber != 0) {
          *errorNumber = errno;
        }

        return (result != 0) ? false : true;
      }



      bool rename (string const& oldName, string const& newName, int* errorNumber) {
        if (errorNumber != 0) {
          *errorNumber = 0;
        }

        int result = std::rename(oldName.c_str(), newName.c_str());

        if (errorNumber) {
          *errorNumber = errno;
        }

        return (result != 0) ? false : true;
      }



      bool createDirectory (string const& name, int* errorNumber) {
        if (errorNumber != 0) {
          *errorNumber = 0;
        }

        return createDirectory(name, 0777, errorNumber);
      }



      bool createDirectory (string const& name, int mask, int* errorNumber) {
        if (errorNumber != 0) {
          *errorNumber = 0;
        }

        int result = TRI_MKDIR(name.c_str(), mask);

        if (result != 0 && errno == EEXIST && isDirectory(name)) {
          result = 0;
        }

        if (errorNumber) {
          *errorNumber = errno;
        }

        return (result != 0) ? false : true;
      }



      vector<string> listFiles (string const& directory) {
        vector < string > result;

#ifdef TRI_HAVE_WIN32_LIST_FILES

        struct _finddata_t fd;
        intptr_t handle;

        string filter = directory + "\\*";
        handle = _findfirst(filter.c_str(), &fd);

        if (handle == -1) {
          return result;
        }

        do {
          if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
            result.push_back(fd.name);
          }
        } while(_findnext(handle, &fd) != -1);

        _findclose(handle);

#else

        DIR * d = opendir(directory.c_str());

        if (d == 0) {
          return result;
        }

        dirent * de = readdir(d);

        while (de != 0) {
          if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
            result.push_back(de->d_name);
          }

          de = readdir(d);
        }

        closedir(d);

#endif

        return result;
      }



      bool isDirectory (string const& path) {
        struct stat stbuf;
        int res = stat(path.c_str(), &stbuf);

        return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFDIR);
      }



      bool isSymbolicLink (string const& path) {

#ifdef TRI_HAVE_WIN32_SYMBOLIC_LINK

        // .........................................................................
        // TODO: On the NTFS file system, there are the following file links:
        // hard links -
        // junctions -
        // symbolic links -
        // .........................................................................
        return false;

#else

        struct stat stbuf;
        int res = stat(path.c_str(), &stbuf);

        return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFLNK);

#endif
      }



      bool isRegularFile (string const& path) {
        struct stat stbuf;
        int res = stat(path.c_str(), &stbuf);

        return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFREG);
      }



      bool exists (string const& path) {
        struct stat stbuf;
        int res = stat(path.c_str(), &stbuf);

        return res == 0;
      }


      off_t size (string const& path) {
        int64_t result = TRI_SizeFile(path.c_str());

        if (result < 0) {
          return (off_t) 0;
        }

        return (off_t) result;
      }


      string stripExtension (string const& path, string const& extension) {
        size_t pos = path.rfind(extension);
        if (pos == string::npos) {
          return path;
        }

        string last = path.substr(pos);
        if (last == extension) {
          return path.substr(0, pos);
        }

        return path;
      }



      bool changeDirectory (string const& path) {
        return TRI_CHDIR(path.c_str()) == 0;
      }



      string currentDirectory (int* errorNumber) {
        if (errorNumber != 0) {
          *errorNumber = 0;
        }

        size_t len = 1000;
        char* current = new char[len];

        while (TRI_GETCWD(current, len) == NULL) {
          if (errno == ERANGE) {
            len += 1000;
            delete[] current;
            current = new char[len];
          }
          else {
            delete[] current;

            if (errorNumber != 0) {
              *errorNumber = errno;
            }

            return "";
          }
        }

        string result = current;

        delete[] current;

        return result;
      }


      string homeDirectory () {
        char* dir = TRI_HomeDirectory();
        string result = dir;
        TRI_FreeString(TRI_CORE_MEM_ZONE, dir);

        return result;
      }
    }
  }
}


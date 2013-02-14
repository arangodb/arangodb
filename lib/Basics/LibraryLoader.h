////////////////////////////////////////////////////////////////////////////////
/// @brief library loader
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_LIBRARY_LOADER_H
#define TRIAGENS_BASICS_LIBRARY_LOADER_H 1

#include "Basics/Common.h"

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief library loader
////////////////////////////////////////////////////////////////////////////////

    namespace LibraryLoader {

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief processes a directory or file
////////////////////////////////////////////////////////////////////////////////

      template<typename F>
      void process (string const& pathname, string const& symbol, F& obj) {
        if (basics::FileUtils::isDirectory(pathname)) {
          vector<string> files = basics::FileUtils::listFiles(pathname);

          for (vector<string>::iterator i = files.begin();  i != files.end();  ++i) {
            string const& f = *i;

            processFile(pathname + "/" + f, symbol, obj);
          }
        }
        else {
          processFile(pathname, symbol, obj);
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief processes a directory
////////////////////////////////////////////////////////////////////////////////

      template<typename F>
      void processDirectory (string const& pathname, string const& symbol, F& obj) {
        if (! basics::FileUtils::isDirectory(pathname)) {
          LOGGER_ERROR("database directory '" << pathname << "' is no directory");
          return;
        }

        vector<string> files = basics::FileUtils::listFiles(pathname);

        for (vector<string>::iterator i = files.begin();  i != files.end();  ++i) {
          string const& f = *i;

          processFile(f, symbol, obj);
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief processes self
////////////////////////////////////////////////////////////////////////////////

      template<typename F>
      void processSelf (string const& symbol, F& obj) {
        processPrivate(0, symbol, obj);
      }

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief processes a file
////////////////////////////////////////////////////////////////////////////////

      template<typename F>
      void processFile (string const& filename, string const& symbol, F& obj) {
        if (basics::FileUtils::isDirectory(filename)) {
          LOGGER_DEBUG("skipping directory '" << filename << "'");
        }
        else {
          if (filename.empty()) {
            return;
          }

          if (filename.size() < 3 || filename.substr(filename.size() - 3) != ".so") {
            LOGGER_DEBUG("skipping non .so file '" << filename << "'");
          }
          else {
            processPrivate(filename.c_str(), symbol, obj);
          }
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief closes a shared library
////////////////////////////////////////////////////////////////////////////////

      void closeLibrary (void* handle);

// -----------------------------------------------------------------------------
      // private functions
// -----------------------------------------------------------------------------

      void* loadSharedLibrary (char const* filename, string const& symbol, void*& handle);

      template<typename F>
      void processPrivate (char const* filename, string const& symbol, F& obj) {
        void* handle;
        void* init = loadSharedLibrary(filename, symbol, handle);

        if (init != 0) {
          string f = (filename ? filename : "--self--");

          if (! obj.processHandle(f, init, handle)) {
            closeLibrary(handle);
          }
        }
      }
    }
  }
}

#endif

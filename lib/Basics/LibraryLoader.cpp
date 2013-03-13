////////////////////////////////////////////////////////////////////////////////
/// @brief library loader
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "LibraryLoader.h"

#ifdef TRI_HAVE_DLFCN_H
#include <dlfcn.h>
#endif

using namespace triagens::basics;

namespace triagens {
  namespace rest {
    namespace LibraryLoader {

// -----------------------------------------------------------------------------
      // private functions
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_DLFCN_H

      void* loadSharedLibrary (char const* filename, string const& symbol, void*& handle) {
        LOGGER_DEBUG("trying to use library file '" << (filename ? filename : "self") << "'");

        // open library
        handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

        if (handle == 0) {
          LOGGER_DEBUG("cannot open library file '" << (filename ? filename : "self") << "'");
          LOGGER_DEBUG("dlerror: " << dlerror());
          return 0;
        }

        // look for init function
        void* init = dlsym(handle, symbol.c_str());

        if (init == 0) {
          LOGGER_DEBUG("cannot find '" << symbol << "' in '" << (filename ? filename : "self") << "'");
          dlclose(handle);
          return 0;
        }

        return init;
      }



      void closeLibrary (void* handle) {
        dlclose(handle);
      }

#else

      void* loadShardLibrary(char const* filename, string const& symbol, void*& handle) {
        // ...........................................................................
        // TODO: implement load for windows
        // ...........................................................................
        assert(0);
        return 0;
      }

#endif

    }
  }
}

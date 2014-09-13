////////////////////////////////////////////////////////////////////////////////
/// @brief source code loader
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "MRLoader.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "MRuby/mr-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a loader
////////////////////////////////////////////////////////////////////////////////

MRLoader::MRLoader () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

bool MRLoader::loadScript (mrb_state* mrb, string const& name) {
  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    LOG_ERROR("unknown script '%s'", name.c_str());
    return false;
  }

  bool ok = TRI_ExecuteRubyString(mrb, i->second.c_str(), name.c_str(), false, NULL);

  if (! ok) {
    TRI_LogRubyException(mrb, mrb->exc);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads all scripts
////////////////////////////////////////////////////////////////////////////////

bool MRLoader::loadAllScripts (mrb_state* mrb) {
  if (_directory.empty()) {
    return true;
  }

  vector<string> parts = getDirectoryParts();

  bool result = true;

  for (size_t i = 0; i < parts.size(); i++) {
    result &= TRI_ExecuteRubyDirectory(mrb, parts.at(i).c_str());
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a named script
////////////////////////////////////////////////////////////////////////////////

bool MRLoader::executeScript (mrb_state* mrb, string const& name) {
  findScript(name);

  map<string, string>::iterator i = _scripts.find(name);

  if (i == _scripts.end()) {
    return false;
  }

  string content = "(function() { " + i->second + "/* end-of-file '" + name + "' */ })()";

  bool ok = TRI_ExecuteRubyString(mrb, content.c_str(), name.c_str(), false, NULL);

  if (! ok) {
    TRI_LogRubyException(mrb, mrb->exc);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all scripts
////////////////////////////////////////////////////////////////////////////////

bool MRLoader::executeAllScripts (mrb_state* mrb) {
  if (_directory.empty()) {
    return true;
  }

  return TRI_ExecuteRubyDirectory(mrb, _directory.c_str());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

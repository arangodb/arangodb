////////////////////////////////////////////////////////////////////////////////
/// @brief abstract actions
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
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ACTIONS_ACTIONS_H
#define ARANGODB_ACTIONS_ACTIONS_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_vocbase_s;

namespace triagens {
  namespace rest {
    class HttpResponse;
    class HttpRequest;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief action result
////////////////////////////////////////////////////////////////////////////////

class TRI_action_result_t {
  public:
    TRI_action_result_t ()
      : isValid(false), 
        requeue(false), 
        canceled(false), 
        response(nullptr), 
        sleep(0.0) {
    }

    // Please be careful here: In the beginning we had "bool requeue" after
    // the response pointer in this struct. However, this triggered a nasty
    // compiler bug in Visual Studio Express 2013 which lead to the fact
    // that sometimes requeue was involuntarily flipped to "true" during
    // a return of a TRI_action_result_t from a function call.
    // In this order it seems to work.
    // Details: v8-actions.cpp: v8_action_t::execute returns a TRI_action_result_t
    // to RestActionHandler::executeAction and suddenly requeue is true.

    bool isValid;
    bool requeue;
    bool canceled;

    triagens::rest::HttpResponse* response;

    double sleep;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief action descriptor
////////////////////////////////////////////////////////////////////////////////

class TRI_action_t {
  public:
    TRI_action_t ()
      : _type(),
        _url(),
        _urlParts(0),
        _isPrefix(false),
        _allowUseDatabase(false) {
    }

    virtual ~TRI_action_t () {}

    virtual TRI_action_result_t execute (struct TRI_vocbase_s*,
                                         triagens::rest::HttpRequest*,
                                         triagens::basics::Mutex* dataLock,
                                         void** data) = 0;

    virtual bool cancel (triagens::basics::Mutex* dataLock,
                         void** data) = 0;

    std::string _type;
    std::string _url;

    size_t _urlParts;

    bool _isPrefix;
    bool _allowUseDatabase;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_DefineActionVocBase (std::string const& name, TRI_action_t* action);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_LookupActionVocBase (triagens::rest::HttpRequest* request);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes all defined actions
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupActions ();

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief simple client
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
/// @author Achim Brandt
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SimpleClient.h"

#include <stdio.h>
#include <string>
#include <errno.h>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#include "GeneralClientConnection.h"
#include "SimpleHttpResult.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

namespace triagens {
  namespace httpclient {

// -----------------------------------------------------------------------------
    // constructors and destructors
// -----------------------------------------------------------------------------

    SimpleClient::SimpleClient (GeneralClientConnection* connection, double requestTimeout, bool warn) :
      _connection(connection),
      _writeBuffer(TRI_UNKNOWN_MEM_ZONE),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE),
      _requestTimeout(requestTimeout),
      _warn(warn) {

      _errorMessage = "";
      _written = 0;
      _state = IN_CONNECT;

      reset();
    }

    SimpleClient::~SimpleClient () {
      _connection->disconnect();
    }

// -----------------------------------------------------------------------------
    // protected methods
// -----------------------------------------------------------------------------

    void SimpleClient::handleConnect () {
      if (! _connection->connect()) {
        setErrorMessage("Could not connect to '" +  _connection->getEndpoint()->getSpecification() + "'", errno);
        _state = DEAD;
      }
      else {
        // can write now
        _state = IN_WRITE;
        _written = 0;
      }
    }

    bool SimpleClient::close () {
      _connection->disconnect();
      _state = IN_CONNECT;

      reset();

      return true;
    }

    void SimpleClient::reset () {
      _readBuffer.clear();
    }

    double SimpleClient::now () {
      struct timeval tv;
      gettimeofday(&tv, 0);

      double sec = tv.tv_sec; // seconds
      double usc = tv.tv_usec; // microseconds

      return sec + usc / 1000000.0;
    }

  }

}

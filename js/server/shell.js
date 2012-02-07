////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                             PRINT
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global variable holding the current printed query
////////////////////////////////////////////////////////////////////////////////

var it = undefined;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of results to print
////////////////////////////////////////////////////////////////////////////////

var queryLimit = 20;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery.prototype.PRINT = function() {
  if (this instanceof AvocadoFluentQuery) {
    var count = 0;

    try {
      while (this.hasNext() && count++ < queryLimit) {
        internal.output(JSON.stringify(this.next()), "\n");
      }

      if (this.hasNext()) {
        internal.output("...more results...");
      }

      it = this;
    }
    catch (e) {
      internal.output("encountered error while printing: " + e);
    }
  }
  else {
    internal.output(this.toString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.PRINT = function() {
  if (this instanceof AvocadoFluentQuery2) {
    var count = 0;

    try {
      while (this.hasNext() && count++ < queryLimit) {
        internal.output(JSON.stringify(this.next()), "\n");
      }

      if (this.hasNext()) {
        internal.output("...more results...");
      }

      it = this;
    }
    catch (e) {
      internal.output("encountered error while printing: " + e);
    }
  }
  else {
    internal.output(this.toString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a query
////////////////////////////////////////////////////////////////////////////////

AvocadoCursor.prototype.PRINT = function() {
  if (this instanceof AvocadoCursor) {
    var count = 0;

    try {
      while (this.hasNext() && count++ < queryLimit) {
        internal.output(JSON.stringify(this.next()), "\n");
      }

      if (this.hasNext()) {
        internal.output("...more results...");
      }

      it = this;
    }
    catch (e) {
      internal.output("encountered error while printing: " + e);
    }
  }
  else {
    internal.output(this.toString());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

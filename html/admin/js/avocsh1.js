////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB web browser shell
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief starts pager (dummy)
////////////////////////////////////////////////////////////////////////////////

var SYS_START_PAGER = function() {};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops pager (dummy)
////////////////////////////////////////////////////////////////////////////////

var SYS_STOP_PAGER = function() {};

////////////////////////////////////////////////////////////////////////////////
/// @brief escapes HTML
////////////////////////////////////////////////////////////////////////////////

var escapeHTML = (function() {
                    var MAP = {
                      '&': '&amp;',
                      '<': '&lt;',
                      '>': '&gt;',
                      '"': '&#34;',
                      "'": '&#39;',
                      "\n": '<br>',
                      " ": '&nbsp;'
                    };

                    var repl = function(c) { return MAP[c]; };

                    return function(s) {
                      return s.replace(/[&<>'"\n ]/g, repl); //'
                    };
                  })(); 

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

var TRI_SYS_OUTPUT = function (value) {
  var result = "";

  if (value === null) {
    result = "null";
  }
  else if (typeof(value) == "string") {
    result = value;
  }
  else if (typeof(value) == "number" || typeof(value) == "boolean") {
    result = "" + value;
  }
  else if (typeof(value) == "object") {
    try {
      result = JSON.stringify(value);
    }
    catch (err) {
      result = value.toString();
    }
  }

  $('#avocshWindow').append('<p class="avocshSuccess">' + escapeHTML(result) + '</p>'); 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

var print = TRI_SYS_OUTPUT;

////////////////////////////////////////////////////////////////////////////////
/// @brief arango server connection
////////////////////////////////////////////////////////////////////////////////

function ArangoConnection () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a get request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.get = function (url) {
var msg; 
  $.ajax({
    async: false, 
    type: "GET",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data; 
    }
  });
  return msg;  
};

ArangoConnection.prototype.GET = ArangoConnection.prototype.get;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a delete request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.delete = function (url) {
var msg; 
  $.ajax({
    async: false, 
    type: "DELETE",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};


ArangoConnection.prototype.DELETE = ArangoConnection.prototype.delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a post request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.post = function (url, body) {
var msg;
  $.ajax({
    async: false, 
    type: "POST",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data; //JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};


ArangoConnection.prototype.POST = ArangoConnection.prototype.post;

////////////////////////////////////////////////////////////////////////////////
/// @brief executs a put request
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.put = function (url, body) {
var msg; 
  $.ajax({
    async: false, 
    type: "PUT",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
return data;
      msg = JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};

ArangoConnection.prototype.PUT = ArangoConnection.prototype.put;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

var ModuleCache = { "/internal" : { "exports": { } } };

////////////////////////////////////////////////////////////////////////////////
/// @brief connection
////////////////////////////////////////////////////////////////////////////////

var arango = new ArangoConnection();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

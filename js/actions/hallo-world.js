////////////////////////////////////////////////////////////////////////////////
/// @brief demo actions
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("actions");

////////////////////////////////////////////////////////////////////////////////
/// @brief Hallo World Demo
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "hallo-world",
  domain : "user",

  callback : 
    function (req, res) {
      var text;

      res.responseCode = 200;
      res.contentType = "text/html";

      text = "<h1>Hallo World</h1>\n"
           + "\n";

      text += "<ul>\n"
           +  "<li>request type: " + req.requestType + "</li>\n"
           +  "</ul>\n";

      text += "<h2>Parameters</h1>\n"
           +  "<ul>\n";

      for (var i in req.parameters) {
        if (req.parameters.hasOwnProperty(i)) {
          text += "<li>" + i + " : " + req.parameters[i] + "</li>\n";
        }
      }

      text += "</ul>\n";

      if (req.suffix && 0 < req.suffix.length) {
        text += "<h2>Suffices</h2>\n"
             + "<ol>\n";

        for (var i = 0;  i < req.suffix.length;  ++i) {
          text += "<li>" + req.suffix[i] + "</li>\n";
        }

        text += "</ol>\n\n";
      }

      if (req.headers) {
        text += "<h2>Headers</h2>\n"
             +  "<ul>\n";

        for (var i in req.headers) {
          if (req.headers.hasOwnProperty(i)) {
            text += "<li>" + i + " : " + req.headers[i] + "</li>\n";
          }
        }

        text += "</ul>\n\n";

        res.body = text;
      }
    }
});

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

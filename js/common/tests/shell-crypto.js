////////////////////////////////////////////////////////////////////////////////
/// @brief test the crypto interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var crypto = require("org/arangodb/crypto");

// -----------------------------------------------------------------------------
// --SECTION--                                                    crypto methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function CryptoSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test md5, invalid values
////////////////////////////////////////////////////////////////////////////////

    testMd5Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.md5(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test md5
////////////////////////////////////////////////////////////////////////////////

    testMd5 : function () {
      var data = [
        [ "", "d41d8cd98f00b204e9800998ecf8427e" ],
        [ " ", "7215ee9c7d9dc229d2921a40e899ec5f" ],
        [ "arangodb", "335889e69e03cefd041babef1b02a0b8" ],
        [ "Arangodb", "0862bc79ec789143f75e3282df98c8f4" ],
        [ "ArangoDB is a database", "b88ddc26cfa3a652fdd8bf8e8c069540" ]
      ];
      
      data.forEach(function (value) {
        assertEqual(value[1], crypto.md5(value[0]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha256, invalid values
////////////////////////////////////////////////////////////////////////////////

    testSha256Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.sha256(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha256
////////////////////////////////////////////////////////////////////////////////

    testSha256 : function () {
      var data = [
        [ "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ],
        [ " ", "36a9e7f1c95b82ffb99743e0c5c4ce95d83c9a430aac59f84ef3cbfab6145068"],
        [ "arangodb", "d0a274654772fa104df32ff457ff0a432f2dfad18e7c3146fab3c807f2ed86e5" ],
        [ "Arangodb", "38579e73fd7435de7db93028a1b340be77445b46c94dff013c05c696ccae259c" ],
        [ "ArangoDB is a database", "00231e8f9c0a617426ae51e4e230a1b25f6d5b82c10fccc835b514142f235f31" ]
      ];
      
      data.forEach(function (value) {
        assertEqual(value[1], crypto.sha256(value[0]));
      });
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test random
////////////////////////////////////////////////////////////////////////////////

    testRandom : function () {
      var i;

      for (i = 0; i < 100; ++i) {
        assertTrue(crypto.rand() != 0);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CryptoSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:


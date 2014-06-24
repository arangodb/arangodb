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
/// @brief test sha224, invalid values
////////////////////////////////////////////////////////////////////////////////

    testSha224Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.sha224(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha224
////////////////////////////////////////////////////////////////////////////////

    testSha224 : function () {
      var data = [
        [ "", "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" ],
        [ " ", "ca17734c016e36b898af29c1aeb142e774abf4b70bac55ec98a27ba8"],
        [ "arangodb", "deeb6d8f9b6c316e7f5a601fb2549a2ebc857bee78df38b1977c9989" ],
        [ "Arangodb", "6d7a8a7fb22537dab437c8c2915874a170b2b14eb1aa787df32d6999" ],
        [ "ArangoDB is a database", "9a3e02d47eb686c67f6b9a51efe16e8b4f88b0ee14248636d6163f1d" ]
      ];

      data.forEach(function (value) {
        assertEqual(value[1], crypto.sha224(value[0]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha1, invalid values
////////////////////////////////////////////////////////////////////////////////

    testSha1Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.sha1(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha1
////////////////////////////////////////////////////////////////////////////////

    testSha1 : function () {
      var data = [
        [ "", "da39a3ee5e6b4b0d3255bfef95601890afd80709" ],
        [ " ", "b858cb282617fb0956d960215c8e84d1ccf909c6"],
        [ "arangodb", "1f9aa6577198dd5fcab487d08d45258608dac9b5" ],
        [ "Arangodb", "5fc1b451c5cd4770df14bd3ae362b5587a195311" ],
        [ "ArangoDB is a database", "9e45475b50ea3e8438c55919238aa5b0736bda43" ]
      ];

      data.forEach(function (value) {
        assertEqual(value[1], crypto.sha1(value[0]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hmac, invalid values
////////////////////////////////////////////////////////////////////////////////

    testHmacInvalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value1, i, arr) {
        arr.forEach(function(value2) {
          try {
            crypto.hmac(value1, value2);
            fail();
          }
          catch (err) {
          }
        });
      });
      try {
        crypto.hmac("a", "b", "nosuchalgo");
        fail();
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hmac
////////////////////////////////////////////////////////////////////////////////

    testHmac : function () {
      var data = [
        [ "secret", "", undefined, "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169" ],
        [ "secret", "", "sha256", "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169" ],
        [ "secret", "", "SHA256", "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169" ],
        [ "secret", " ", "sha256", "449cae45786ff49422f05eb94182fb6456b10db5c54f2342387168702e4f5197"],
        [ "secret", "arangodb", "sha256", "85ae370b23a90d5f511a378678edc084f2a0d1190b96f1f543f7e0848a597a6d" ],
        [ "secret", "Arangodb", "sha256", "95144e880bbc4a4bf10a2b683603c763a38817b544e1c2f6ff1bd3523bf60f9e" ],
        [ "secret", "ArangoDB is a database", "sha256", "4888d586d3208ca18ebaf78569949a13f3c03585edb007771cd820820a351b0f" ],
        [ "SECRET", "ArangoDB is a database", "sha256", "a04df5ce362f49439db5e30032b20e0fa64d01c60ceb32a9150e58d3c2c929af" ],
        [ "secret", "ArangoDB is a database", "sha224", "b55c13e25227abf919b510cf2289f4501fa13584676e7e4d56108172" ],
        [ "secret", "ArangoDB is a database", "SHA224", "b55c13e25227abf919b510cf2289f4501fa13584676e7e4d56108172" ],
        [ "secret", "ArangoDB is a database", "sha1", "f39d7a76e502ba3f79d663cfbc9ac43eb6fd323e" ],
        [ "secret", "ArangoDB is a database", "SHA1", "f39d7a76e502ba3f79d663cfbc9ac43eb6fd323e" ],
        [ "secret", "ArangoDB is a database", "md5", "6eecfc947725974efc24bbaaafe15a13" ],
        [ "secret", "ArangoDB is a database", "MD5", "6eecfc947725974efc24bbaaafe15a13" ]
      ];

      data.forEach(function (value) {
        if (value[2] === undefined) {
          assertEqual(value[3], crypto.hmac(value[0], value[1]));
        }
        assertEqual(value[3], crypto.hmac(value[0], value[1], value[2]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test random
////////////////////////////////////////////////////////////////////////////////

    testRandom : function () {
      var i;

      for (i = 0; i < 100; ++i) {
        assertTrue(crypto.rand() !== 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constantEquals
////////////////////////////////////////////////////////////////////////////////

    testConstantEquals : function () {
      var time = require('internal').time;
      var data = [
        [ "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169", "", false ],
        [ "", "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169", false ],
        [ "449cae45786ff49422f05eb94182fb6456b10db5c54f2342387168702e4f5197", "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169", false ],
        [ "85ae370b23a90d5f511a378678edc084f2a0d1190b96f1f543f7e0848a597a6d", "449cae45786ff49422f05eb94182fb6456b10db5c54f2342387168702e4f5197", false ],
        [ "95144e880bbc4a4bf10a2b683603c763a38817b544e1c2f6ff1bd3523bf60f9e", "85ae370b23a90d5f511a378678edc084f2a0d1190b96f1f543f7e0848a597a6d", false ],
        [ "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169", "f9e66e179b6747ae54108f82f8ade8b3c25d76fd30afde6c395822c530196169", true ],
        [ "449cae45786ff49422f05eb94182fb6456b10db5c54f2342387168702e4f5197", "449cae45786ff49422f05eb94182fb6456b10db5c54f2342387168702e4f5197", true ],
        [ "85ae370b23a90d5f511a378678edc084f2a0d1190b96f1f543f7e0848a597a6d", "85ae370b23a90d5f511a378678edc084f2a0d1190b96f1f543f7e0848a597a6d", true ],
        [ "95144e880bbc4a4bf10a2b683603c763a38817b544e1c2f6ff1bd3523bf60f9e", "95144e880bbc4a4bf10a2b683603c763a38817b544e1c2f6ff1bd3523bf60f9e", true ]
      ];

      data.forEach(function(value) {
        assertEqual(value[2], crypto.constantEquals(value[0], value[1]));
      });
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


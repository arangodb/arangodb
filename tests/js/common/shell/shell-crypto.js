/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, fail */

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
var crypto = require("@arangodb/crypto");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function CryptoSuite () {
  'use strict';
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
/// @brief test sha512, invalid values
////////////////////////////////////////////////////////////////////////////////

    testSha512Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.sha512(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha512
////////////////////////////////////////////////////////////////////////////////

    testSha512 : function () {
      var data = [
        [ "", "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" ],
        [ " ", "f90ddd77e400dfe6a3fcf479b00b1ee29e7015c5bb8cd70f5f15b4886cc339275ff553fc8a053f8ddc7324f45168cffaf81f8c3ac93996f6536eef38e5e40768"],
        [ "arangodb", "87509a178c07ce9f75bf70042297e414fded142109644781774bee3a5634ef0986de9806b9a63e18353037ef9fc04c6fc0cab2b12eff5d081e0a9d4d8412c4eb" ],
        [ "Arangodb", "fb4d3ac6ccf6ac32751943ae10aa4cb86e1495042898e2d24fae79220f9421d94394db3be05a5e5f92b4ffe7ca4356bff56aa3eee0e68365e77245ebb6c34fb5" ],
        [ "ArangoDB is a database", "b6a1ca6cdc7d8085ceda20a5b78251787df5f959daa36929f6bc6bb517dd9adc5d1610f43443151d14294ece1885e5560c12ca44d10e430d0208ca4bc481ebbd" ]
      ];

      data.forEach(function (value) {
        assertEqual(value[1], crypto.sha512(value[0]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha256, invalid values
////////////////////////////////////////////////////////////////////////////////

    testSha384Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value) {
        try {
          crypto.sha384(value);
          fail();
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sha384
////////////////////////////////////////////////////////////////////////////////

    testSha384 : function () {
      var data = [
        [ "", "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" ],
        [ " ", "588016eb10045dd85834d67d187d6b97858f38c58c690320c4a64e0c2f92eebd9f1bd74de256e8268815905159449566"],
        [ "arangodb", "d2b5ff08b3784080520f11535243c2314e3f9ef42335d8e80d17cb8b002626d9833d9cd68c50b0b5aea8f2c111fb95dd" ],
        [ "Arangodb", "643d99c8edd96d48075161ad92f541bdd6d77460c1b1fd14353abcc309155f84ca7c138df1b647db59c537afd7b80521" ],
        [ "ArangoDB is a database", "579f8b2972baf5b0acb3b4db39afeebd7274b1a2083cd110a554df0b63cdc0cc757d1e8d771e51e71cfe2c2ac2617e93" ]
      ];

      data.forEach(function (value) {
        assertEqual(value[1], crypto.sha384(value[0]));
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
        [ "secret", "ArangoDB is a database", "sha384", "bb8da1979a964225996280d30c1ff73b1297145aaaa57520d4a1f38c648a85541f2d8e22f7e20dc3a556563e386521a7" ],
        [ "secret", "ArangoDB is a database", "SHA384", "bb8da1979a964225996280d30c1ff73b1297145aaaa57520d4a1f38c648a85541f2d8e22f7e20dc3a556563e386521a7" ],
        [ "secret", "ArangoDB is a database", "sha512", "8ef4e708db5bbc13ecf675ab81c9bfac72faedaf68fae91c51c0736746d087396af758a43cd60f763a5a8187d856c906c1677c7525b756cdb1ad5a7df823df73" ],
        [ "secret", "ArangoDB is a database", "SHA512", "8ef4e708db5bbc13ecf675ab81c9bfac72faedaf68fae91c51c0736746d087396af758a43cd60f763a5a8187d856c906c1677c7525b756cdb1ad5a7df823df73" ],
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
/// @brief test hotp
////////////////////////////////////////////////////////////////////////////////

    testHotp : function () {
      var data = [
        [ "secret", undefined, "814628" ],
        [ "secret", 0, "814628" ],
        [ "secret", 1, "533881"],
        [ "secret", 2, "720111" ],
        [ "secret", 3, "282621" ],
        [ "secret", 42, "852786" ],
        [ "SECRET", 12345678, "208658" ]
      ];

      data.forEach(function (value) {
        assertEqual(value[2], crypto.hotpGenerate(value[0], value[1]));
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], value[1], 0);
        assertNotEqual(result, null);
        assertEqual(result.delta, 0);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) - 1, 0);
        assertEqual(result, null);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) + 1, 0);
        assertEqual(result, null);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) - 20, 19);
        assertEqual(result, null);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) + 20, 19);
        assertEqual(result, null);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) - 20, 20);
        assertNotEqual(result, null);
        assertEqual(result.delta, 20);
      });

      data.forEach(function (value) {
        var result = crypto.hotpVerify(value[2], value[0], (value[1] || 0) + 20, 20);
        assertNotEqual(result, null);
        assertEqual(result.delta, -20);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test pbkdf2, invalid values
////////////////////////////////////////////////////////////////////////////////

    testPbkdf2Invalid : function () {
      [ undefined, null, true, false, 0, 1, -1, 32.5, [ ], { } ].forEach(function (value1, i, arr) {
        arr.forEach(function(value2) {
          arr.forEach(function(value3) {
            arr.forEach(function(value4) {
              try {
                crypto.pbkdf2(value1, value2, value3, value4);
                fail();
              }
              catch (err) {
              }
            });
          });
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test pbkdf2
////////////////////////////////////////////////////////////////////////////////

    testPbkdf2 : function () {
      var data = [
        [ 'secret', '', 10, 16, 'sha1', '7ae85791509c581fbda11a945893d623' ],
        [ 'secret', '', 10, 16, null, '7ae85791509c581fbda11a945893d623' ],
        [ 'secret', '', 10, 16, undefined, '7ae85791509c581fbda11a945893d623' ],
        [ 'secret', ' ', 10, 16, undefined, '539e069417a0148aa8673d206b57f345' ],
        [ 'secret', 'arangodb', 10, 16, undefined, 'cd55a9a823ba8a788060c2353440092f' ],
        [ 'secret', 'Arangodb', 10, 16, undefined, '9af0cdc4ab37b4a17120f9796941046d' ],
        [ 'secret', 'Arangodb', 10, 32, undefined, '9af0cdc4ab37b4a17120f9796941046d34c5260035189c297df5c07f9f7748a6' ],
        [ 'secret', 'Arangodb', 100, 16, undefined, 'e9afe4e0f3d6fb5dddbe98046be214f7' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, undefined, '9a6a9c9f7bc553f9ee41e95f22a9600f' ],
        [ 'SECRET', 'ArangoDB is a database', 10, 16, undefined, '9a56ce2d622a09678bcc7fbb1f01d47c' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, 'sha224', 'eb22c5bd06658b6c1b3e592c67d4264a' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, 'sha256', '4af8fad53d19ac984c0d1a8c128d5391' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, 'sha384', '9684202a2d1c1ed248724d248c866f11' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, 'sha512', '5818089d14f7366e0656ec851d55a140' ],
        [ 'secret', 'ArangoDB is a database', 10, 16, 'md5', 'b4a63e5a9c02d76cb40aa37fd34c0a4f' ]
      ];

      data.forEach(function (value) {
        assertEqual(value[5], crypto.pbkdf2(value[0], value[1], value[2], value[3], value[4]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test jwtEncode / jwtDecode
////////////////////////////////////////////////////////////////////////////////

    testJwt : function () {
      var data = [
        [ "secret", "", undefined, "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.IiI.NXK2YUp4x5L1lDfGi34S-_Sk3q6Xeehm3gSwpwpjFDk" ],
        [ "secret", "", "hs256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.IiI.NXK2YUp4x5L1lDfGi34S-_Sk3q6Xeehm3gSwpwpjFDk" ],
        [ "secret", "", "HS256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.IiI.NXK2YUp4x5L1lDfGi34S-_Sk3q6Xeehm3gSwpwpjFDk" ],
        [ "secret", " ", "hs256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.IiAi.FKO_hoD03EEA3P7487qeO9JqAi8VriTu2JaEnHfZvz8"],
        [ "secret", "arangodb", "hs256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.ImFyYW5nb2RiIg.pyWsjffR5WfVkRxtckXKwh-emE2kmKH0ZJRCCllqIYc" ],
        [ "secret", "Arangodb", "hs256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.IkFyYW5nb2RiIg.BjQqhHpWiGqi2RBeAjV1V0gkUBPNZHtKCu5rgeu9eno" ],
        [ "secret", {foxx: "roxx"}, "HS256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJmb3h4Ijoicm94eCJ9.tCZwaqnZ7Wj9BljBndyDtINYWmmvr0eLsq8bkmtXhg0" ],
        [ "SECRET", {foxx: "roxx"}, "HS256", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJmb3h4Ijoicm94eCJ9.oXcCBnmuv9GzqFc0_N2qFXLWKDjCKEmN015CccDAgfw" ],
        [ "secret", {foxx: "roxx"}, "HS384", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzM4NCJ9.eyJmb3h4Ijoicm94eCJ9.kkgSDKjZZh8OSbjeGLjObeLVcbJiH7EtFzS-WjQtSLfYsNLfSULsTuOYVctMaAk5" ],
        [ "SECRET", {foxx: "roxx"}, "HS384", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzM4NCJ9.eyJmb3h4Ijoicm94eCJ9.HHrNONcNcrK0Y-xfiOsw9tNe-zcsOmS9kdTr14dmH_2-71QTPIRnJGTLfl58URtM" ],
        [ "secret", {foxx: "roxx"}, "HS512", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzUxMiJ9.eyJmb3h4Ijoicm94eCJ9.zjLEjyjxv_NzWMPQEyXcSgFB9c2-t1n_jZRQkxnpQU9-UNJQ-kUpW8pYsObMHDKcmM8GspmX4X5653Fb-ZDkWA" ],
        [ "SECRET", {foxx: "roxx"}, "HS512", "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzUxMiJ9.eyJmb3h4Ijoicm94eCJ9.6MGHLPoS6r_F9HTZcHyRFWaQmLDf4boaTK5cxNnJPQeXNTSp8itLo4b1KPnq-wL4Q4HxnomghQLWRUjW612Wug" ],
        [ "", {foxx: "roxx"}, "none", "eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.eyJmb3h4Ijoicm94eCJ9." ],
        [ null, {foxx: "roxx"}, "none", "eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.eyJmb3h4Ijoicm94eCJ9." ]
      ];

      data.forEach(function (value) {
        if (value[2] === undefined) {
          assertEqual(value[3], crypto.jwtEncode(value[0], value[1]));
        }
        assertEqual(value[3], crypto.jwtEncode(value[0], value[1], value[2]));
        assertEqual(value[1], crypto.jwtDecode(value[0], value[3]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test jwtEncode / jwtDecode
////////////////////////////////////////////////////////////////////////////////

    testJwtBadSignature : function () {
      var message = {foxx: "roxx"};
      var token, err;

      token = crypto.jwtEncode(null, message, 'none');
      try {
        crypto.jwtDecode('secret', token);
      } catch(e) {
        err = e;
      }
      assertTrue(err);

      token = crypto.jwtEncode('secret', message, 'HS512');
      try {
        crypto.jwtDecode('SECRET', message);
      } catch(e) {
        err = e;
      }
      assertTrue(err);

      try {
        crypto.jwtEncode('secret', message, 'none');
      } catch(e) {
        err = e;
      }
      assertTrue(err);
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
/// @brief test uuidv4
////////////////////////////////////////////////////////////////////////////////

    testUuidv4 : function () {
      const uuidv4 = crypto.uuidv4();
      assertTrue(uuidv4.match(/^[0-9a-f]{8}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{12}$/));
      assertNotEqual(uuidv4, crypto.uuidv4());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test genRandomBytes
////////////////////////////////////////////////////////////////////////////////

    testRandomBytes : function () {
      const bytes = crypto.genRandomBytes(33);
      assertTrue(bytes instanceof Buffer);
      assertEqual(bytes.length, 33);
      assertNotEqual(Array.from(bytes).reduce((sum, value) => sum + value), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constantEquals
////////////////////////////////////////////////////////////////////////////////

    testConstantEquals : function () {
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


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CryptoSuite);

return jsunity.done();


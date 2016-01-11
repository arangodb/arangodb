/*jshint globalstrict:false, strict:false, maxlen:400 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the base64 functions
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
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function Base64Suite () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test encode
////////////////////////////////////////////////////////////////////////////////

    testBase64Encode : function () {
      var data = [
        ["",""],
        [" ","IA=="],
        ["  ","ICA="],
        ["\nnew line\n","Cm5ldyBsaW5lCg=="],
        ["abc","YWJj"],
        ["ABC","QUJD"],
        ["abC","YWJD"],
        [" aBC","IGFCQw=="],
        ["123","MTIz"],
        ["abcdef123456","YWJjZGVmMTIzNDU2"],
        ["the Quick brown fox jumped over the lazy dog","dGhlIFF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c="],
        ["This is a long string that contains a lot of characters. It should work without any particular problems, too.","VGhpcyBpcyBhIGxvbmcgc3RyaW5nIHRoYXQgY29udGFpbnMgYSBsb3Qgb2YgY2hhcmFjdGVycy4gSXQgc2hvdWxkIHdvcmsgd2l0aG91dCBhbnkgcGFydGljdWxhciBwcm9ibGVtcywgdG9vLg=="],
        [1,"MQ=="],
        [2,"Mg=="],
        [-1,"LTE="],
        [100,"MTAw"],
        [1000,"MTAwMA=="],
        [99,"OTk="]
      ];
      
      data.forEach(function (value) {
        assertEqual(value[1], internal.base64Encode(value[0]));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test decode
////////////////////////////////////////////////////////////////////////////////

    testBase64Decode : function () {
      var data = [
        ["",""],
        [" ","IA=="],
        ["  ","ICA="],
        ["\nnew line\n","Cm5ldyBsaW5lCg=="],
        ["abc","YWJj"],
        ["ABC","QUJD"],
        ["abC","YWJD"],
        [" aBC","IGFCQw=="],
        ["123","MTIz"],
        ["abcdef123456","YWJjZGVmMTIzNDU2"],
        ["the Quick brown fox jumped over the lazy dog","dGhlIFF1aWNrIGJyb3duIGZveCBqdW1wZWQgb3ZlciB0aGUgbGF6eSBkb2c="],
        ["This is a long string that contains a lot of characters. It should work without any particular problems, too.","VGhpcyBpcyBhIGxvbmcgc3RyaW5nIHRoYXQgY29udGFpbnMgYSBsb3Qgb2YgY2hhcmFjdGVycy4gSXQgc2hvdWxkIHdvcmsgd2l0aG91dCBhbnkgcGFydGljdWxhciBwcm9ibGVtcywgdG9vLg=="],
        ["1","MQ=="],
        ["2","Mg=="],
        ["-1","LTE="],
        ["100","MTAw"],
        ["1000","MTAwMA=="],
        ["99","OTk="]
      ];
      
      data.forEach(function (value) {
        assertEqual(value[0], internal.base64Decode(value[1]));
      });
    }
    
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(Base64Suite);

return jsunity.done();


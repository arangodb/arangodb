/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, fail */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const errors = require("internal").errors;

function binaryLiteralsSuite () {
  return {
    
    testBrokenBinaryLiterals : function () {
      let cases = [
        "0b", 
        "0B", 
        "0b010101z111", 
        "0b010101001001z", 
        "0bz001010", 
        "0ba01010b00101", 
        "0b01010ab00101", 
        "0b010100001011a", 
        "0b001010.0", 
        "0b001010.12", 
        "0b-001010", 
        "0b000000000000000000000000000000000",
        "0b000000000000000000000000000000001",
        "0b000000000000000000000000000000000000000000000000000000000000000",
        "0B000000000000000000000000000000000000000000000000000000000000000",
        "0b0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
        "0B0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001",
        "0b111111111111111111111111111111110",
        "0b111111111111111111111111111111111",
        "0b0000011111111111111111111111111111111",
        "0B0000000000000000000011111111111111111111111111111111",
        "-0B0000000000000000000011111111111111111111111111111111", 
      ];

      cases.forEach((c) => {
        const query = "RETURN " + c;
        try {
          AQL_EXECUTE(query).json[0];
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum, c);
        }
      });
    },

    testWorkingBinaryLiterals : function () {
      let cases = [
        ["0b0", 0],
        ["0B0", 0],
        ["0b00", 0],
        ["0B00", 0],
        ["0b000", 0],
        ["0B000", 0],
        ["0b1", 1],
        ["0B1", 1],
        ["0b01", 1],
        ["0B01", 1],
        ["0b10", 2],
        ["0B10", 2],
        ["0b010", 2],
        ["0B010", 2],
        ["0b11", 3],
        ["0B11", 3],
        ["0b100", 4],
        ["0B100", 4],
        ["0b101", 5],
        ["0B101", 5],
        ["0b110", 6],
        ["0B110", 6],
        ["0b111", 7],
        ["0B111", 7],
        ["0b1000", 8],
        ["0B1000", 8],
        ["0b1001", 9],
        ["0B1001", 9],
        ["0b010000", 16],
        ["0B010000", 16],
        ["0b1111111", 127],
        ["0B1111111", 127],
        ["0b10000000", 128],
        ["0B10000000", 128],
        ["0b11111111", 255],
        ["0B11111111", 255],
        ["0b101010101", 341],
        ["0B101010101", 341],
        ["0b1111111111111111", 65535],
        ["0B1111111111111111", 65535],
        ["0b10000000000000000", 65536],
        ["0B10000000000000000", 65536],
        ["0b111111111111111111111111", 16777215],
        ["0B111111111111111111111111", 16777215],
        ["0b00000000000000000000000000000000", 0],
        ["0b00000000000000000000000000000001", 1],
        ["0b11111111111111111111111111111111", 4294967295],
        ["0B11111111111111111111111111111111", 4294967295],
        ["-0b1", -1],
        ["-0b1111", -15],
      ];

      cases.forEach((c) => {
        const query = "RETURN " + c[0];
        let result = AQL_EXECUTE(query).json[0];
        assertEqual(c[1], result, c);
      });
    },

  };
}

function hexadecimalLiteralsSuite () {
  return {
    
    testBrokenHexadecimalLiterals : function () {
      let cases = [
        "0x", 
        "0X", 
        "0x01234z",
        "0x01234z4353",
        "0xz01234",
        "0xffffffff0",
        "0x100000000",
        "0x101010101",
        "0x1111111111111111",
        "0x10000000000000000",
        "0x111111111111111111111111",
        "0x0000011111111111111111111111111111111", 
        "0x0000000000000000000011111111111111111111111111111111", 
        "0x0000000000FFFFFFFF", 
        "0X0000000000fFfFfFfF",
        "0x000000000000000000000000000000000000000000000000000000000000000",
        "0X000000000000000000000000000000000000000000000000000000000000000",
        "0x0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001", 
        "0X0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001", 
      ];

      cases.forEach((c) => {
        const query = "RETURN " + c;
        try {
          AQL_EXECUTE(query).json[0];
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum, c);
        }
      });
    },

    testWorkingHexadecimalLiterals : function () {
      let cases = [
        ["0x0", 0],
        ["0X0", 0],
        ["0x00", 0],
        ["0X00", 0],
        ["0x000", 0],
        ["0X000", 0],
        ["0x1", 1],
        ["0X1", 1],
        ["0x01", 1],
        ["0X01", 1],
        ["0x02", 2],
        ["0X02", 2],
        ["0x3", 3],
        ["0X3", 3],
        ["0x4", 4],
        ["0X4", 4],
        ["0x5", 5],
        ["0X5", 5],
        ["0x6", 6],
        ["0X6", 6],
        ["0x7", 7],
        ["0X7", 7],
        ["0x8", 8],
        ["0X8", 8],
        ["0x9", 9],
        ["0X9", 9],
        ["0xa", 10],
        ["0Xa", 10],
        ["0xA", 10],
        ["0XA", 10],
        ["0xb", 11],
        ["0Xb", 11],
        ["0xB", 11],
        ["0XB", 11],
        ["0xc", 12],
        ["0Xc", 12],
        ["0xC", 12],
        ["0XC", 12],
        ["0xd", 13],
        ["0Xd", 13],
        ["0xD", 13],
        ["0XD", 13],
        ["0xe", 14],
        ["0Xe", 14],
        ["0xE", 14],
        ["0XE", 14],
        ["0xf", 15],
        ["0Xf", 15],
        ["0xF", 15],
        ["0XF", 15],
        ["0x10", 16],
        ["0X10", 16],
        ["0x010", 16],
        ["0X010", 16],
        ["0x11", 17],
        ["0X11", 17],
        ["0x1a", 26],
        ["0X1a", 26],
        ["0x1A", 26],
        ["0X1A", 26],
        ["0x1f", 31],
        ["0X1F", 31],
        ["0x7f", 127],
        ["0X7F", 127],
        ["0x80", 128],
        ["0X80", 128],
        ["0xa0", 160],
        ["0Xaa", 170],
        ["0XaA", 170],
        ["0XAa", 170],
        ["0XAA", 170],
        ["0xff", 255],
        ["0XFF", 255],
        ["0x100", 256],
        ["0X100", 256],
        ["0x101", 257],
        ["0X101", 257],
        ["0x110", 272],
        ["0X110", 272],
        ["0x111", 273],
        ["0X111", 273],
        ["0xfff", 4095],
        ["0xFFF", 4095],
        ["0x1000", 4096],
        ["0x1000", 4096],
        ["0x1001", 4097],
        ["0X1001", 4097],
        ["0x10000", 65536],
        ["0X10000", 65536],
        ["0xabcde", 703710],
        ["0xAbCdE", 703710],
        ["0x1111111", 17895697],
        ["0X1111111", 17895697],
        ["0x00000000", 0],
        ["0x10000000", 268435456],
        ["0X10000000", 268435456],
        ["0x11111111", 286331153],
        ["0X11111111", 286331153],
        ["0xffffffff", 4294967295],
        ["0xFFFFFFFF", 4294967295],
        ["-0x1", -1],
        ["-0X1", -1],
        ["-0xaa", -170],
        ["-0Xaa", -170],
        ["-0x00000000", 0],
        ["-0xffffffff", -4294967295],
        ["-0Xffffffff", -4294967295],
        ["-0XFFFFFFFF", -4294967295],
      ];

      cases.forEach((c) => {
        const query = "RETURN " + c[0];
        let result = AQL_EXECUTE(query).json[0];
        assertEqual(c[1], result, c);
      });
    },

  };
}

jsunity.run(binaryLiteralsSuite);
jsunity.run(hexadecimalLiteralsSuite);

return jsunity.done();

/* jshint globalstrict:false, strict:false, maxlen:5000 */
/* global assertEqual, assertNotEqual, assertTrue, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const errors = internal.errors;
const jsunity = require('jsunity');
const db = internal.db;
const helper = require('@arangodb/aql-helper');
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

function ahuacatlStringFunctionsTestSuite () {
  return {
    testRepeat: function () {
      const values = [
        [ "a", 3, undefined, "aaa" ],
        [ "abc", 3, undefined, "abcabcabc" ],
        [ "", 3, undefined, "" ],
        [ "ABC", 3, ",", "ABC,ABC,ABC" ],
        [ "foxxFoxxFaxx", 0, undefined, "" ],
        [ "foxxFoxxFaxx", 0, "abc", "" ],
        [ "f", 1024, undefined, Array(1025).join("f") ],
        [ "f", 2, "abc", "fabcf" ],
        [ "ü", 10, undefined, "üüüüüüüüüü" ],
        [ "ü", 10, "ä", "üäüäüäüäüäüäüäüäüäü" ],
        [ "ü", 10, "äxö", "üäxöüäxöüäxöüäxöüäxöüäxöüäxöüäxöüäxöü" ],
        [ "MÖTÖR", 3, "㗸", "MÖTÖR㗸MÖTÖR㗸MÖTÖR" ],
      ];

      values.forEach((v) => {
        let query = 'RETURN REPEAT(' + JSON.stringify(v[0]) + ', ' + JSON.stringify(v[1]);
        if (v[2] !== undefined) {
          query += ', ' + JSON.stringify(v[2]);
        }
        query += ')';
        assertEqual([ v[3] ], getQueryResults(query), v);
      });
    },
    
    testRepeatInvalidInputs: function () {
      [-1, -1000].forEach((v) => {
        const query = 'RETURN REPEAT("abc", ' + JSON.stringify(v) + ')';
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, query);
      });
    },
    
    testRepeatTooLongInput: function () {
      [
        ["abcdefghij", 1717986919],
        ["a", 16384 * 1024 * 1024 + 1],
        ["ab", 8192 * 1024 * 1024 + 1],
      ].forEach((v) => {
        const query = 'RETURN REPEAT(' + JSON.stringify(v[0]) + ', ' + JSON.stringify(v[1]) + ')';
        assertQueryWarningAndNull(errors.ERROR_RESOURCE_LIMIT.code, query);
      });
    },
    
    testRepeatInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REPEAT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REPEAT(1)');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REPEAT(1, 2, 3, 4)');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test to_char
// //////////////////////////////////////////////////////////////////////////////

    testToChar: function () {
      const values =  [
        [ 32, ' ' ],     [ 33, '!' ],     [ 34, '"' ],     [ 35, '#' ],
        [ 36, '$' ],     [ 37, '%' ],     [ 38, '&' ],     [ 39, "'" ],
        [ 40, '(' ],     [ 41, ')' ],     [ 42, '*' ],     [ 43, '+' ],
        [ 44, ',' ],     [ 45, '-' ],     [ 46, '.' ],     [ 47, '/' ],
        [ 48, '0' ],     [ 49, '1' ],     [ 50, '2' ],     [ 51, '3' ],
        [ 52, '4' ],     [ 53, '5' ],     [ 54, '6' ],     [ 55, '7' ],
        [ 56, '8' ],     [ 57, '9' ],     [ 58, ':' ],     [ 59, ';' ],
        [ 60, '<' ],     [ 61, '=' ],     [ 62, '>' ],     [ 63, '?' ],
        [ 64, '@' ],     [ 65, 'A' ],     [ 66, 'B' ],     [ 67, 'C' ],
        [ 68, 'D' ],     [ 69, 'E' ],     [ 70, 'F' ],     [ 71, 'G' ],
        [ 72, 'H' ],     [ 73, 'I' ],     [ 74, 'J' ],     [ 75, 'K' ],
        [ 76, 'L' ],     [ 77, 'M' ],     [ 78, 'N' ],     [ 79, 'O' ],
        [ 80, 'P' ],     [ 81, 'Q' ],     [ 82, 'R' ],     [ 83, 'S' ],
        [ 84, 'T' ],     [ 85, 'U' ],     [ 86, 'V' ],     [ 87, 'W' ],
        [ 88, 'X' ],     [ 89, 'Y' ],     [ 90, 'Z' ],     [ 91, '[' ],
        [ 92, '\\' ],    [ 93, ']' ],     [ 94, '^' ],     [ 95, '_' ],
        [ 96, '`' ],     [ 97, 'a' ],     [ 98, 'b' ],     [ 99, 'c' ],
        [ 100, 'd' ],    [ 101, 'e' ],    [ 102, 'f' ],    [ 103, 'g' ],
        [ 104, 'h' ],    [ 105, 'i' ],    [ 106, 'j' ],    [ 107, 'k' ],
        [ 108, 'l' ],    [ 109, 'm' ],    [ 110, 'n' ],    [ 111, 'o' ],
        [ 112, 'p' ],    [ 113, 'q' ],    [ 114, 'r' ],    [ 115, 's' ],
        [ 116, 't' ],    [ 117, 'u' ],    [ 118, 'v' ],    [ 119, 'w' ],
        [ 120, 'x' ],    [ 121, 'y' ],    [ 122, 'z' ],    [ 123, '{' ],
        [ 124, '|' ],    [ 125, '}' ],    [ 126, '~' ],    [ 127, '\x7F' ],
        [ 128, '\x80' ], [ 129, '\x81' ], [ 130, '\x82' ], [ 131, '\x83' ],
        [ 132, '\x84' ], [ 133, '\x85' ], [ 134, '\x86' ], [ 135, '\x87' ],
        [ 136, '\x88' ], [ 137, '\x89' ], [ 138, '\x8A' ], [ 139, '\x8B' ],
        [ 140, '\x8C' ], [ 141, '\x8D' ], [ 142, '\x8E' ], [ 143, '\x8F' ],
        [ 144, '\x90' ], [ 145, '\x91' ], [ 146, '\x92' ], [ 147, '\x93' ],
        [ 148, '\x94' ], [ 149, '\x95' ], [ 150, '\x96' ], [ 151, '\x97' ],
        [ 152, '\x98' ], [ 153, '\x99' ], [ 154, '\x9A' ], [ 155, '\x9B' ],
        [ 156, '\x9C' ], [ 157, '\x9D' ], [ 158, '\x9E' ], [ 159, '\x9F' ],
        [ 160, ' ' ],    [ 161, '¡' ],    [ 162, '¢' ],    [ 163, '£' ],
        [ 164, '¤' ],    [ 165, '¥' ],    [ 166, '¦' ],    [ 167, '§' ],
        [ 168, '¨' ],    [ 169, '©' ],    [ 170, 'ª' ],    [ 171, '«' ],
        [ 172, '¬' ],    [ 173, '­' ],    [ 174, '®' ],    [ 175, '¯' ],
        [ 176, '°' ],    [ 177, '±' ],    [ 178, '²' ],    [ 179, '³' ],
        [ 180, '´' ],    [ 181, 'µ' ],    [ 182, '¶' ],    [ 183, '·' ],
        [ 184, '¸' ],    [ 185, '¹' ],    [ 186, 'º' ],    [ 187, '»' ],
        [ 188, '¼' ],    [ 189, '½' ],    [ 190, '¾' ],    [ 191, '¿' ],
        [ 192, 'À' ],    [ 193, 'Á' ],    [ 194, 'Â' ],    [ 195, 'Ã' ],
        [ 196, 'Ä' ],    [ 197, 'Å' ],    [ 198, 'Æ' ],    [ 199, 'Ç' ],
        [ 200, 'È' ],    [ 201, 'É' ],    [ 202, 'Ê' ],    [ 203, 'Ë' ],
        [ 204, 'Ì' ],    [ 205, 'Í' ],    [ 206, 'Î' ],    [ 207, 'Ï' ],
        [ 208, 'Ð' ],    [ 209, 'Ñ' ],    [ 210, 'Ò' ],    [ 211, 'Ó' ],
        [ 212, 'Ô' ],    [ 213, 'Õ' ],    [ 214, 'Ö' ],    [ 215, '×' ],
        [ 216, 'Ø' ],    [ 217, 'Ù' ],    [ 218, 'Ú' ],    [ 219, 'Û' ],
        [ 220, 'Ü' ],    [ 221, 'Ý' ],    [ 222, 'Þ' ],    [ 223, 'ß' ],
        [ 224, 'à' ],    [ 225, 'á' ],    [ 226, 'â' ],    [ 227, 'ã' ],
        [ 228, 'ä' ],    [ 229, 'å' ],    [ 230, 'æ' ],    [ 231, 'ç' ],
        [ 232, 'è' ],    [ 233, 'é' ],    [ 234, 'ê' ],    [ 235, 'ë' ],
        [ 236, 'ì' ],    [ 237, 'í' ],    [ 238, 'î' ],    [ 239, 'ï' ],
        [ 240, 'ð' ],    [ 241, 'ñ' ],    [ 242, 'ò' ],    [ 243, 'ó' ],
        [ 244, 'ô' ],    [ 245, 'õ' ],    [ 246, 'ö' ],    [ 247, '÷' ],
        [ 248, 'ø' ],    [ 249, 'ù' ],    [ 250, 'ú' ],    [ 251, 'û' ],
        [ 252, 'ü' ],    [ 253, 'ý' ],    [ 254, 'þ' ],    [ 255, 'ÿ' ],
        [ 256, 'Ā' ],    [ 257, 'ā' ],    [ 258, 'Ă' ],    [ 259, 'ă' ],
        [ 260, 'Ą' ],    [ 261, 'ą' ],    [ 262, 'Ć' ],    [ 263, 'ć' ],
        [ 264, 'Ĉ' ],    [ 265, 'ĉ' ],    [ 266, 'Ċ' ],    [ 267, 'ċ' ],
        [ 268, 'Č' ],    [ 269, 'č' ],    [ 270, 'Ď' ],    [ 271, 'ď' ],
        [ 272, 'Đ' ],    [ 273, 'đ' ],    [ 274, 'Ē' ],    [ 275, 'ē' ],
        [ 276, 'Ĕ' ],    [ 277, 'ĕ' ],    [ 278, 'Ė' ],    [ 279, 'ė' ],
        [ 280, 'Ę' ],    [ 281, 'ę' ],    [ 282, 'Ě' ],    [ 283, 'ě' ],
        [ 284, 'Ĝ' ],    [ 285, 'ĝ' ],    [ 286, 'Ğ' ],    [ 287, 'ğ' ],
        [ 288, 'Ġ' ],    [ 289, 'ġ' ],    [ 290, 'Ģ' ],    [ 291, 'ģ' ],
        [ 292, 'Ĥ' ],    [ 293, 'ĥ' ],    [ 294, 'Ħ' ],    [ 295, 'ħ' ],
        [ 296, 'Ĩ' ],    [ 297, 'ĩ' ],    [ 298, 'Ī' ],    [ 299, 'ī' ],
        [ 300, 'Ĭ' ],    [ 301, 'ĭ' ],    [ 302, 'Į' ],    [ 303, 'į' ],
        [ 304, 'İ' ],    [ 305, 'ı' ],    [ 306, 'Ĳ' ],    [ 307, 'ĳ' ],
        [ 308, 'Ĵ' ],    [ 309, 'ĵ' ],    [ 310, 'Ķ' ],    [ 311, 'ķ' ],
        [ 312, 'ĸ' ],    [ 313, 'Ĺ' ],    [ 314, 'ĺ' ],    [ 315, 'Ļ' ],
        [ 316, 'ļ' ],    [ 317, 'Ľ' ],    [ 318, 'ľ' ],    [ 319, 'Ŀ' ],
        [ 320, 'ŀ' ],    [ 321, 'Ł' ],    [ 322, 'ł' ],    [ 323, 'Ń' ],
        [ 324, 'ń' ],    [ 325, 'Ņ' ],    [ 326, 'ņ' ],    [ 327, 'Ň' ],
        [ 328, 'ň' ],    [ 329, 'ŉ' ],    [ 330, 'Ŋ' ],    [ 331, 'ŋ' ],
        [ 332, 'Ō' ],    [ 333, 'ō' ],    [ 334, 'Ŏ' ],    [ 335, 'ŏ' ],
        [ 336, 'Ő' ],    [ 337, 'ő' ],    [ 338, 'Œ' ],    [ 339, 'œ' ],
        [ 340, 'Ŕ' ],    [ 341, 'ŕ' ],    [ 342, 'Ŗ' ],    [ 343, 'ŗ' ],
        [ 344, 'Ř' ],    [ 345, 'ř' ],    [ 346, 'Ś' ],    [ 347, 'ś' ],
        [ 348, 'Ŝ' ],    [ 349, 'ŝ' ],    [ 350, 'Ş' ],    [ 351, 'ş' ],
        [ 352, 'Š' ],    [ 353, 'š' ],    [ 354, 'Ţ' ],    [ 355, 'ţ' ],
        [ 356, 'Ť' ],    [ 357, 'ť' ],    [ 358, 'Ŧ' ],    [ 359, 'ŧ' ],
        [ 360, 'Ũ' ],    [ 361, 'ũ' ],    [ 362, 'Ū' ],    [ 363, 'ū' ],
        [ 364, 'Ŭ' ],    [ 365, 'ŭ' ],    [ 366, 'Ů' ],    [ 367, 'ů' ],
        [ 368, 'Ű' ],    [ 369, 'ű' ],    [ 370, 'Ų' ],    [ 371, 'ų' ],
        [ 372, 'Ŵ' ],    [ 373, 'ŵ' ],    [ 374, 'Ŷ' ],    [ 375, 'ŷ' ],
        [ 376, 'Ÿ' ],    [ 377, 'Ź' ],    [ 378, 'ź' ],    [ 379, 'Ż' ],
        [ 380, 'ż' ],    [ 381, 'Ž' ],    [ 382, 'ž' ],    [ 383, 'ſ' ],
        [ 384, 'ƀ' ],    [ 385, 'Ɓ' ],    [ 386, 'Ƃ' ],    [ 387, 'ƃ' ],
        [ 388, 'Ƅ' ],    [ 389, 'ƅ' ],    [ 390, 'Ɔ' ],    [ 391, 'Ƈ' ],
        [ 392, 'ƈ' ],    [ 393, 'Ɖ' ],    [ 394, 'Ɗ' ],    [ 395, 'Ƌ' ],
        [ 396, 'ƌ' ],    [ 397, 'ƍ' ],    [ 398, 'Ǝ' ],    [ 399, 'Ə' ],
        [ 400, 'Ɛ' ],    [ 401, 'Ƒ' ],    [ 402, 'ƒ' ],    [ 403, 'Ɠ' ],
        [ 404, 'Ɣ' ],    [ 405, 'ƕ' ],    [ 406, 'Ɩ' ],    [ 407, 'Ɨ' ],
        [ 408, 'Ƙ' ],    [ 409, 'ƙ' ],    [ 410, 'ƚ' ],    [ 411, 'ƛ' ],
        [ 412, 'Ɯ' ],    [ 413, 'Ɲ' ],    [ 414, 'ƞ' ],    [ 415, 'Ɵ' ],
        [ 416, 'Ơ' ],    [ 417, 'ơ' ],    [ 418, 'Ƣ' ],    [ 419, 'ƣ' ],
        [ 420, 'Ƥ' ],    [ 421, 'ƥ' ],    [ 422, 'Ʀ' ],    [ 423, 'Ƨ' ],
        [ 424, 'ƨ' ],    [ 425, 'Ʃ' ],    [ 426, 'ƪ' ],    [ 427, 'ƫ' ],
        [ 428, 'Ƭ' ],    [ 429, 'ƭ' ],    [ 430, 'Ʈ' ],    [ 431, 'Ư' ],
        [ 432, 'ư' ],    [ 433, 'Ʊ' ],    [ 434, 'Ʋ' ],    [ 435, 'Ƴ' ],
        [ 436, 'ƴ' ],    [ 437, 'Ƶ' ],    [ 438, 'ƶ' ],    [ 439, 'Ʒ' ],
        [ 440, 'Ƹ' ],    [ 441, 'ƹ' ],    [ 442, 'ƺ' ],    [ 443, 'ƻ' ],
        [ 444, 'Ƽ' ],    [ 445, 'ƽ' ],    [ 446, 'ƾ' ],    [ 447, 'ƿ' ],
        [ 448, 'ǀ' ],    [ 449, 'ǁ' ],    [ 450, 'ǂ' ],    [ 451, 'ǃ' ],
        [ 452, 'Ǆ' ],    [ 453, 'ǅ' ],    [ 454, 'ǆ' ],    [ 455, 'Ǉ' ],
        [ 456, 'ǈ' ],    [ 457, 'ǉ' ],    [ 458, 'Ǌ' ],    [ 459, 'ǋ' ],
        [ 460, 'ǌ' ],    [ 461, 'Ǎ' ],    [ 462, 'ǎ' ],    [ 463, 'Ǐ' ],
        [ 464, 'ǐ' ],    [ 465, 'Ǒ' ],    [ 466, 'ǒ' ],    [ 467, 'Ǔ' ],
        [ 468, 'ǔ' ],    [ 469, 'Ǖ' ],    [ 470, 'ǖ' ],    [ 471, 'Ǘ' ],
        [ 472, 'ǘ' ],    [ 473, 'Ǚ' ],    [ 474, 'ǚ' ],    [ 475, 'Ǜ' ],
        [ 476, 'ǜ' ],    [ 477, 'ǝ' ],    [ 478, 'Ǟ' ],    [ 479, 'ǟ' ],
        [ 480, 'Ǡ' ],    [ 481, 'ǡ' ],    [ 482, 'Ǣ' ],    [ 483, 'ǣ' ],
        [ 484, 'Ǥ' ],    [ 485, 'ǥ' ],    [ 486, 'Ǧ' ],    [ 487, 'ǧ' ],
        [ 488, 'Ǩ' ],    [ 489, 'ǩ' ],    [ 490, 'Ǫ' ],    [ 491, 'ǫ' ],
        [ 492, 'Ǭ' ],    [ 493, 'ǭ' ],    [ 494, 'Ǯ' ],    [ 495, 'ǯ' ],
        [ 496, 'ǰ' ],    [ 497, 'Ǳ' ],    [ 498, 'ǲ' ],    [ 499, 'ǳ' ],
        [ 500, 'Ǵ' ],    [ 501, 'ǵ' ],    [ 502, 'Ƕ' ],    [ 503, 'Ƿ' ],
        [ 504, 'Ǹ' ],    [ 505, 'ǹ' ],    [ 506, 'Ǻ' ],    [ 507, 'ǻ' ],
        [ 508, 'Ǽ' ],    [ 509, 'ǽ' ],    [ 510, 'Ǿ' ],    [ 511, 'ǿ' ],
        [ 512, 'Ȁ' ],    [ 513, 'ȁ' ],    [ 514, 'Ȃ' ],    [ 515, 'ȃ' ],
        [ 516, 'Ȅ' ],    [ 517, 'ȅ' ],    [ 518, 'Ȇ' ],    [ 519, 'ȇ' ],
        [ 520, 'Ȉ' ],    [ 521, 'ȉ' ],    [ 522, 'Ȋ' ],    [ 523, 'ȋ' ],
        [ 524, 'Ȍ' ],    [ 525, 'ȍ' ],    [ 526, 'Ȏ' ],    [ 527, 'ȏ' ],
        [ 528, 'Ȑ' ],    [ 529, 'ȑ' ],    [ 530, 'Ȓ' ],    [ 531, 'ȓ' ],
        [ 532, 'Ȕ' ],    [ 533, 'ȕ' ],    [ 534, 'Ȗ' ],    [ 535, 'ȗ' ],
        [ 536, 'Ș' ],    [ 537, 'ș' ],    [ 538, 'Ț' ],    [ 539, 'ț' ],
        [ 540, 'Ȝ' ],    [ 541, 'ȝ' ],    [ 542, 'Ȟ' ],    [ 543, 'ȟ' ],
        [ 544, 'Ƞ' ],    [ 545, 'ȡ' ],    [ 546, 'Ȣ' ],    [ 547, 'ȣ' ],
        [ 548, 'Ȥ' ],    [ 549, 'ȥ' ],    [ 550, 'Ȧ' ],    [ 551, 'ȧ' ],
        [ 552, 'Ȩ' ],    [ 553, 'ȩ' ],    [ 554, 'Ȫ' ],    [ 555, 'ȫ' ],
        [ 556, 'Ȭ' ],    [ 557, 'ȭ' ],    [ 558, 'Ȯ' ],    [ 559, 'ȯ' ],
        [ 560, 'Ȱ' ],    [ 561, 'ȱ' ],    [ 562, 'Ȳ' ],    [ 563, 'ȳ' ],
        [ 564, 'ȴ' ],    [ 565, 'ȵ' ],    [ 566, 'ȶ' ],    [ 567, 'ȷ' ],
        [ 568, 'ȸ' ],    [ 569, 'ȹ' ],    [ 570, 'Ⱥ' ],    [ 571, 'Ȼ' ],
        [ 572, 'ȼ' ],    [ 573, 'Ƚ' ],    [ 574, 'Ⱦ' ],    [ 575, 'ȿ' ],
        [ 576, 'ɀ' ],    [ 577, 'Ɂ' ],    [ 578, 'ɂ' ],    [ 579, 'Ƀ' ],
        [ 580, 'Ʉ' ],    [ 581, 'Ʌ' ],    [ 582, 'Ɇ' ],    [ 583, 'ɇ' ],
        [ 584, 'Ɉ' ],    [ 585, 'ɉ' ],    [ 586, 'Ɋ' ],    [ 587, 'ɋ' ],
        [ 588, 'Ɍ' ],    [ 589, 'ɍ' ],    [ 590, 'Ɏ' ],    [ 591, 'ɏ' ],
        [ 592, 'ɐ' ],    [ 593, 'ɑ' ],    [ 594, 'ɒ' ],    [ 595, 'ɓ' ],
        [ 596, 'ɔ' ],    [ 597, 'ɕ' ],    [ 598, 'ɖ' ],    [ 599, 'ɗ' ],
        [ 600, 'ɘ' ],    [ 601, 'ə' ],    [ 602, 'ɚ' ],    [ 603, 'ɛ' ],
        [ 604, 'ɜ' ],    [ 605, 'ɝ' ],    [ 606, 'ɞ' ],    [ 607, 'ɟ' ],
        [ 608, 'ɠ' ],    [ 609, 'ɡ' ],    [ 610, 'ɢ' ],    [ 611, 'ɣ' ],
        [ 612, 'ɤ' ],    [ 613, 'ɥ' ],    [ 614, 'ɦ' ],    [ 615, 'ɧ' ],
        [ 616, 'ɨ' ],    [ 617, 'ɩ' ],    [ 618, 'ɪ' ],    [ 619, 'ɫ' ],
        [ 620, 'ɬ' ],    [ 621, 'ɭ' ],    [ 622, 'ɮ' ],    [ 623, 'ɯ' ],
        [ 624, 'ɰ' ],    [ 625, 'ɱ' ],    [ 626, 'ɲ' ],    [ 627, 'ɳ' ],
        [ 628, 'ɴ' ],    [ 629, 'ɵ' ],    [ 630, 'ɶ' ],    [ 631, 'ɷ' ],
        [ 632, 'ɸ' ],    [ 633, 'ɹ' ],    [ 634, 'ɺ' ],    [ 635, 'ɻ' ],
        [ 636, 'ɼ' ],    [ 637, 'ɽ' ],    [ 638, 'ɾ' ],    [ 639, 'ɿ' ],
        [ 640, 'ʀ' ],    [ 641, 'ʁ' ],    [ 642, 'ʂ' ],    [ 643, 'ʃ' ],
        [ 644, 'ʄ' ],    [ 645, 'ʅ' ],    [ 646, 'ʆ' ],    [ 647, 'ʇ' ],
        [ 648, 'ʈ' ],    [ 649, 'ʉ' ],    [ 650, 'ʊ' ],    [ 651, 'ʋ' ],
        [ 652, 'ʌ' ],    [ 653, 'ʍ' ],    [ 654, 'ʎ' ],    [ 655, 'ʏ' ],
        [ 656, 'ʐ' ],    [ 657, 'ʑ' ],    [ 658, 'ʒ' ],    [ 659, 'ʓ' ],
        [ 660, 'ʔ' ],    [ 661, 'ʕ' ],    [ 662, 'ʖ' ],    [ 663, 'ʗ' ],
        [ 664, 'ʘ' ],    [ 665, 'ʙ' ],    [ 666, 'ʚ' ],    [ 667, 'ʛ' ],
        [ 668, 'ʜ' ],    [ 669, 'ʝ' ],    [ 670, 'ʞ' ],    [ 671, 'ʟ' ],
        [ 672, 'ʠ' ],    [ 673, 'ʡ' ],    [ 674, 'ʢ' ],    [ 675, 'ʣ' ],
        [ 676, 'ʤ' ],    [ 677, 'ʥ' ],    [ 678, 'ʦ' ],    [ 679, 'ʧ' ],
        [ 680, 'ʨ' ],    [ 681, 'ʩ' ],    [ 682, 'ʪ' ],    [ 683, 'ʫ' ],
        [ 684, 'ʬ' ],    [ 685, 'ʭ' ],    [ 686, 'ʮ' ],    [ 687, 'ʯ' ],
        [ 688, 'ʰ' ],    [ 689, 'ʱ' ],    [ 690, 'ʲ' ],    [ 691, 'ʳ' ],
        [ 692, 'ʴ' ],    [ 693, 'ʵ' ],    [ 694, 'ʶ' ],    [ 695, 'ʷ' ],
        [ 696, 'ʸ' ],    [ 697, 'ʹ' ],    [ 698, 'ʺ' ],    [ 699, 'ʻ' ],
        [ 700, 'ʼ' ],    [ 701, 'ʽ' ],    [ 702, 'ʾ' ],    [ 703, 'ʿ' ],
        [ 704, 'ˀ' ],    [ 705, 'ˁ' ],    [ 706, '˂' ],    [ 707, '˃' ],
        [ 708, '˄' ],    [ 709, '˅' ],    [ 710, 'ˆ' ],    [ 711, 'ˇ' ],
        [ 712, 'ˈ' ],    [ 713, 'ˉ' ],    [ 714, 'ˊ' ],    [ 715, 'ˋ' ],
        [ 716, 'ˌ' ],    [ 717, 'ˍ' ],    [ 718, 'ˎ' ],    [ 719, 'ˏ' ],
        [ 720, 'ː' ],    [ 721, 'ˑ' ],    [ 722, '˒' ],    [ 723, '˓' ],
        [ 724, '˔' ],    [ 725, '˕' ],    [ 726, '˖' ],    [ 727, '˗' ],
        [ 728, '˘' ],    [ 729, '˙' ],    [ 730, '˚' ],    [ 731, '˛' ],
        [ 732, '˜' ],    [ 733, '˝' ],    [ 734, '˞' ],    [ 735, '˟' ],
        [ 736, 'ˠ' ],    [ 737, 'ˡ' ],    [ 738, 'ˢ' ],    [ 739, 'ˣ' ],
        [ 740, 'ˤ' ],    [ 741, '˥' ],    [ 742, '˦' ],    [ 743, '˧' ],
        [ 744, '˨' ],    [ 745, '˩' ],    [ 746, '˪' ],    [ 747, '˫' ],
        [ 748, 'ˬ' ],    [ 749, '˭' ],    [ 750, 'ˮ' ],    [ 751, '˯' ],
        [ 752, '˰' ],    [ 753, '˱' ],    [ 754, '˲' ],    [ 755, '˳' ],
        [ 756, '˴' ],    [ 757, '˵' ],    [ 758, '˶' ],    [ 759, '˷' ],
        [ 760, '˸' ],    [ 761, '˹' ],    [ 762, '˺' ],    [ 763, '˻' ],
        [ 764, '˼' ],    [ 765, '˽' ],    [ 766, '˾' ],    [ 767, '˿' ],
        [ 768, '̀' ],     [ 769, '́' ],     [ 770, '̂' ],     [ 771, '̃' ],
        [ 772, '̄' ],     [ 773, '̅' ],     [ 774, '̆' ],     [ 775, '̇' ],
        [ 776, '̈' ],     [ 777, '̉' ],     [ 778, '̊' ],     [ 779, '̋' ],
        [ 780, '̌' ],     [ 781, '̍' ],     [ 782, '̎' ],     [ 783, '̏' ],
        [ 784, '̐' ],     [ 785, '̑' ],     [ 786, '̒' ],     [ 787, '̓' ],
        [ 788, '̔' ],     [ 789, '̕' ],     [ 790, '̖' ],     [ 791, '̗' ],
        [ 792, '̘' ],     [ 793, '̙' ],     [ 794, '̚' ],     [ 795, '̛' ],
        [ 796, '̜' ],     [ 797, '̝' ],     [ 798, '̞' ],     [ 799, '̟' ],
        [ 800, '̠' ],     [ 801, '̡' ],     [ 802, '̢' ],     [ 803, '̣' ],
        [ 804, '̤' ],     [ 805, '̥' ],     [ 806, '̦' ],     [ 807, '̧' ],
        [ 808, '̨' ],     [ 809, '̩' ],     [ 810, '̪' ],     [ 811, '̫' ],
        [ 812, '̬' ],     [ 813, '̭' ],     [ 814, '̮' ],     [ 815, '̯' ],
        [ 816, '̰' ],     [ 817, '̱' ],     [ 818, '̲' ],     [ 819, '̳' ],
        [ 820, '̴' ],     [ 821, '̵' ],     [ 822, '̶' ],     [ 823, '̷' ],
        [ 824, '̸' ],     [ 825, '̹' ],     [ 826, '̺' ],     [ 827, '̻' ],
        [ 828, '̼' ],     [ 829, '̽' ],     [ 830, '̾' ],     [ 831, '̿' ],
        [ 832, '̀' ],     [ 833, '́' ],     [ 834, '͂' ],     [ 835, '̓' ],
        [ 836, '̈́' ],     [ 837, 'ͅ' ],     [ 838, '͆' ],     [ 839, '͇' ],
        [ 840, '͈' ],     [ 841, '͉' ],     [ 842, '͊' ],     [ 843, '͋' ],
        [ 844, '͌' ],     [ 845, '͍' ],     [ 846, '͎' ],     [ 847, '͏' ],
        [ 848, '͐' ],     [ 849, '͑' ],     [ 850, '͒' ],     [ 851, '͓' ],
        [ 852, '͔' ],     [ 853, '͕' ],     [ 854, '͖' ],     [ 855, '͗' ],
        [ 856, '͘' ],     [ 857, '͙' ],     [ 858, '͚' ],     [ 859, '͛' ],
        [ 860, '͜' ],     [ 861, '͝' ],     [ 862, '͞' ],     [ 863, '͟' ],
        [ 864, '͠' ],     [ 865, '͡' ],     [ 866, '͢' ],     [ 867, 'ͣ' ],
        [ 868, 'ͤ' ],     [ 869, 'ͥ' ],     [ 870, 'ͦ' ],     [ 871, 'ͧ' ],
        [ 872, 'ͨ' ],     [ 873, 'ͩ' ],     [ 874, 'ͪ' ],     [ 875, 'ͫ' ],
        [ 876, 'ͬ' ],     [ 877, 'ͭ' ],     [ 878, 'ͮ' ],     [ 879, 'ͯ' ],
        [ 880, 'Ͱ' ],    [ 881, 'ͱ' ],    [ 882, 'Ͳ' ],    [ 883, 'ͳ' ],
        [ 884, 'ʹ' ],    [ 885, '͵' ],    [ 886, 'Ͷ' ],    [ 887, 'ͷ' ],
        [ 900, '΄' ],    [ 901, '΅' ],    [ 902, 'Ά' ],    [ 903, '·' ],
        [ 912, 'ΐ' ],    [ 913, 'Α' ],    [ 914, 'Β' ],    [ 915, 'Γ' ],
        [ 916, 'Δ' ],    [ 917, 'Ε' ],    [ 918, 'Ζ' ],    [ 919, 'Η' ],
        [ 920, 'Θ' ],    [ 921, 'Ι' ],    [ 922, 'Κ' ],    [ 923, 'Λ' ],
        [ 924, 'Μ' ],    [ 925, 'Ν' ],    [ 926, 'Ξ' ],    [ 927, 'Ο' ],
        [ 932, 'Τ' ],    [ 933, 'Υ' ],    [ 934, 'Φ' ],    [ 935, 'Χ' ],
        [ 936, 'Ψ' ],    [ 937, 'Ω' ],    [ 938, 'Ϊ' ],    [ 939, 'Ϋ' ],
        [ 940, 'ά' ],    [ 941, 'έ' ],    [ 942, 'ή' ],    [ 943, 'ί' ],
        [ 944, 'ΰ' ],    [ 945, 'α' ],    [ 946, 'β' ],    [ 947, 'γ' ],
        [ 948, 'δ' ],    [ 949, 'ε' ],    [ 950, 'ζ' ],    [ 951, 'η' ],
        [ 952, 'θ' ],    [ 953, 'ι' ],    [ 954, 'κ' ],    [ 955, 'λ' ],
        [ 956, 'μ' ],    [ 957, 'ν' ],    [ 958, 'ξ' ],    [ 959, 'ο' ],
        [ 960, 'π' ],    [ 961, 'ρ' ],    [ 962, 'ς' ],    [ 963, 'σ' ],
        [ 964, 'τ' ],    [ 965, 'υ' ],    [ 966, 'φ' ],    [ 967, 'χ' ],
        [ 968, 'ψ' ],    [ 969, 'ω' ],    [ 970, 'ϊ' ],    [ 971, 'ϋ' ],
        [ 972, 'ό' ],    [ 973, 'ύ' ],    [ 974, 'ώ' ],    [ 975, 'Ϗ' ],
        [ 976, 'ϐ' ],    [ 977, 'ϑ' ],    [ 978, 'ϒ' ],    [ 979, 'ϓ' ],
        [ 980, 'ϔ' ],    [ 981, 'ϕ' ],    [ 982, 'ϖ' ],    [ 983, 'ϗ' ],
        [ 984, 'Ϙ' ],    [ 985, 'ϙ' ],    [ 986, 'Ϛ' ],    [ 987, 'ϛ' ],
        [ 988, 'Ϝ' ],    [ 989, 'ϝ' ],    [ 990, 'Ϟ' ],    [ 991, 'ϟ' ],
        [ 992, 'Ϡ' ],    [ 993, 'ϡ' ],    [ 994, 'Ϣ' ],    [ 995, 'ϣ' ],
        [ 996, 'Ϥ' ],    [ 997, 'ϥ' ],    [ 998, 'Ϧ' ],    [ 999, 'ϧ' ],
        [ 1000, 'Ϩ' ],   [ 1001, 'ϩ' ],   [ 1002, 'Ϫ' ],   [ 1003, 'ϫ' ],
        [ 1004, 'Ϭ' ],   [ 1005, 'ϭ' ],   [ 1006, 'Ϯ' ],   [ 1007, 'ϯ' ],
        [ 1008, 'ϰ' ],   [ 1009, 'ϱ' ],   [ 1010, 'ϲ' ],   [ 1011, 'ϳ' ],
        [ 1012, 'ϴ' ],   [ 1013, 'ϵ' ],   [ 1014, '϶' ],   [ 1015, 'Ϸ' ],
        [ 1016, 'ϸ' ],   [ 1017, 'Ϲ' ],   [ 1018, 'Ϻ' ],   [ 1019, 'ϻ' ],
        [ 1020, 'ϼ' ],   [ 1021, 'Ͻ' ],   [ 1022, 'Ͼ' ],   [ 1023, 'Ͽ' ],

        [ 12340, '〴' ], [ 12463, 'ク' ], [ 12586, 'ㄪ' ], [ 12709, 'ㆥ' ],
        [ 12832, '㈠' ], [ 12955, '㊛' ], [ 13078, '㌖' ], [ 13201, '㎑' ],
        [ 13324, '㐌' ], [ 13447, '㒇' ], [ 13570, '㔂' ], [ 13693, '㕽' ],
        [ 13816, '㗸' ], [ 13939, '㙳' ], [ 14062, '㛮' ], [ 14185, '㝩' ],
        [ 14308, '㟤' ], [ 14431, '㡟' ], [ 14554, '㣚' ], [ 14677, '㥕' ],
        [ 14800, '㧐' ], [ 14923, '㩋' ], [ 15046, '㫆' ], [ 15169, '㭁' ],
        [ 15292, '㮼' ], [ 15415, '㰷' ], [ 15538, '㲲' ], [ 15661, '㴭' ],
        [ 15784, '㶨' ], [ 15907, '㸣' ], [ 16030, '㺞' ], [ 16153, '㼙' ],
        [ 16276, '㾔' ], [ 16399, '䀏' ], [ 16522, '䂊' ], [ 16645, '䄅' ],
        [ 16768, '䆀' ], [ 16891, '䇻' ], [ 17014, '䉶' ], [ 17137, '䋱' ],
        [ 17260, '䍬' ], [ 17383, '䏧' ], [ 17506, '䑢' ], [ 17629, '䓝' ],
        [ 17752, '䕘' ], [ 17875, '䗓' ], [ 17998, '䙎' ], [ 18121, '䛉' ],
        [ 18244, '䝄' ], [ 18367, '䞿' ], [ 18490, '䠺' ], [ 18613, '䢵' ],
        [ 18736, '䤰' ], [ 18859, '䦫' ], [ 18982, '䨦' ], [ 19105, '䪡' ],
        [ 19228, '䬜' ], [ 19351, '䮗' ], [ 19474, '䰒' ], [ 19597, '䲍' ],
        [ 19720, '䴈' ], [ 19843, '䶃' ], [ 19966, '䷾' ],  [ 20089, '乹' ],
        [ 20212, '仴' ], [ 20335, '佯' ], [ 20458, '俪' ], [ 20581, '健' ],
        [ 20704, '僠' ], [ 20827, '兛' ], [ 20950, '凖' ], [ 21073, '剑' ],
        [ 21196, '勌' ], [ 21319, '升' ], [ 21442, '参' ], [ 21565, '吽' ],
        [ 21688, '咸' ], [ 21811, '唳' ], [ 21934, '單' ], [ 22057, '嘩' ],
        [ 22180, '嚤' ], [ 22303, '土' ], [ 22426, '垚' ], [ 22549, '堕' ],
        [ 22672, '墐' ], [ 22795, '夋' ], [ 22918, '妆' ], [ 23041, '威' ],
        [ 23164, '婼' ], [ 23287, '嫷' ], [ 23410, '孲' ], [ 23533, '寭' ],
        [ 23656, '屨' ], [ 23779, '峣' ], [ 23902, '嵞' ], [ 24025, '巙' ],
        [ 24148, '幔' ], [ 24271, '廏' ], [ 24394, '彊' ], [ 24517, '必' ],
        [ 24640, '恀' ], [ 24763, '悻' ], [ 24886, '愶' ], [ 25009, '憱' ],
        [ 25132, '戬' ], [ 25255, '抧' ], [ 25378, '挢' ], [ 25501, '掝' ],
        [ 25624, '搘' ], [ 25747, '撓' ], [ 25870, '攎' ], [ 25993, '斉' ],
        [ 26116, '昄' ], [ 26239, '晿' ], [ 26362, '曺' ], [ 26485, '杵' ],
        [ 26608, '柰' ], [ 26731, '桫' ], [ 26854, '棦' ], [ 26977, '楡' ],
        [ 27100, '槜' ], [ 27223, '橗' ], [ 27346, '櫒' ], [ 27469, '歍' ],
        [ 27592, '毈' ], [ 27715, '汃' ], [ 27838, '沾' ], [ 27961, '洹' ],
        [ 28084, '涴' ], [ 28207, '港' ], [ 28330, '溪' ], [ 28453, '漥' ],
        [ 28576, '澠' ], [ 28699, '瀛' ], [ 28822, '炖' ], [ 28945, '焑' ],
        [ 29068, '熌' ], [ 29191, '爇' ], [ 29314, '犂' ], [ 29437, '狽' ],
        [ 29560, '獸' ], [ 29683, '珳' ], [ 29806, '瑮' ], [ 29929, '瓩' ],
        [ 30052, '畤' ], [ 30175, '痟' ], [ 30298, '癚' ], [ 30421, '盕' ],
        [ 30544, '睐' ], [ 30667, '矋' ], [ 30790, '硆' ], [ 30913, '磁' ],
        [ 31036, '礼' ], [ 31159, '禷' ], [ 31282, '稲' ], [ 31405, '窭' ],
        [ 31528, '笨' ], [ 31651, '箣' ], [ 31774, '簞' ], [ 31897, '粙' ],
        [ 32020, '純' ], [ 32143, '綏' ], [ 32266, '縊' ], [ 32389, '纅' ],
        [ 32512, '缀' ], [ 32635, '罻' ], [ 32758, '翶' ],
        
      ];

      values.forEach((v) => {
        assertEqual([ v[1] ], getQueryResults('RETURN TO_CHAR(' + v[0] + ')'), v);
      });
    },
    
    testToCharInvalidInputs: function () {
      ["piff", "paff", -1, -1000, null].forEach((v) => {
        const query = 'RETURN TO_CHAR(' + JSON.stringify(v) + ')';
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, query);
      });
    },
    
    testToCharInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_CHAR()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_CHAR(1, 2)');
    },
    
// //////////////////////////////////////////////////////////////////////////////
// / @brief test to_base64
// //////////////////////////////////////////////////////////////////////////////

    testToBase64Values: function () {
      [ 
        [ null, "" ],
        [ -13, "LTEz" ],
        [ 10, "MTA="],
        [ true, "dHJ1ZQ==" ],
        [ false, "ZmFsc2U=" ],
        [ "", "" ],
        [ "foobar", "Zm9vYmFy" ],
        [ " ", "IA==" ],
        [ "The quick brown fox jumps over the lazy dog", "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==" ],
      ].forEach(function(test) {
        assertEqual([ test[1] ], getQueryResults('RETURN TO_BASE64(' + JSON.stringify(test[0]) + ')'), test);
      });
    },
    
    testToBase64InvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_BASE64()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_BASE64("test", "meow")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_BASE64("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_BASE64("test", "meow", "foo", "bar")');
    },
    
// //////////////////////////////////////////////////////////////////////////////
// / @brief test tohex
// //////////////////////////////////////////////////////////////////////////////

    testToHexValues: function () {
      [ 
        [ null, "" ],
        [ -13, "2d3133" ],
        [ 10, "3130"],
        [ true, "74727565" ],
        [ false, "66616c7365" ],
        [ "", "" ],
        [ "foobar", "666f6f626172" ],
        [ " ", "20" ],
        [ "The quick brown fox jumps over the lazy dog", "54686520717569636b2062726f776e20666f78206a756d7073206f76657220746865206c617a7920646f67"],
      ].forEach(function(test) {
        assertEqual([ test[1] ], getQueryResults('RETURN TO_HEX(' + JSON.stringify(test[0]) + ')'), test);
      });
    },
    
    testToHexInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_HEX()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_HEX("test", "meow")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_HEX("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN TO_HEX("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test encodeURIcomponent
// //////////////////////////////////////////////////////////////////////////////

    testEncodeURIComponentValues: function () {
      [ 
        [ null, "" ],
        [ -13, "-13" ],
        [ 10, "10"],
        [ true, "true" ],
        [ false, "false" ],
        [ "", "" ],
        [ "foobar", "foobar" ],
        [ " ", "%20" ],
        [ "?x=шеллы", "%3Fx%3D%D1%88%D0%B5%D0%BB%D0%BB%D1%8B"],
        [ "?x=test", "%3Fx%3Dtest"],
        [ "The quick brown fox jumps over the lazy dog", "The%20quick%20brown%20fox%20jumps%20over%20the%20lazy%20dog"],
        [ "https://w3schools.com/my test.asp?name=ståle&car=saab", "https%3A%2F%2Fw3schools.com%2Fmy%20test.asp%3Fname%3Dst%C3%A5le%26car%3Dsaab"],
      ].forEach(function(test) {
        assertEqual([ test[1] ], getQueryResults('RETURN ENCODE_URI_COMPONENT(' + JSON.stringify(test[0]) + ')'), test);
      });
    },
    
    testEncodeURIComponentInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN ENCODE_URI_COMPONENT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN ENCODE_URI_COMPONENT("test", "meow")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN ENCODE_URI_COMPONENT("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN ENCODE_URI_COMPONENT("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test UUID
// //////////////////////////////////////////////////////////////////////////////

    testUUIDValues: function () {
      assertMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/, getQueryResults('RETURN UUID()'));
    },

    testUUIDInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN UUID("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN UUID("test", "meow")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN UUID("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN UUID("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test Soundex
// //////////////////////////////////////////////////////////////////////////////
    
    testToSoundexValues: function () {
      [ 
        [ null, "" ],
        [ "a", "A000" ],
        [ "ab", "A100" ],
        [ "text", "T230" ],
        [ "tixt", "T230"],
        [ "Text", "T230" ],
        [ "Tixt", "T230"],
        [ "tExT", "T230" ],
        [ "tIxT", "T230"],
        [ true, "T600" ],
        [ false, "F420" ],
        [ "", "" ],
        [ " ", ""],
        [ "\n", ""],
        [ "         ", "" ],
        [ "    foobar", "F160" ],
        [ "foobar      ", "F160" ],
        [ "      foobar      ", "F160" ],
        [ "foobar", "F160" ],
        [ "SOUNDEX", "S532" ],
        [ "SOUNTEKS", "S532" ],
        [ "mötör", "M360" ],
        [ "2m2ö2t2ö2r2", "M360" ],
        [ "Öööööö", "" ],
        [ "The quick brown fox jumps over the lazy dog", "T221"],
      ].forEach(function(test) {
        assertEqual([ test[1] ], getQueryResults('RETURN SOUNDEX(' + JSON.stringify(test[0]) + ')'), test);
      });
    },

    testSoundexInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SOUNDEX()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SOUNDEX("test", "meow")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SOUNDEX("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SOUNDEX("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test LevenshteinDistance
// //////////////////////////////////////////////////////////////////////////////

    testToLevenshteinDistanceValues: function () {
      [ 
        [ null, "", 0 ],
        [ null, null, 0 ],
        [ "", "", 0 ],
        [ "", "foobar", 6 ],
        [ "foobar", "", 6 ],
        [ "foobar", "foo", 3 ],
        [ "foo", "foobar", 3 ],
        [ "or", "of", 1 ],
        [ "or", "", 2 ],
        [ "or", "The quick brown fox jumps over the lazy dog", 41 ],
        [ true, "foobar", 6 ],
        [ false, "foobar", 5 ],
        [ "foobar", true, 6 ],
        [ "foobar", false, 5 ],
        [ true, true, 0 ],
        [ false, false, 0 ],
        [ true, false, 4 ],
        [ false, true, 4 ],
        [ "", "", 0 ],
        [ " ", "", 1 ],
        [ "", " ", 1 ],
        [ "der mötör trötet", "der mötör trötet", 0 ],
        [ "der mötör trötet", "der trötet", 6 ],
        [ "der mötör trötet", "dertrötet", 7 ],
        [ "Öööööö", "öö", 4 ],
        [ "The quick brown fox jumps over the lazy dog", "The quick black dog jumps over the brown fox", 13 ],
        [ "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of \"de Finibus Bonorum et Malorum\" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, 'Lorem ipsum dolor sit amet..', comes from a line in section 1.10.32.  The standard chunk of Lorem Ipsum used since the 1500s is reproduced below for those interested. Sections 1.10.32 and 1.10.33 from 'de Finibus Bonorum et Malorum' by Cicero are also reproduced in their exact original form, accompanied by English versions from the 1914 translation by H. Rackham..", "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of \"de Finibus Bonorum et Malorum\" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, 'Lorem ipsum dolor sit amet..', comes from a line in section 1.10.32.  The standard chunk of Lorem Ipsum used since the 1500s is reproduced below for those interested. Sections 1.10.32 and 1.10.33 from 'de Finibus Bonorum et Malorum' by Cicero are also reproduced in their exact original form.", 74 ],
      ].forEach(function(test) {
        assertEqual([ test[2] ], getQueryResults('RETURN LEVENSHTEIN_DISTANCE(' + JSON.stringify(test[0]) + ', ' + JSON.stringify(test[1]) + ')'), test);
      });
    },

    testLevenshteinDistanceInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LEVENSHTEIN_DISTANCE()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LEVENSHTEIN_DISTANCE("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LEVENSHTEIN_DISTANCE("test", "meow", "foo")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LEVENSHTEIN_DISTANCE("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test JSON_STRINGIFY
// //////////////////////////////////////////////////////////////////////////////

    testJsonStringify: function () {
      assertEqual([ 'null' ], getQueryResults("RETURN JSON_STRINGIFY(null)"));
      assertEqual([ 'false' ], getQueryResults("RETURN JSON_STRINGIFY(false)"));
      assertEqual([ 'true' ], getQueryResults("RETURN JSON_STRINGIFY(true)"));
      assertEqual([ '-19.3' ], getQueryResults("RETURN JSON_STRINGIFY(-19.3)"));
      assertEqual([ '0' ], getQueryResults("RETURN JSON_STRINGIFY(0)"));
      assertEqual([ '0' ], getQueryResults("RETURN JSON_STRINGIFY(0.0)"));
      assertEqual([ '0.1' ], getQueryResults("RETURN JSON_STRINGIFY(0.1)"));
      assertEqual([ '100' ], getQueryResults("RETURN JSON_STRINGIFY(100)"));
      assertEqual([ '10001434.2' ], getQueryResults("RETURN JSON_STRINGIFY(10001434.2)"));
      assertEqual([ '"foo bar"' ], getQueryResults(`RETURN JSON_STRINGIFY('foo bar')`));
      assertEqual([ '"foo \\" bar"' ], getQueryResults(`RETURN JSON_STRINGIFY('foo \\" bar')`));
      assertEqual([ '[]' ], getQueryResults("RETURN JSON_STRINGIFY([ ])"));
      assertEqual([ '[1,2,3,4]' ], getQueryResults("RETURN JSON_STRINGIFY([ 1, 2, 3, 4 ])"));
      assertEqual([ '[[1,2,3,4]]' ], getQueryResults("RETURN JSON_STRINGIFY([ [ 1, 2, 3, 4  ] ])"));
      assertEqual([ '{}' ], getQueryResults("RETURN JSON_STRINGIFY({ })"));
      assertEqual([ '{"a":1,"b":2}' ], getQueryResults("RETURN JSON_STRINGIFY({ a: 1, b : 2     })"));
      assertEqual([ '{"A":2,"a":1}' ], getQueryResults("RETURN JSON_STRINGIFY({ A: 2, a: 1 })"));
      assertEqual([ '{"a":1,"b":"foo","c":null}' ], getQueryResults(`RETURN JSON_STRINGIFY({ "a": 1, "b": "foo", c: null })`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test JSON_PARSE
// //////////////////////////////////////////////////////////////////////////////

    testJsonParse: function () {
      // invalid JSON
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE(null)"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE(false)"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE(true)"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE(1234)"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE(' ')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('abcd')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE([])"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE({})"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('[')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('[]]')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('{')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('{}}')"));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('{a}')"));
      assertEqual([ null ], getQueryResults(`RETURN JSON_PARSE('{\\"a\\"}')`));
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('{a:1}')"));

      // valid JSON
      assertEqual([ null ], getQueryResults("RETURN JSON_PARSE('null')"));
      assertEqual([ false ], getQueryResults("RETURN JSON_PARSE('false')"));
      assertEqual([ true ], getQueryResults("RETURN JSON_PARSE('true')"));
      assertEqual([ -19.3 ], getQueryResults("RETURN JSON_PARSE('-19.3')"));
      assertEqual([ 0 ], getQueryResults("RETURN JSON_PARSE('0')"));
      assertEqual([ 0 ], getQueryResults("RETURN JSON_PARSE('0.0')"));
      assertEqual([ 0.1 ], getQueryResults("RETURN JSON_PARSE('0.1')"));
      assertEqual([ 100 ], getQueryResults("RETURN JSON_PARSE('100')"));
      assertEqual([ 10001434.2 ], getQueryResults("RETURN JSON_PARSE('10001434.2')"));
      assertEqual([ 'foo bar' ], getQueryResults(`RETURN JSON_PARSE('\\"foo bar\\"')`));
      assertEqual([ [] ], getQueryResults("RETURN JSON_PARSE('[]')"));
      assertEqual([ [1, 2, 3, 4] ], getQueryResults("RETURN JSON_PARSE('[ 1, 2, 3, 4 ]')"));
      assertEqual([ [[1, 2, 3, 4]] ], getQueryResults("RETURN JSON_PARSE('[ [ 1, 2, 3, 4  ] ]')"));
      assertEqual([ {} ], getQueryResults("RETURN JSON_PARSE('{ }')"));
      assertEqual([ { a: 1, b: 2 } ], getQueryResults(`RETURN JSON_PARSE('{ \\"a\\": 1, \\"b\\" : 2     }')`));
      assertEqual([ { A: 2, a: 1 } ], getQueryResults(`RETURN JSON_PARSE('{ \\"A\\": 2, \\"a\\": 1 }')`));
      assertEqual([ { a: 1, b: 'foo', c: null } ], getQueryResults(`RETURN JSON_PARSE('{ \\"a\\": 1, \\"b\\": \\"foo\\", \\"c\\": null }')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test regex function, invalid arguments
// //////////////////////////////////////////////////////////////////////////////

    testRegexInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_TEST()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_TEST("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_TEST("test", "meow", "foo", "bar")');

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "[")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "[^")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "a.(")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "(a")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "(a]")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "**")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "?")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_TEST("test", "*")');
    },

    testRegex: function () {
      var values = [
        // empty values
        ['', '', true],
        ['', '^', true],
        ['', '$', true],
        ['', '^$', true],
        ['', '^.*$', true],
        ['', '^.+$', false],
        ['', '.', false],
        ['', '.?', true],
        [' ', '.?', true],
        [' ', '.+', true],
        [' ', '.$', true],
        [' ', '^.', true],
        [' ', '.$', true],
        [' ', '^.$', true],
        [' ', '^.{1,}$', true],
        [' ', '^.{2,}$', false],

        // whole words
        ['the quick brown fox', 'the', true],
        ['the quick brown fox', 'quick', true],
        ['the quick brown fox', 'quicK', false],
        ['the quick brown fox', 'quIcK', false],
        ['the quick brown fox', 'brown', true],
        ['the quick brown fox', 'fox', true],
        ['the quick brown fox', 'The', false],
        ['the quick brown fox', 'THE', false],
        ['the quick brown fox', 'foxx', false],
        ['the quick brown fox', 'hasi', false],
        ['the quick brown fox', 'number', false],

        // anchored
        ['the quick brown fox', '^the', true],
        ['the quick brown fox', '^the$', false],
        ['the quick brown fox', '^the quick', true],
        ['the quick brown fox', '^the quick brown', true],
        ['the quick brown fox', '^the quick brown fo', true],
        ['the quick brown fox', '^th', true],
        ['the quick brown fox', '^t', true],
        ['the quick brown fox', '^the quick$', false],
        ['the quick brown fox', '^quick', false],
        ['the quick brown fox', 'quick$', false],
        ['the quick brown fox', '^quick$', false],
        ['the quick brown fox', '^brown', false],
        ['the quick brown fox', 'brown$', false],
        ['the quick brown fox', '^brown$', false],
        ['the quick brown fox', 'fox', true],
        ['the quick brown fox', 'fox$', true],
        ['the quick brown fox', '^fox$', false],
        ['the quick brown fox', 'The', false],
        ['the quick brown fox', '^The', false],
        ['the quick brown fox', 'THE', false],
        ['the quick brown fox', '^THE', false],
        ['the quick brown fox', 'foxx', false],
        ['the quick brown fox', 'foxx$', false],
        ['the quick brown fox', 'the quick brown fox$', true],
        ['the quick brown fox', 'brown fox$', true],
        ['the quick brown fox', 'quick brown fox$', true],
        ['the quick brown fox', 'he quick brown fox$', true],
        ['the quick brown fox', 'e quick brown fox$', true],
        ['the quick brown fox', 'quick brown fox$', true],
        ['the quick brown fox', 'x$', true],
        ['the quick brown fox', '^', true],
        ['the quick brown fox', '$', true],
        ['the quick brown fox', '^.*$', true],
        ['the quick brown fox', '.*', true],
        ['the quick brown fox', '^.*', true],
        ['the quick brown fox', '^.*$', true],

        // partials
        ['the quick brown fox', ' quick', true],
        ['the quick brown fox', ' Quick', false],
        ['the quick brown fox', 'the quick', true],
        ['the quick brown fox', 'the slow', false],
        ['the quick brown fox', 'the quick brown', true],
        ['the quick brown fox', 'the quick browne', false],
        ['the quick brown fox', 'the quick brownfox', false],
        ['the quick brown fox', 'the quick brown fox', true],
        ['the quick brown fox', 'the quick brown foxx', false],
        ['the quick brown fox', 'quick brown fox', true],
        ['the quick brown fox', 'a quick brown fox', false],
        ['the quick brown fox', 'brown fox', true],
        ['the quick brown fox', 'rown fox', true],
        ['the quick brown fox', 'rown f', true],
        ['the quick brown fox', 'e q', true],
        ['the quick brown fox', 'f z', false],
        ['the quick brown fox', 'red fo', false],
        ['the quick brown fox', 'köter', false],
        ['the quick brown fox', 'ö', false],
        ['the quick brown fox', 'z', false],
        ['the quick brown fox', 'z', false],
        ['the quick brown fox', ' ', true],
        ['the quick brown fox', '  ', false],
        ['the quick brown fox', '', true],
        ['the quick brown fox', 'number one', false],
        ['the quick brown fox', '123', false],

        // wildcards
        ['the quick brown fox', 'the.*fox', true],
        ['the quick brown fox', '^the.*fox$', true],
        ['the quick brown fox', '^the.*dog$', false],
        ['the quick brown fox', 'the (quick|slow) (red|green|brown) (dog|cat|fox)', true],
        ['the quick brown fox', 'the .*(red|green|brown) (dog|cat|fox)', true],
        ['the quick brown fox', '^the .*(red|green|brown) (dog|cat|fox)', true],
        ['the quick brown fox', 'the (quick|slow) (red|green|brown) (dog|cat)', false],
        ['the quick brown fox', '^the (quick|slow) (red|green|brown) (dog|cat)', false],
        ['the quick brown fox', '^the .*(red|green|brown) (dog|cat)', false],
        ['the quick brown fox', 'the .*(red|green|brown) (dog|cat)', false],
        ['the quick brown fox', 'the (slow|lazy) brown (fox|wolf)', false],
        ['the quick brown fox', 'the.*brown (fox|wolf)', true],
        ['the quick brown fox', '^t.*(fox|wolf)', true],
        ['the quick brown fox', '^t.*(fox|wolf)$', true],
        ['the quick brown fox', '^t.*(fo|wolf)x$', true],
        ['the quick brown fox', '^t.*(fo|wolf)xx', false],

        // line breaks
        ['the quick\nbrown\nfox', 'the.*fox', false],
        ['the quick\nbrown\nfox', 'the(.|\n)*fox', true],
        ['the quick\nbrown\nfox', '^the.*fox$', false],
        ['the quick\nbrown\nfox', '^the(.|\n)*fox$', true],
        ['the quick\nbrown\nfox', 'the.*fox$', false],
        ['the quick\nbrown\nfox', 'the(.|\n)*fox$', true],
        ['the quick\nbrown\nfox', '^the.*fox$', false],
        ['the quick\nbrown\nfox', '^the(.|\n)*fox$', true],
        ['the quick\nbrown\nfox', '^the.*dog$', false],
        ['the quick\nbrown\nfox', '^the(.|\n)*dog$', false],
        ['the quick\nbrown\nfox', 'brown fox', false],
        ['the quick\nbrown\nfox', 'brown.fox', false],
        ['the quick\nbrown\nfox', 'brown(.|\n)fox', true],
        ['the quick\nbrown\nfox', 'brown\\nfox', true],
        ['the quick\nbrown\nfox', 'quick.brown', false],
        ['the quick\nbrown\nfox', 'quick(.|\n)brown', true],
        ['the quick\r\nbrown\nfox', 'quick.brown', false],
        ['the quick\r\nbrown\nfox', 'quick(.|\r\n)brown', true],
        ['the quick\r\nbrown\nfox', 'quick\\r\\nbrown', true],
        ['the quick\r\nbrown\nfox', 'quick\\r\\red', false]
      ];

      values.forEach(function (v) {
        var query = 'RETURN REGEX_TEST(@what, @re)';
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);

        query = 'RETURN @what =~ @re';
        assertEqual(v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);

        query = 'RETURN @what !~ @re';
        assertEqual(!v[2], getQueryResults(query, { what: v[0], re: v[1] })[0], v);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test regex function, cache
// //////////////////////////////////////////////////////////////////////////////

    testRegexCache: function () {
      var actual = getQueryResults(`FOR i IN 1..100 RETURN REGEX_TEST(CONCAT('test', i), 'test')`);
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i]);
      }
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test REGEX_REPLACE, invalid arguments
// //////////////////////////////////////////////////////////////////////////////

    testRegexReplaceInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_REPLACE()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_REPLACE("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_TEST("test", "meow", "woof", "foo", "bar")');

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_REPLACE("test", "?", "hello")');
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_REGEX.code, 'RETURN REGEX_REPLACE("test", "*", "hello")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test REGEX_REPLACE
// //////////////////////////////////////////////////////////////////////////////

    testRegexReplace: function () {
      var values = [
        // whole words
        [['the quick brown fox', 'the', 'A'], 'A quick brown fox'],
        [['the quick brown fox', 'quick', 'slow'], 'the slow brown fox'],
        [['the quick brown fox', 'quicK', 'slow'], 'the quick brown fox'],
        [['the quick brown fox', 'brown ', 'white'], 'the quick whitefox'],
        [['the quick brown fox', 'fox', 'dog'], 'the quick brown dog'],
        [['the quick brown fox', 'hasi', 'fasi'], 'the quick brown fox'],

        // anchored
        [['the quick brown fox', '^the', 'A'], 'A quick brown fox'],
        [['the quick brown fox', '^the$', 'A'], 'the quick brown fox'],
        [['the quick brown fox', '^the quick brown fo', 'fa'], 'fax'],

        // // partials
        [['the quick brown fox', 'ö', 'i'], 'the quick brown fox'],
        [['the quick brown fox', 'o', 'i'], 'the quick briwn fix'],
        [['the quick brown fox', ' ', '-'], 'the-quick-brown-fox'],
        [['the quick brown fox', '  ', '-'], 'the quick brown fox'],
        [['the quick brown fox', ' ', ''], 'thequickbrownfox'],
        [['the quick brown fox', '', 'x'], 'xtxhxex xqxuxixcxkx xbxrxoxwxnx xfxoxxx'],

        // // wildcards
        [['the quick brown fox', 'the.*fox', 'jumped over'], 'jumped over'],
        [['the quick brown fox', '^the.*fox$', 'jumped over'], 'jumped over'],
        [['the quick brown fox', '^the.*dog$', 'jumped over'], 'the quick brown fox'],
        [['the quick brown fox', 'the (quick|slow) (red|green|brown) (dog|cat|fox)', 'jumped over'], 'jumped over'],
        [['the quick brown fox', 'the .*(red|green|brown) (dog|cat|fox)', 'jumped over'], 'jumped over'],
        [['the quick brown fox', 'the (slow|lazy) brown (fox|wolf)', 'jumped over'], 'the quick brown fox'],
        [['the quick brown fox', 'the (slow|quick) brown (fox|wolf)', 'jumped over'], 'jumped over'],

        // // line breaks
        [['the quick\nbrown\nfox', 'the(.|\n)*fox', 'jumped over'], 'jumped over'],
        [['the quick\nbrown\nfox', '^the.*fox$', 'jumped over'], 'the quick\nbrown\nfox']
      ];

      values.forEach(function (v) {
        var query;
        query = 'RETURN REGEX_REPLACE(@what, @re, @with)';
        assertEqual(v[1], getQueryResults(query, { what: v[0][0], re: v[0][1], with: v[0][2] })[0], v);
      });
    },
    
  // //////////////////////////////////////////////////////////////////////////////
// / @brief test RegexSplit
// //////////////////////////////////////////////////////////////////////////////
    testToRegexSplitValues: function () {
      [ 
        [ "hypertext language, programming", "[\s, ]+", true, 1, ["hypertext"] ],
        [ "hypertext language, programming", "[\s, ]+", true, 2, ["hypertext", "language"] ],
        [ "hypertext language, programming", "[\s, ]+", true, 3, ["hypertext", "language", "programming"] ],
        [ "this|is|a|text", "[|]", true, 1, ["this"] ],
        [ "this|is|a|text", "[|]", true, 2, ["this", "is"] ],
        [ "this|is|a|text", "[|]", true, 3, ["this", "is", "a"] ],
        [ "this|is|a|text", "[|]", true, 4, ["this", "is", "a", "text"] ],
        [ "ca,bc,a,bca,bca,bc", "a,b", true, 1, ["c"] ],
        [ "ca,bc,a,bca,bca,bc", "a,b", true, 2, ["c", "c,"] ],
        [ "ca,bc,a,bca,bca,bc", "a,b", true, 3, ["c", "c,", "c"] ],
        [ "ca,bc,a,bca,bca,bc", "a,b", true, 4, ["c", "c,", "c", "c"] ],
        [ "ca,bc,a,bca,bca,bc", "a,b", true, 5, ["c", "c,", "c", "c", "c"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 1, ["This is a line"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 2, ["This is a line", "\n"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 3, ["This is a line", "\n", " This is yet another lin"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 4, ["This is a line", "\n", " This is yet another lin", "\r"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 5, ["This is a line", "\n", " This is yet another lin", "\r", ""] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 6, ["This is a line", "\n", " This is yet another lin", "\r", "", "\n"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 7, ["This is a line", "\n", " This is yet another lin", "\r", "", "\n", " This again is a line"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 8, ["This is a line", "\n", " This is yet another lin", "\r", "", "\n", " This again is a line", "\r"] ],
        [ "This is a line.\n This is yet another line\r\n This again is a line.\r Mac line ", "\.?(\n|\r|\r\n)", true, 9, ["This is a line", "\n", " This is yet another lin", "\r", "", "\n", " This again is a line", "\r", " Mac line "] ],
      ].forEach(function(test) {
        assertEqual([ test[4] ], getQueryResults('RETURN REGEX_SPLIT(' + JSON.stringify(test[0]) + ', ' + JSON.stringify(test[1]) + ', ' + JSON.stringify(test[2]) + ', ' + JSON.stringify(test[3]) + ')'), test);
      });
    },

    testRegexSplitInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_SPLIT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_SPLIT("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_SPLIT("test", "meow", "foo", "bar", "git")');
    },


  // //////////////////////////////////////////////////////////////////////////////
// / @brief test RegexMatches
// //////////////////////////////////////////////////////////////////////////////
    testToRegexMatchesValues: function () {
      [ 
        ["my-us3r_n4m3", "^[a-z0-9_-]{3,16}$", true, ["my-us3r_n4m3"] ],
        ["my-us3r_n4m3", "^[a-z0-9_-]{3,16}$", false, ["my-us3r_n4m3"] ],
        ["my-Us3r_N4m3", "^[a-z0-9_-]{3,16}$", true, ["my-Us3r_N4m3"] ],
        ["my-Us3r_N4m3", "^[a-z0-9_-]{3,16}$", false, null ],
        ["th1s1s-wayt00_l0ngt0beausername", "^[a-z0-9_-]{3,16}$", true, null ],
        ["th1s1s-wayt00_l0ngt0beausername", "^[a-z0-9_-]{3,16}$", false, null ],
        ["myp4ssw0rd", "^[a-z0-9_-]{6,18}$", true, ["myp4ssw0rd"] ],
        ["myp4ssw0rd", "^[a-z0-9_-]{6,18}$", false, ["myp4ssw0rd"] ],
        ["myP4ssw0rD", "^[a-z0-9_-]{6,18}$", true, ["myP4ssw0rD"] ],
        ["myP4ssw0rD", "^[a-z0-9_-]{6,18}$", false, null ],
        ["mypa$$w0rd", "^[a-z0-9_-]{6,18}$", true, null ],
        ["mypa$$w0rd", "^[a-z0-9_-]{6,18}$", false, null ],
        ["#a3c113", "^#?([a-f0-9]{6}|[a-f0-9]{3})$", true, ["#a3c113", "a3c113"] ],
        ["#a3c113", "^#?([a-f0-9]{6}|[a-f0-9]{3})$", false, ["#a3c113", "a3c113"] ],
        ["#4d82h4", "^#?([a-f0-9]{6}|[a-f0-9]{3})$", true, null ],
        ["#4d82h4", "^#?([a-f0-9]{6}|[a-f0-9]{3})$", false, null ],
        ["my-title-here", "^[a-z0-9-]+$", true, ["my-title-here"] ],
        ["my-title-here", "^[a-z0-9-]+$", false, ["my-title-here"] ],
        ["my-Title-Here", "^[a-z0-9-]+$", true, ["my-Title-Here"] ],
        ["my-Title-Here", "^[a-z0-9-]+$", false, null ],
        ["john@doe.com", "^([a-z0-9_\.-]+)@([\da-z\.-]+)\.([a-z\.]{2,6})$", true, ["john@doe.com", "john", "doe.", "om"] ],
        ["john@doe.com", "^([a-z0-9_\.-]+)@([\da-z\.-]+)\.([a-z\.]{2,6})$", false, ["john@doe.com", "john", "doe.", "om"] ],
        ["jOhn@doe.com", "^([a-z0-9_\.-]+)@([\da-z\.-]+)\.([a-z\.]{2,6})$", true, ["jOhn@doe.com", "jOhn", "doe.", "om"] ],
        ["jOhn@doe.com", "^([a-z0-9_\.-]+)@([\da-z\.-]+)\.([a-z\.]{2,6})$", false, null ],
        ["http://www.google.com", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", true, ["http://www.google.com", "http://", "www.google.", "om", ""] ],
        ["http://www.google.com", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", false, ["http://www.google.com", "http://", "www.google.", "om", ""] ],
        ["http://www.gOoGle.com", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", true, ["http://www.gOoGle.com", "http://", "www.gOoGle.", "om", ""] ],
        ["http://www.gOoGle.com", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", true, ["http://www.gOoGle.com", "http://", "www.gOoGle.", "om", ""] ],
        ["http://google.com/some/file!.html", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", true, null ],
        ["http://google.com/some/file!.html", "^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$", false, null ],
        ["73.60.124.136", "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$", true, ["73.60.124.136"] ],
        ["73.60.124.136", "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$", false, ["73.60.124.136"] ],
        ["256.60.124.136", "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$", true, null ],
        ["256.60.124.136", "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$", false, null ],
      ].forEach(function(test) {
        assertEqual([ test[3] ], getQueryResults('RETURN REGEX_MATCHES(' + JSON.stringify(test[0]) + ', ' + JSON.stringify(test[1]) + ', ' + JSON.stringify(test[2]) + ')'), test);
      });
    },

    testRegexMatchesInvalidNumberOfParameters: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_MATCHES()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_MATCHES("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN REGEX_MATCHES("test", "meow", "foo", "bar")');
    },
    
// //////////////////////////////////////////////////////////////////////////////
// / @brief test like function, invalid arguments
// //////////////////////////////////////////////////////////////////////////////

    testLikeInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, 'RETURN LIKE');
      assertQueryError(errors.ERROR_QUERY_PARSE.code, 'RETURN "test" LIKE');
      assertQueryError(errors.ERROR_QUERY_PARSE.code, 'RETURN LIKE "test"');
      assertQueryError(errors.ERROR_QUERY_PARSE.code, 'RETURN "test" LIKE "meow", "foo", "bar")');

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LIKE()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LIKE("test")');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LIKE("test", "meow", "foo", "bar")');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test like function
// //////////////////////////////////////////////////////////////////////////////

    testLike: function () {
      // containment
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "test"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "%test"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "test%"'));
      assertEqual([ true ], getQueryResults('RETURN "this is a test string" LIKE "%test%"'));
      assertEqual([ true ], getQueryResults('RETURN "this is a test string" LIKE "this%test%"'));
      assertEqual([ true ], getQueryResults('RETURN "this is a test string" LIKE "this%is%test%"'));
      assertEqual([ true ], getQueryResults('RETURN "this is a test string" LIKE "this%g"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "this%n"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "This%n"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "his%"'));
      assertEqual([ true ], getQueryResults('RETURN "this is a test string" LIKE "%g"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "%G"'));
      assertEqual([ false ], getQueryResults('RETURN "this is a test string" LIKE "this%test%is%"'));

      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "test")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "%test")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "test%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("this is a test string", "%test%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("this is a test string", "this%test%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("this is a test string", "this%is%test%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("this is a test string", "this%g")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "this%n")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "This%n")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "his%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("this is a test string", "%g")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "%G")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("this is a test string", "this%test%is%")'));

      // special characters
      assertEqual([ true ], getQueryResults('RETURN "%" LIKE "\\%"'));
      assertEqual([ true ], getQueryResults('RETURN "a%c" LIKE "a%c"'));
      assertEqual([ false ], getQueryResults('RETURN "a%c" LIKE "ac"'));
      assertEqual([ false ], getQueryResults('RETURN "a%c" LIKE "a\\\\%"'));
      assertEqual([ false ], getQueryResults('RETURN "a%c" LIKE "\\\\%a%"'));
      assertEqual([ false ], getQueryResults('RETURN "a%c" LIKE "\\\\%\\\\%"'));
      assertEqual([ true ], getQueryResults('RETURN "%%" LIKE "\\\\%\\\\%"'));
      assertEqual([ true ], getQueryResults('RETURN "_" LIKE "\\\\_"'));
      assertEqual([ true ], getQueryResults('RETURN "_" LIKE "\\\\_%"'));
      assertEqual([ true ], getQueryResults('RETURN "abcd" LIKE "_bcd"'));
      assertEqual([ true ], getQueryResults('RETURN "abcde" LIKE "_bcd%"'));
      assertEqual([ false ], getQueryResults('RETURN "abcde" LIKE "\\\\_bcd%"'));
      assertEqual([ true ], getQueryResults('RETURN "\\\\abc" LIKE "\\\\\\\\%"'));
      assertEqual([ true ], getQueryResults('RETURN "\\abc" LIKE "\\a%"'));
      assertEqual([ true ], getQueryResults('RETURN "[ ] ( ) % * . + -" LIKE "[%"'));
      assertEqual([ true ], getQueryResults('RETURN "[ ] ( ) % * . + -" LIKE "[ ] ( ) \\% * . + -"'));
      assertEqual([ true ], getQueryResults('RETURN "[ ] ( ) % * . + -" LIKE "%. +%"'));
      assertEqual([ true ], getQueryResults('RETURN "abc^def$g" LIKE "abc^def$g"'));
      assertEqual([ true ], getQueryResults('RETURN "abc^def$g" LIKE "%^%$g"'));
      assertEqual([ true ],  getQueryResults('RETURN "der\*hund"  LIKE "%*%"'));
      assertEqual([ true ],  getQueryResults('RETURN "der*hund"  LIKE "%*%"'));
      assertEqual([ false ], getQueryResults('RETURN "der*hund"  LIKE "*"'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("%", "\\%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("a%c", "a%c")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("a%c", "ac")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("a%c", "a\\\\%")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("a%c", "\\\\%a%")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("a%c", "\\\\%\\\\%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("%%", "\\\\%\\\\%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("_", "\\\\_")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("_", "\\\\_%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("abcd", "_bcd")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("abcde", "_bcd%")'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("abcde", "\\\\_bcd%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("\\\\abc", "\\\\\\\\%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("\\abc", "\\a%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("[ ] ( ) % * . + -", "[%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("[ ] ( ) % * . + -", "[ ] ( ) \\% * . + -")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("[ ] ( ) % * . + -", "%. +%")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("abc^def$g", "abc^def$g")'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("abc^def$g", "%^%$g")'));

      // case-sensitivity
      assertEqual([ false ], getQueryResults('RETURN "ABCD" LIKE "abcd"'));
      assertEqual([ false ], getQueryResults('RETURN "abcd" LIKE "ABCD"'));
      assertEqual([ false ], getQueryResults('RETURN "MÖterTräNenMÜtterSöhne" LIKE "MÖTERTRÄNENMÜTTERSÖHNE"'));

      assertEqual([ false ], getQueryResults('RETURN LIKE("ABCD", "abcd", false)'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("ABCD", "abcd", true)'));
      assertEqual([ false ], getQueryResults('RETURN LIKE("abcd", "ABCD", false)'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("abcd", "ABCD", true)'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("MÖterTräNenMÜtterSöhne", "MöterTräNenMütterSöhne", true)'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("MÖterTräNenMÜtterSöhne", "mötertränenmüttersöhne", true)'));
      assertEqual([ true ], getQueryResults('RETURN LIKE("MÖterTräNenMÜtterSöhne", "MÖTERTRÄNENMÜTTERSÖHNE", true)'));

      assertEqual([ [ true, false, true, false ] ], getQueryResults('RETURN [ LIKE("Möter", "m_ter", true), LIKE("Möter", "m_ter", false), LIKE("Möter", "m_ter", true), LIKE("Möter", "m_ter", false) ]'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test like with special characters
// //////////////////////////////////////////////////////////////////////////////

    testLikeSpecialChars: function () {
      var data = [
        'the quick\nbrown fox jumped over\r\nthe lazy dog',
        `'the "\\quick\\\n "brown\\\rfox' jumped`,
        `'the fox'' jumped \\over the \newline \roof'`
      ];

      data.forEach(function (value) {
        var actual = getQueryResults('RETURN LIKE(' + JSON.stringify(value) + `, 'foobar')`);
        assertEqual([ false ], actual);

        actual = getQueryResults('RETURN LIKE(' + JSON.stringify(value) + ', ' + JSON.stringify(value) + ')');
        assertEqual([ true ], actual);

        actual = getQueryResults('RETURN ' + JSON.stringify(value) + ' LIKE ' + JSON.stringify(value));
        assertEqual([ true ], actual);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test like function, cache
// //////////////////////////////////////////////////////////////////////////////

    testLikeCache: function () {
      var actual = getQueryResults(`FOR i IN 1..100 RETURN LIKE(CONCAT('test', i), 'test%')`);
      assertEqual(100, actual.length);
      for (var i = 0; i < actual.length; ++i) {
        assertTrue(actual[i]);
      }
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test first require function / expected datatype & arg. mismatch
// //////////////////////////////////////////////////////////////////////////////

    testContainsFirst: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONTAINS("test")'); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONTAINS("test", "test", "test", "test")'); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONTAINS()'); 
      assertEqual([ -1 ], getQueryResults('RETURN CONTAINS("test", "test2", "test3")')); 
      assertEqual([ false ], getQueryResults('RETURN CONTAINS(null, null)')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS(4, 4)')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS({ }, { })')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS({ a: 1, b: 2 }, { a: 1, b: 2 })')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS([ ], [ ])')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS([ 1, 2, 3 ], [ 1, 2, 3 ])')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS([ 1, 2 ], [ 1, 2 ])')); 
      assertEqual([ true ], getQueryResults('RETURN CONTAINS([ 1, 2, 3 ], 2)')); 
      assertEqual([ false ], getQueryResults('RETURN CONTAINS(null, "yes")')); 
      assertEqual([ false ], getQueryResults('RETURN CONTAINS(3, null)')); 
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains true1
// //////////////////////////////////////////////////////////////////////////////

    testContainsTrue1: function () {
      assertEqual([ true ], getQueryResults('RETURN CONTAINS("test2", "test")'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains true2
// //////////////////////////////////////////////////////////////////////////////

    testContainsTrue2: function () {
      assertEqual([ true ], getQueryResults('RETURN CONTAINS("xxasdxx", "asd")'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains false1
// //////////////////////////////////////////////////////////////////////////////

    testContainsFalse1: function () {
      assertEqual([ false ], getQueryResults('RETURN CONTAINS("test", "test2")'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains false2
// //////////////////////////////////////////////////////////////////////////////

    testContainsFalse2: function () {
      assertEqual([ false ], getQueryResults('RETURN CONTAINS("test123", "")'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contanis false3
// //////////////////////////////////////////////////////////////////////////////

    testContainsFalse3: function () {
      assertEqual([ false ], getQueryResults('RETURN CONTAINS("", "test123")'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains indexed
// //////////////////////////////////////////////////////////////////////////////

    testContainsIndexed: function () {
      assertEqual([ 0 ], getQueryResults(`RETURN CONTAINS("test2", "test", true)`));
      assertEqual([ true ], getQueryResults(`RETURN CONTAINS("test2", "test", false)`));
      assertEqual([ 1 ], getQueryResults(`RETURN CONTAINS("test2", "est", true)`));
      assertEqual([ true ], getQueryResults(`RETURN CONTAINS("test2", "est", false)`));
      assertEqual([ -1 ], getQueryResults(`RETURN CONTAINS("this is a long test", "this is a test", true)`));
      assertEqual([ false ], getQueryResults(`RETURN CONTAINS("this is a long test", "this is a test", false)`));
      assertEqual([ 18 ], getQueryResults(`RETURN CONTAINS("this is a test of this test", "this test", true)`));
      assertEqual([ true ], getQueryResults(`RETURN CONTAINS("this is a test of this test", "this test", false)`));
      assertEqual([ -1 ], getQueryResults(`RETURN CONTAINS("this is a test of this test", "This", true)`));
      assertEqual([ false ], getQueryResults(`RETURN CONTAINS("this is a test of this test", "This", false)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test contains with special characters
// //////////////////////////////////////////////////////////////////////////////

    testContainsSpecialChars: function () {
      var data = [
        'the quick\nbrown fox jumped over\r\nthe lazy dog',
        `'the "\\quick\\\n "brown\\\rfox' jumped`,
        `'the fox'' jumped \\over the \newline \roof'`
      ];

      data.forEach(function (value) {
        var actual = getQueryResults(`RETURN CONTAINS(` + JSON.stringify(value) + `, 'foobar', false)`);
        assertEqual([ false ], actual);

        actual = getQueryResults(`RETURN CONTAINS(` + JSON.stringify(value) + ', ' + JSON.stringify(value) + ', false)');
        assertEqual([ true ], actual);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test left function
// //////////////////////////////////////////////////////////////////////////////

    testLeft: function () {
      assertEqual(['cp', 'f', '', 'foo', 'foo', '', '', '', 'mö', 'mötö', '😰2🤓'],
      getQueryResults(`FOR t IN [ [ 'cpp', 2 ], [ 'foo', 1 ], [ 'foo', 0 ], [ 'foo', 4 ], [ 'foo', 999999999 ], [ '', 0 ], [ '', 1 ], [ '', 2 ], [ 'mötör', 2 ], [ 'mötör', 4 ], ['😰2🤓4🤡6😎8', 3] ] RETURN LEFT(t[0], t[1])`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test left function
// //////////////////////////////////////////////////////////////////////////////

    testLeftInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LEFT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN LEFT('foo')`);
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN LEFT('foo', 2, 3)`);
      assertEqual([ '' ], getQueryResults(`RETURN LEFT(null, 1)`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT(null, 2)`));
      assertEqual([ 'tr' ], getQueryResults(`RETURN LEFT(true, 2)`));
      assertEqual([ '4' ], getQueryResults(`RETURN LEFT(4, 2)`));
      assertEqual([ '[]' ], getQueryResults(`RETURN LEFT([ ], 2)`));
      assertEqual([ '[' ], getQueryResults(`RETURN LEFT([ ], 1)`));
      assertEqual([ '{}' ], getQueryResults(`RETURN LEFT({ }, 2)`));
      assertEqual([ '{' ], getQueryResults(`RETURN LEFT({ }, 1)`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', null)`));
      assertEqual([ 'f' ], getQueryResults(`RETURN LEFT('foo', true)`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', 'bar')`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', [ ])`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', { })`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', -1)`));
      assertEqual([ '' ], getQueryResults(`RETURN LEFT('foo', -1.5)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test right function
// //////////////////////////////////////////////////////////////////////////////

    testRight: function () {
      assertEqual(['pc', 'o', '', 'foo', 'foo', '', '', '', 'ör', 'ötör', '6😎8'],
      getQueryResults(`FOR t IN [ [ 'ppc', 2 ], [ 'foo', 1 ], [ 'foo', 0 ], [ 'foo', 4 ], [ 'foo', 999999999 ], [ '', 0 ], [ '', 1 ], [ '', 2 ], [ 'mötör', 2 ], [ 'mötör', 4 ], ['😰2🤓4🤡6😎8', 3] ] RETURN RIGHT(t[0], t[1])`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test left function
// //////////////////////////////////////////////////////////////////////////////

    testRightInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN RIGHT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN RIGHT('foo')`);
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN RIGHT('foo', 2, 3)`);
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT(null, 1)`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT(null, 2)`));
      assertEqual([ 'ue' ], getQueryResults(`RETURN RIGHT(true, 2)`));
      assertEqual([ '4' ], getQueryResults(`RETURN RIGHT(4, 2)`));
      assertEqual([ '[]' ], getQueryResults(`RETURN RIGHT([ ], 2)`));
      assertEqual([ ']' ], getQueryResults(`RETURN RIGHT([ ], 1)`));
      assertEqual([ '{}' ], getQueryResults(`RETURN RIGHT({ }, 2)`));
      assertEqual([ '}' ], getQueryResults(`RETURN RIGHT({ }, 1)`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', null)`));
      assertEqual([ 'o' ], getQueryResults(`RETURN RIGHT('foo', true)`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', 'bar')`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', [ ])`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', { })`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', -1)`));
      assertEqual([ '' ], getQueryResults(`RETURN RIGHT('foo', -1.5)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test substitute function
// //////////////////////////////////////////////////////////////////////////////

    testSubstitute: function () {
      var values = [
        [ 'the quick brown dog jumped over the lazy foxx', 'the quick brown foxx jumped over the lazy dog', [ 'foxx', 'dog' ], [ 'dog', 'foxx' ] ],
        [ 'the quick brown foxx jumped over the lazy foxx', 'the quick brown foxx jumped over the lazy dog', [ 'foxx', 'dog' ], [ 'foxx', 'foxx' ] ],
        [ 'the quick brown dog jumped over the lazy foxx', 'the quick brown foxx jumped over the lazy dog', { 'foxx': 'dog', 'dog': 'foxx' } ],
        [ 'the quick brown foxx jumped over the lazy foxx', 'the quick brown foxx jumped over the lazy dog', { 'foxx': 'foxx', 'dog': 'foxx' } ],
        [ 'the unspecified unspecified foxx jumped over the unspecified dog', 'the quick brown foxx jumped over the lazy dog', [ 'quick', 'brown', 'lazy' ], 'unspecified' ],
        [ 'the slow yellow foxx jumped over the eager dog', 'the quick brown foxx jumped over the lazy dog', [ 'quick', 'brown', 'lazy' ], [ 'slow', 'yellow', 'eager' ] ],
        [ 'the empty  foxx jumped over the  dog', 'the quick brown foxx jumped over the lazy dog', [ 'quick', 'brown', 'lazy' ], [ 'empty' ] ],
        [ 'the empty empty foxx jumped over the empty dog', 'the quick brown foxx jumped over the lazy dog', [ 'quick', 'brown', 'lazy' ], 'empty' ],
        [ 'the quick brown foxx jumped over the empty. dog', 'the quick brown foxx jumped over the lazy dog', [ 'dog', 'lazy' ], 'empty.', 1 ],
        [ 'the.quick.brown.foxx.jumped.over.the.lazy\tdog', 'the\r\nquick\r\nbrown\r\nfoxx\r\njumped\r\nover\r\nthe\r\nlazy\tdog', '\r\n', '.' ],
        [ 'the.quick.brown.foxx.jumped\r\nover\r\nthe\r\nlazy dog', 'the\r\nquick\r\nbrown\r\nfoxx\r\njumped\r\nover\r\nthe\r\nlazy dog', '\r\n', '.', 4 ],
        [ 'A capital foxx escaped!', 'the quick brown foxx jumped over the lazy dog', [ 'the quick brown', 'jumped over the lazy dog' ], [ 'A capital', 'escaped!' ] ],
        [ 'a quick brown foxx jumped over a lazy dog', 'the quick brown foxx jumped over the lazy dog', 'the', 'a' ],
        [ 'a quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', 'the', 'a', 1 ],
        [ 'the quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', 'the', 'a', 0 ],
        [ 'a quick brown foxx jumped over a lazy dog', 'the quick brown foxx jumped over the lazy dog', [ 'the' ], [ 'a' ] ],
        [ 'a quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', [ 'the' ], [ 'a' ], 1 ],
        [ 'the quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', 'thisIsNotThere', 'a'],
        [ 'the quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', {'thisIsNotThere': 'a'}],
        [ 'the quick brown foxx jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', ['thisIsNotThere', 'thisIsAlsoNotThere'], ['a', 'b'], 0 ],
        [ 'mötör quick brown mötör jumped over the lazy dog', 'the quick brown foxx jumped over the lazy dog', [ 'over', 'the', 'foxx' ], 'mötör', 2 ],
        [ 'apfela', 'rabarbara', ['barbara', 'rabarbar'], ['petra', 'apfel']],
        [ 'rapetra', 'rabarbara', ['barbara', 'bar'], ['petra', 'foo']],
        [ 'AbCdEF', 'aBcDef', { a: 'A', B: 'b', c: 'C', D: 'd', e: 'E', f: 'F' } ],
        [ 'AbcDef', 'aBcDef', { a: 'A', B: 'b', c: 'C', D: 'd', e: 'E', f: 'F' }, 2 ],
        [ 'aBcDef', 'aBcDef', { a: 'A', B: 'b', c: 'C', D: 'd', e: 'E', f: 'F' }, 0 ],
        [ 'xxxxyyyyzzz', 'aaaabbbbccc', [ 'a', 'b', 'c' ], [ 'x', 'y', 'z' ] ],
        [ 'xxaabbbbccc', 'aaaabbbbccc', [ 'a', 'b', 'c' ], [ 'x', 'y', 'z' ], 2 ],
        [ 'xxxxyybbccc', 'aaaabbbbccc', [ 'a', 'b', 'c' ], [ 'x', 'y', 'z' ], 6 ],
        [ 'aaaayyybccc', 'aaaabbbbccc', [ 'A', 'b', 'c' ], [ 'x', 'y', 'z' ], 3 ],
        [ 'the quick  foxx', 'the quick brown foxx', 'brown' ],
        [ 'the quick brown foxx', 'the quick brown foxx', [ ] ],
        [ 'the quick  foxx', 'the quick brown foxx', [ 'brown' ], [ ] ],
        [ 'the   ant', 'the quick brown foxx', [ 'quick', 'brown', 'foxx' ], [ '', null, 'ant' ] ], 
        [ 'the   ant', 'the quick brown foxx', { quick: '', brown: null, foxx: 'ant' } ],
      ];

      values.forEach(function (value) {
        var expected = value[0];
        var args = [ ];
        var i;
        var n = value.length;
        for (i = 1; i < n; ++i) {
          args.push(JSON.stringify(value[i]));
        }
        var nuResults = getQueryResults('RETURN SUBSTITUTE(' + args.join(', ') + ')');
        assertEqual([ expected ], nuResults, value);
      });
    },
    
// //////////////////////////////////////////////////////////////////////////////
// / @brief test substitute function
// //////////////////////////////////////////////////////////////////////////////

    testSubstituteInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SUBSTITUTE()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SUBSTITUTE('foo', 'bar', 'baz', 2, 2)`);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test split function
// //////////////////////////////////////////////////////////////////////////////

    testSplit: function () {
      var values = [
        [ [ '' ], '', '\n' ],
        [ [ '' ], '', 'foobar' ],
        [ [ ], '', '' ],
        [ [ ], '', [ '' ] ],
        [ [ '' ], '', [ 'a', 'b', 'c' ] ],
        [ [ 'this\nis\na\nt', 'st' ], 'this\nis\na\ntest', 'e' ],
        [ [ 'th', 's\n', 's\na\nt', 'st' ], 'this\nis\na\ntest', [ 'e', 'i' ] ],
        [ [ 'th', '\n', '\na\ntest' ], 'this\nis\na\ntest', 'is' ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\na\ntest', '\n' ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\na\ntest', [ '\n' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\na\ntest', [ '\n', '\r' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\ris\ra\rtest', [ '\n', '\r' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\tis\ta\ttest', [ '\t' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\tis\ta\ttest', '\t' ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\ra\ttest', [ '\n', '\r', '\t' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this is a test', [ ' ' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this is a test', ' ' ],
        [ [ 'this', 'is', 'a', 'test' ], 'this/SEP/is/SEP/a/SEP/test', '/SEP/' ],
        [ [ 'this', 'is', 'a', 'test' ], 'this/SEP/is/SEP/a/SEP/test', [ '/SEP/' ] ],
        [ [ 'this', 'is', 'a', 'test' ], 'this/SEP1/is/SEP2/a/SEP3/test', [ '/SEP1/', '/SEP2/', '/SEP3/' ] ],
        [ [ 'the', 'quick', 'brown', 'foxx' ], 'the quick brown foxx', ' ' ],
        [ [ 'the quick ', ' foxx' ], 'the quick brown foxx', 'brown' ],
        [ [ 't', 'h', 'e', ' ', 'q', 'u', 'i', 'c', 'k', ' ', 'b', 'r', 'o', 'w', 'n', ' ', 'f', 'o', 'x', 'x' ], 'the quick brown foxx', '' ],
        [ [ 't', 'h', 'e', ' ', 'q', 'u', 'i', 'c', 'k', ' ', 'b', 'r', 'o', 'w', 'n', ' ', 'f', 'o', 'x', 'x' ], 'the quick brown foxx', ['', 'ab'] ],
        [ [ 'the quick', 'brown foxx' ], 'the quick()brown foxx', '()' ],
        [ [ 'the quick  brown foxx' ], 'the quick  brown foxx', ' {2}' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '?' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '+' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '*' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '^' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '$' ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', '.' ],
        [ [ 'the quick7brown foxx' ], 'the quick7brown foxx', '\\d' ],
        [ [ 'the quick brown foxx' ], "the quick brown foxx", ["[a-f]"] ],
        [ [ 'the quick brown foxx' ], 'the quick brown foxx', 'u|f' ]
      ];

      values.forEach(function (value) {
        var expected = value[0], text = value[1], separator = value[2];
        var res = getQueryResults('RETURN SPLIT(' + JSON.stringify(text) + ', ' + JSON.stringify(separator) + ')');
        assertEqual([ expected ], res, JSON.stringify(value));
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test split function
// //////////////////////////////////////////////////////////////////////////////

    testSplitMaxLength: function () {
      var values = [
        [ null, 'foobar', '', -1 ],
        [ null, 'foobar', '', -10 ],
        [ [ ], 'foobar', '', 0 ],
        [ [ 'f' ], 'foobar', '', 1 ],
        [ [ ], 'this\nis\na\ntest', '\n', 0 ],
        [ [ 'this' ], 'this\nis\na\ntest', '\n', 1 ],
        [ [ 'this', 'is', 'a' ], 'this\nis\na\ntest', '\n', 3 ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\na\ntest', '\n', 5 ],
        [ [ 'this', 'is', 'a', 'test' ], 'this\nis\na\ntest', '\n', 500 ],
        [ [ 't', 'h', 'i', 's', ' ' ], 'this is a test', '', 5 ]
      ];

      values.forEach(function (value) {
        var expected = value[0], text = value[1], separator = value[2], limit = value[3];
        var res = getQueryResults('RETURN SPLIT(' + JSON.stringify(text) + ', ' + JSON.stringify(separator) + ', ' + JSON.stringify(limit) + ')');
        assertEqual([ expected ], res, JSON.stringify(value));
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test split function
// //////////////////////////////////////////////////////////////////////////////

    testSplitEmpty: function () {
      assertEqual([ [ 'the foxx' ] ], getQueryResults(`RETURN SPLIT('the foxx')`));
      assertEqual([ [ '' ] ], getQueryResults(`RETURN SPLIT('')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test split function
// //////////////////////////////////////////////////////////////////////////////

    testSplitInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SPLIT()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SPLIT('foo', '', 10, '')`);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test trim function
// //////////////////////////////////////////////////////////////////////////////

    testTrim: function () {
      assertEqual([ 'foo',
                  'foo  ',
                  '  foo',
                  '',
                  '',
                  '',
                  'abc',
                  'abc\n\r\t',
                  '\t\r\nabc',
                  'a\rb\nc',
                  'a\rb\nc ',
                  '\ta\rb\nc' ],
                getQueryResults(`FOR t IN [
[ '  foo  ', 0 ],
[ '  foo  ', 1 ],
[ '  foo  ', 2 ],
[ '', 0 ],
[ '', 1 ],
[ '', 2 ],
[ '\t\r\nabc\n\r\t', 0 ],
[ '\t\r\nabc\n\r\t', 1 ],
[ '\t\r\nabc\t\r\n', 2 ],
[ '\ta\rb\nc ', 0 ],
[ '\ta\rb\nc ', 1 ],
[ '\ta\rb\nc ', 2 ]
] RETURN TRIM(t[0], t[1])`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test trim function
// //////////////////////////////////////////////////////////////////////////////

  testTrimSpecial: function () {
    assertEqual([ 'foo',
                '  foo  ',
                '',
                'abc',
                '\t\r\nabc\n\r\t',
                '\r\nabc\t\r',
                'a\rb\n',
                '\rb\n',
                '\ta\rb'
              ],
              getQueryResults(`FOR t IN [
[ '  foo  ', '\r\n\t ' ],
[ '  foo  ', '\r\n\t' ],
[ '', '\r\n\t' ],
[ '\t\r\nabc\n\r\t', '\r\n\t ' ],
[ '\t\r\nabc\n\r\t', '\r\n ' ],
[ '\t\r\nabc\t\r\n', '\t\n' ],
[ '\ta\rb\nc', '\tc' ],
[ '\ta\rb\nc', '\tac' ],
[ '\ta\rb\nc', '\nc' ]
] RETURN TRIM(t[0], t[1])`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test trim function
// //////////////////////////////////////////////////////////////////////////////

  testTrimInvalid: function () {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN TRIM()`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN TRIM('foo', 2, 2)`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN LTRIM('foo', 2, 2)`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN LTRIM()`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN RTRIM('foo', 2, 2)`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN RTRIM()`);

    let all = ['TRIM', 'LTRIM', 'RTRIM'];
    for (let i = 0; i < 3; i++) {
      let WHICH = all[i];
      assertEqual([ '' ], getQueryResults(`RETURN ${WHICH}(null)`));
      assertEqual([ 'true' ], getQueryResults(`RETURN ${WHICH}(true)`));
      assertEqual([ '4' ], getQueryResults(`RETURN ${WHICH}(4)`));
      assertEqual([ '[]' ], getQueryResults(`RETURN ${WHICH}([ ])`));
      assertEqual([ '{}' ], getQueryResults(`RETURN ${WHICH}({ })`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', null)`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', true)`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', 'bar')`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', [ ])`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', { })`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', -1)`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', -1.5)`));
      assertEqual([ 'foo' ], getQueryResults(`RETURN ${WHICH}('foo', 3)`));
    }
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ltrim function
// //////////////////////////////////////////////////////////////////////////////

  testLtrim: function () {
    var expected = [ 'foo  ',
                      'abc\n\r\t',
                      'a\rb\nc ',
                      'This\nis\r\na\ttest\r\n' ];

    var actual = getQueryResults(`FOR t IN [
'  foo  ',
'\t\r\nabc\n\r\t',
'\ta\rb\nc ',
'\r\nThis\nis\r\na\ttest\r\n'
] RETURN LTRIM(t)`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ltrim function
// //////////////////////////////////////////////////////////////////////////////

  testLtrimSpecial1: function () {
    var expected = [ 'foo  ',
                      '\t\r\nabc\n\r\t',
                      '\ta\rb\nc ',
                      'This\nis\r\na\ttest\r\n'
                    ];
    var actual = getQueryResults(`FOR t IN [
'  foo  ',
'\t\r\nabc\n\r\t',
'\ta\rb\nc ',
'\r\nThis\nis\r\na\ttest\r\n'
] RETURN LTRIM(t, '\r \n')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ltrim function
// //////////////////////////////////////////////////////////////////////////////

  testLtrimSpecial2: function () {
    var expected = [ '  foo  ',
                      'a,b,c,d,,e,f,,',
                      'foo,bar,baz\r\n'
                   ];
    var actual = getQueryResults(`FOR t IN [
'  foo  ',
',,,a,b,c,d,,e,f,,',
'foo,bar,baz\r\n'
] RETURN LTRIM(t, ',\n')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test rtrim function
// //////////////////////////////////////////////////////////////////////////////

  testRtrim: function () {
    var expected = [ '', 
                     '', 
                     '',
                     '  foo',
                     '\t\r\nabc',
                     '\ta\rb\nc',
                     '\r\nThis\nis\r\na\ttest'
                   ];
    var actual = getQueryResults(`FOR t IN [
'',
' ',
'   ',
'  foo  ',
'\t\r\nabc\n\r\t',
'\ta\rb\nc ',
'\r\nThis\nis\r\na\ttest\r\n'
] RETURN RTRIM(t)`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ltrim function
// //////////////////////////////////////////////////////////////////////////////

  testRtrimSpecial1: function () {
    var expected = [ '',
                     '',
                     '',
                     '',
                     '\t',
                     '  foo',
                     '\t\r\nabc\n\r\t',
                     '\ta\rb\nc',
                     '\r\nThis\nis\r\na\ttest' ];
    var actual = getQueryResults(`FOR t IN [
'',
'\r',
'\r\n',
' \r\n',
'\t\r\n',
'  foo  ',
'\t\r\nabc\n\r\t',
'\ta\rb\nc ',
'\r\nThis\nis\r\na\ttest\r\n'
] RETURN RTRIM(t, '\r \n')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ltrim function
// //////////////////////////////////////////////////////////////////////////////

  testRtrimSpecial2: function () {
    var expected = [ '  foo  ',
                      ',,,a,b,c,d,,e,f',
                      'foo,bar,baz\r'
                    ];
    var actual = getQueryResults(`FOR t IN [
'  foo  ',
',,,a,b,c,d,,e,f,,',
'foo,bar,baz\r\n'
] RETURN RTRIM(t, ',\n')`);
    assertEqual(expected, actual);
  },

  testRtrimChars: function () {
    var expected = [ '10000', '1000', '100', '10', '1', '', '' ];
    var actual = getQueryResults(`FOR t IN [
'10000x',
'1000x',
'100x',
'10x',
'1x',
'x',
''
] RETURN RTRIM(t, 'x')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_first function
// //////////////////////////////////////////////////////////////////////////////

  testFindFirstEmpty1: function () {
    [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ' ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_FIRST('', ` + JSON.stringify(v) + ')');
      assertEqual([ -1 ], actual);
    });

    var actual = getQueryResults(`RETURN FIND_FIRST('', '')`);
    assertEqual([ 0 ], actual);

    actual = getQueryResults(`RETURN FIND_FIRST('       ', '', 4)`);
    assertEqual([ 4 ], actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_first function
// //////////////////////////////////////////////////////////////////////////////

  testFindFirstEmpty2: function () {
    [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ', '' ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_FIRST(` + JSON.stringify(v) + `, '')`);
      assertEqual([ 0 ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_first function
// //////////////////////////////////////////////////////////////////////////////

  testFindFirst: function () {
    [
      [ -1, 'foo', 'bar' ],
      [ 3, 'foobar', 'bar' ],
      [ 3, 'Foobar', 'bar' ],
      [ -1, 'foobar', 'Bar' ],
      [ 16, 'the quick brown bar jumped over the lazy dog', 'bar' ],
      [ 16, 'the quick brown bar jumped over the lazy dog bar', 'bar' ],
      [ 3, 'FOOBAR', 'BAR' ],
      [ -1, 'FOOBAR', 'bar' ],
      [ -1, 'the quick brown foxx', 'the foxx' ],
      [ 0, 'the quick brown foxx', 'the quick' ],
      [ -1, 'the quick brown foxx', 'the quick brown foxx j' ],
      [ 4, 'the quick brown foxx', 'quick brown' ],
      [ 35, 'the quick brown foxx jumped over a\nnewline', 'newline' ],
      [ 14, 'some linebreak\r\ngoes here', '\r\n' ],
      [ 12, 'foo BAR foo bar', 'bar' ],
      [ 4, 'foo bar foo bar', 'bar' ],
      [ 17, 'Heavy metal from MÖtleyCrÜe or MÖtleyCrÜe doing heavy metal?', 'MÖtleyCrÜe'],
      [ 3, '或或或MÖtleyCrÜe或MÖtleyCrÜe从重金属中提取重金属？',  'MÖtleyCrÜe'],
      [ 10, 'MÖtleyCrÜe或MÖtleyCrÜe从重金属中提取重金属？',  '或']
    ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_FIRST(` + JSON.stringify(v[1]) + ', ' + JSON.stringify(v[2]) + ')');
      assertEqual([ v[0] ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_first function
// //////////////////////////////////////////////////////////////////////////////

  testFindFirstStartEnd: function () {
    [
      [ 3, 'foobar', 'bar', 2 ],
      [ 3, 'foobar', 'bar', 3 ],
      [ -1, 'foobar', 'bar', 4 ],
      [ 3, 'foobar', 'bar', 1, 5 ],
      [ -1, 'foobar', 'bar', 4, 5 ],
      [ -1, 'foobar', 'bar', 1, 4 ],
      [ 3, 'foobar', 'bar', 3 ],
      [ -1, 'foobar', 'bar', 0, 4 ],
      [ 3, 'foobar', 'bar', 0, 5 ],
      [ 3, 'foobar', 'bar', 0, 999 ],
      [ 0, 'the quick brown bar jumped over the lazy dog', 'the', 0 ],
      [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 1 ],
      [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 1 ],
      [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 3 ],
      [ 4, 'the quick brown bar jumped over the lazy dog', 'q', 4 ],
      [ -1, 'the quick brown bar jumped over the lazy dog', 'q', 5 ]
    ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_FIRST(` + JSON.stringify(v[1]) + ', ' + JSON.stringify(v[2]) + ', ' + v[3] + ', ' + (v[4] === undefined ? null : v[4]) + ')');
      assertEqual([ v[0] ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find first function
// //////////////////////////////////////////////////////////////////////////////

  testFindFirstInvalid: function () {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_FIRST()`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_FIRST('foo')`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_FIRST('foo', 'bar', 2, 2, 2)`);
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST(null, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST(true, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST(4, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST([], 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST([ ], 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST({}, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST({ }, 'foo')`));
    assertEqual([ +0 ], getQueryResults(`RETURN FIND_FIRST('foo', null)`));
    assertEqual([ +0 ], getQueryResults(`RETURN FIND_FIRST('foo', '')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', true)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', [])`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', [1,2])`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', { })`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', -1.5)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 3)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 'bar', 'baz')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 'bar', 1, 'bar')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 'bar', -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 'bar', 1, -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_FIRST('foo', 'bar', 1, 0)`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_last function
// //////////////////////////////////////////////////////////////////////////////

  testFindLastEmpty1: function () {
    [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ' ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_LAST('', ` + JSON.stringify(v) + ')');
      assertEqual([ -1 ], actual);
    });

    var actual = getQueryResults(`RETURN FIND_LAST('', '')`);
    assertEqual([ 0 ], actual);
    actual = getQueryResults(`RETURN FIND_LAST('012345', '', 1)`);
    assertEqual([ 6 ], actual);
    actual = getQueryResults(`RETURN FIND_LAST('0123456', '', 1, 4)`);
    assertEqual([ 5 ], actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_last function
// //////////////////////////////////////////////////////////////////////////////

  testFindLastEmpty2: function () {
    [ 'foo', 'bar', 'baz', 'FOO', 'BAR', 'true', ' ', '' ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_LAST(` + JSON.stringify(v) + `, '')`);
      assertEqual([ v.length ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_last function
// //////////////////////////////////////////////////////////////////////////////

  testFindLast: function () {
    [
      [ -1, 'foo', 'bar' ],
      [ 3, 'foobar', 'bar' ],
      [ 3, 'Foobar', 'bar' ],
      [ -1, 'foobar', 'Bar' ],
      [ 16, 'the quick brown bar jumped over the lazy dog', 'bar' ],
      [ 3, 'FOOBAR', 'BAR' ],
      [ -1, 'FOOBAR', 'bar' ],
      [ -1, 'the quick brown foxx', 'the foxx' ],
      [ 0, 'the quick brown foxx', 'the quick' ],
      [ -1, 'the quick brown foxx', 'the quick brown foxx j' ],
      [ 4, 'the quick brown foxx', 'quick brown' ],
      [ 35, 'the quick brown foxx jumped over a\nnewline', 'newline' ],
      [ 14, 'some linebreak\r\ngoes here', '\r\n' ],
      [ 31, 'Heavy metal from MÖtleyCrÜe or MÖtleyCrÜe doing heavy metal?', 'MÖtleyCrÜe'],
      [ 11, 'MÖtleyCrÜe或MÖtleyCrÜe从重金属中提取重金属？',  'MÖtleyCrÜe'],
      [ 10, 'MÖtleyCrÜe或MÖtleyCrÜe从重金属中提取重金属？',  '或']
    ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_LAST(` + JSON.stringify(v[1]) + ', ' + JSON.stringify(v[2]) + ')');
      assertEqual([ v[0] ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_last function
// //////////////////////////////////////////////////////////////////////////////

  testFindLastStartEnd: function () {
    [
      [ 3, 'foobar', 'bar', 0 ],
      [ 3, 'foobar', 'bar', 1 ],
      [ 3, 'foobar', 'bar', 2 ],
      [ 3, 'foobar', 'bar', 3 ],
      [ -1, 'foobar', 'bar', 4 ],
      [ 3, 'foobar', 'bar', 1, 5 ],
      [ 3, 'foobar', 'bar', 2, 5 ],
      [ 3, 'foobar', 'bar', 3, 5 ],
      [ -1, 'foobar', 'bar', 4, 6 ],
      [ -1, 'foobar', 'bar', 4, 5 ],
      [ -1, 'foobar', 'bar', 1, 4 ],
      [ -1, 'foobar', 'bar', 0, 4 ],
      [ 3, 'foobar', 'bar', 0, 5 ],
      [ 3, 'foobar', 'bar', 0, 999 ],
      [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 0 ],
      [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 10 ],
      [ 32, 'the quick brown bar jumped over the lazy dog', 'the', 1 ]
    ].forEach(function (v) {
      var actual = getQueryResults(`RETURN FIND_LAST(` + JSON.stringify(v[1]) + ', ' + JSON.stringify(v[2]) + ', ' + v[3] + ', ' + (v[4] === undefined ? null : v[4]) + ')');
      assertEqual([ v[0] ], actual);
    });
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test find_last function
// //////////////////////////////////////////////////////////////////////////////

  testFindLastInvalid: function () {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_LAST()`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_LAST('foo')`);
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN FIND_LAST('foo', 'bar', 2, 2, 2)`);
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST(null, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST(true, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST(4, 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST([ ], 'foo')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST({ }, 'foo')`));
    assertEqual([ +3 ], getQueryResults(`RETURN FIND_LAST('foo', null)`));
    assertEqual([ +3 ], getQueryResults(`RETURN FIND_LAST('foo', '')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', true)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', [ ])`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', { })`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', -1.5)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 3)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 'bar', 'baz')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 'bar', 1, 'bar')`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 'bar', -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 'bar', 1, -1)`));
    assertEqual([ -1 ], getQueryResults(`RETURN FIND_LAST('foo', 'bar', 1, 0)`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat function
// //////////////////////////////////////////////////////////////////////////////

  testConcat1: function () {
    var expected = [ 'theQuickBrownFoxJumps' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT('the', 'Quick', '', null, 'Brown', null, 'Fox', 'Jumps')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat function
// //////////////////////////////////////////////////////////////////////////////

  testConcat2: function () {
    var expected = [ 'theQuickBrownアボカドJumps名称について' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT('the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat function
// //////////////////////////////////////////////////////////////////////////////

  testConcatList1: function () {
    var expected = [ 'theQuickBrownアボカドJumps名称について' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat function
// //////////////////////////////////////////////////////////////////////////////

  testConcatList2: function () {
    var expected = [ '["the","Quick","",null,"Brown",null,"アボカド","Jumps","名称について"]false' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT([ 'the', 'Quick', '', null, 'Brown', null, 'アボカド', 'Jumps', '名称について' ], null, false)`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat function
// //////////////////////////////////////////////////////////////////////////////

  testConcatInvalid: function () {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONCAT()');
    assertEqual([ 'yestrue' ], getQueryResults('RETURN CONCAT("yes", true)'));
    assertEqual([ 'yes4' ], getQueryResults('RETURN CONCAT("yes", 4)'));
    assertEqual([ 'yes[]' ], getQueryResults('RETURN CONCAT("yes", [ ])'));
    assertEqual([ 'yes{}' ], getQueryResults('RETURN CONCAT("yes", { })'));
    assertEqual([ 'trueyes' ], getQueryResults('RETURN CONCAT(true, "yes")'));
    assertEqual([ '4yes' ], getQueryResults('RETURN CONCAT(4, "yes")'));
    assertEqual([ '[]yes' ], getQueryResults('RETURN CONCAT([ ], "yes")'));
    assertEqual([ '[1,2,3]yes' ], getQueryResults('RETURN CONCAT([ 1,2,3], "yes")'));
    assertEqual([ '[1,2,3]yes' ], getQueryResults('RETURN CONCAT([ 1 , 2, 3 ], "yes")'));
    assertEqual([ '{}yes' ], getQueryResults('RETURN CONCAT({ }, "yes")'));
    assertEqual([ '{"a":"foo","b":2}yes' ], getQueryResults('RETURN CONCAT({ a: "foo", b: 2 }, "yes")'));
    assertEqual([ '[1,"Quick"]["Brown"]falseFox' ], getQueryResults(`RETURN CONCAT([ 1, 'Quick' ], '', null, [ 'Brown' ], false, 'Fox')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparator1: function () {
    var expected = [ 'the,Quick,Brown,Fox,Jumps' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR(',', 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparator2: function () {
    var expected = [ 'the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps')`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorList1: function () {
    var expected = [ '["the","Quick",null,"Brown",null,"Fox","Jumps"],higher,["than","you"]' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR(',', [ 'the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps' ], 'higher', [ 'than', 'you' ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorList2: function () {
    var expected = [ '["the","Quick",null,"Brown","","Fox","Jumps"]*/*/[]*/*/higher*/*/["than","you"]' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ], [ ], 'higher', [ 'than', 'you' ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorList3: function () {
    var expected = [ 'the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/["than","you"]' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorList4: function () {
    var expected = [ 'the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps' ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorList5: function () {
    var expected = [ 'the*/*/Quick*/*/Brown*/*/*/*/Fox*/*/Jumps*/*/[]*/*/higher*/*/["than","you"]' ];
    var actual = getQueryResults(`FOR r IN [ 1 ] return CONCAT_SEPARATOR('*/*/', [ 'the', 'Quick', null, 'Brown', '', 'Fox', 'Jumps', [ ], 'higher', [ 'than', 'you' ] ])`);
    assertEqual(expected, actual);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test concat_separator function
// //////////////////////////////////////////////////////////////////////////////

  testConcatSeparatorInvalid: function () {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONCAT_SEPARATOR()');
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CONCAT_SEPARATOR("yes")');
    assertEqual([ 'yesyes' ], getQueryResults('RETURN CONCAT_SEPARATOR(null, "yes", "yes")'));
    assertEqual([ 'yestrueyes' ], getQueryResults('RETURN CONCAT_SEPARATOR(true, "yes", "yes")'));
    assertEqual([ 'yes4yes' ], getQueryResults('RETURN CONCAT_SEPARATOR(4, "yes", "yes")'));
    assertEqual([ 'yes[]yes' ], getQueryResults('RETURN CONCAT_SEPARATOR([ ], "yes", "yes")'));
    assertEqual([ 'yes{}yes' ], getQueryResults('RETURN CONCAT_SEPARATOR({ }, "yes", "yes")'));
    assertEqual([ 'trueyesyes' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", true, "yes")'));
    assertEqual([ '4yesyes' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", 4, "yes")'));
    assertEqual([ '[]yesyes' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", [ ], "yes")'));
    assertEqual([ '{}yesyes' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", { }, "yes")'));
    assertEqual([ 'yesyestrue' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", "yes", true)'));
    assertEqual([ 'yesyes4' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", "yes", 4)'));
    assertEqual([ 'yesyes[]' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", "yes", [ ])'));
    assertEqual([ 'yesyes[1,2,3]' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", "yes", [ 1,2,3 ])'));
    assertEqual([ 'yesyes{}' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", "yes", { })'));
    assertEqual([ 'yesyes{}' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", [ "yes", { } ])'));
    assertEqual([ '["yes",{}]yestrue' ], getQueryResults('RETURN CONCAT_SEPARATOR("yes", [ "yes", { } ], null, true)'));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test charlength function
// //////////////////////////////////////////////////////////////////////////////

  testCharLength1: () => {
    assertEqual([ 7 ], getQueryResults(`RETURN CHAR_LENGTH('äöüÄÖÜß')`));
    assertEqual([ 10 ], getQueryResults(`RETURN CHAR_LENGTH('アボカド名称について')`));
    assertEqual([ 13 ], getQueryResults(`RETURN CHAR_LENGTH('the quick fox')`));
    assertEqual([ 0 ], getQueryResults(`RETURN CHAR_LENGTH(null)`));
    assertEqual([ 4 ], getQueryResults(`RETURN CHAR_LENGTH(true)`));
    assertEqual([ 5 ], getQueryResults(`RETURN CHAR_LENGTH(false)`));
    assertEqual([ 1 ], getQueryResults(`RETURN CHAR_LENGTH(3)`));
    assertEqual([ 3 ], getQueryResults(`RETURN CHAR_LENGTH(3.3)`));
    assertEqual([ 4 ], getQueryResults(`RETURN CHAR_LENGTH('🥑😄👻👽')`));
    assertEqual([ 2 ], getQueryResults(`RETURN CHAR_LENGTH([ ])`));
    assertEqual([ 7 ], getQueryResults(`RETURN CHAR_LENGTH([ 1, 2, 3 ])`));
    assertEqual([ 2 ], getQueryResults(`RETURN CHAR_LENGTH({ })`));
    assertEqual([ 7 ], getQueryResults(`RETURN CHAR_LENGTH({ a:1 })`));
    assertEqual([ 11 ], getQueryResults(`RETURN CHAR_LENGTH({ a: "foo" })`));
    assertEqual([ 13 ], getQueryResults(`RETURN CHAR_LENGTH({ a:1, b: 2 })`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test charlength function
// //////////////////////////////////////////////////////////////////////////////

  testCharLengthInvalid: () => {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CHAR_LENGTH()');
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN CHAR_LENGTH('yes', 'yes')`);
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test lower function
// //////////////////////////////////////////////////////////////////////////////

  testLower1: () => {
    assertEqual([ 'the quick brown fox jumped' ], getQueryResults(`RETURN LOWER('THE quick Brown foX JuMpED')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test lower function
// //////////////////////////////////////////////////////////////////////////////

  testLower2: () => {
    assertEqual([ 'äöüäöüß アボカド名称について' ], getQueryResults(`RETURN LOWER('äöüÄÖÜß アボカド名称について')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test lower function
// //////////////////////////////////////////////////////////////////////////////

  testLower3: () => {
    assertEqual([ `0123456789<>|,;.:-_#'+*@!"$&/(){[]}?\\` ], getQueryResults(`RETURN LOWER('0123456789<>|,;.:-_#\\'+*@!\\"$&/(){[]}?\\\\')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test lower function
// //////////////////////////////////////////////////////////////////////////////

  testLowerInvalid: () => {
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN LOWER()');
    assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN LOWER('yes', 'yes')`);
    assertEqual([ '' ], getQueryResults('RETURN LOWER(null)'));
    assertEqual([ 'true' ], getQueryResults('RETURN LOWER(true)'));
    assertEqual([ '3' ], getQueryResults('RETURN LOWER(3)'));
    assertEqual([ '[]' ], getQueryResults('RETURN LOWER([])'));
    assertEqual([ '[1,2,3]' ], getQueryResults('RETURN LOWER([1,2,3])'));
    assertEqual([ '{}' ], getQueryResults('RETURN LOWER({})'));
    assertEqual([ '{"a":1,"b":2}' ], getQueryResults('RETURN LOWER({A:1,b:2})'));
    assertEqual([ '{"a":1,"a":2,"b":3}' ], getQueryResults('RETURN LOWER({A:1,a:2,b:3})'));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test upper function
// //////////////////////////////////////////////////////////////////////////////

  testUpper1: () => {
    assertEqual([ 'THE QUICK BROWN FOX JUMPED' ], getQueryResults(`RETURN UPPER('THE quick Brown foX JuMpED')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test upper function
// //////////////////////////////////////////////////////////////////////////////

  testUpper2: () => {
    assertEqual([ 'ÄÖÜÄÖÜSS アボカド名称について' ], getQueryResults(`RETURN UPPER('äöüÄÖÜß アボカド名称について')`));
  },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test upper function
// //////////////////////////////////////////////////////////////////////////////

    testUpperInvalid: () => {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN UPPER()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN UPPER('yes', 'yes')`);
      assertEqual([ '' ], getQueryResults('RETURN UPPER(null)'));
      assertEqual([ 'TRUE' ], getQueryResults('RETURN UPPER(true)'));
      assertEqual([ '3' ], getQueryResults('RETURN UPPER(3)'));
      assertEqual([ '[]' ], getQueryResults('RETURN UPPER([])'));
      assertEqual([ '[1,2,3]' ], getQueryResults('RETURN UPPER([1,2,3])'));
      assertEqual([ '{}' ], getQueryResults('RETURN UPPER({})'));
      assertEqual([ '{"A":1,"B":2}' ], getQueryResults('RETURN UPPER({a:1, b:2})'));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test substring function
// //////////////////////////////////////////////////////////////////////////////

    testSubstring1: function () {
      assertEqual([ 'the' ], getQueryResults(`FOR r IN [ 1 ] RETURN SUBSTRING('the quick brown fox', 0, 3)`));
      assertEqual(['🤡cp'], getQueryResults(`FOR r IN [ 1 ] RETURN SUBSTRING('🤡cpp quick brown fox', 0, 3)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test substring function
// //////////////////////////////////////////////////////////////////////////////

    testSubstring2: function () {
      assertEqual([ 'quick' ], getQueryResults(`FOR r IN [ 1 ] RETURN SUBSTRING('the quick brown fox', 4, 5)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test substring function
// //////////////////////////////////////////////////////////////////////////////

    testSubstring3: function () {
      assertEqual([ 'fox' ], getQueryResults(`RETURN SUBSTRING('the quick brown fox', -3)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test substring function
// //////////////////////////////////////////////////////////////////////////////

    testSubstringInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTRING()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SUBSTRING("yes")`); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SUBSTRING("yes", 0, 2, "yes")`); 
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(null, 0)"));
      assertEqual([ "" ], getQueryResults("RETURN SUBSTRING(null, 1)"));
      assertEqual([ "true" ], getQueryResults("RETURN SUBSTRING(true, 0)"));
      assertEqual([ "3" ], getQueryResults("RETURN SUBSTRING(3, 0)"));
      assertEqual([ "[]" ], getQueryResults("RETURN SUBSTRING([ ], 0)"));
      assertEqual([ "[1,2,3]" ], getQueryResults("RETURN SUBSTRING([ 1, 2, 3 ], 0)"));
      assertEqual([ "2,3]" ], getQueryResults("RETURN SUBSTRING([ 1, 2, 3 ], 3)"));
      assertEqual([ "{}" ], getQueryResults("RETURN SUBSTRING({ }, 0)"));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", null, 0)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", true, 0)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", "yes", 0)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", [ ], 0)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", { }, 0)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", "yes", null)`));
      assertEqual([ "y" ], getQueryResults(`RETURN SUBSTRING("yes", "yes", true)`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", "yes", [ ])`));
      assertEqual([ "" ], getQueryResults(`RETURN SUBSTRING("yes", "yes", { })`));
    },

    testSubstringBytes: function () {
      assertEqual([ '🤡a'  ], getQueryResults(`RETURN SUBSTRING_BYTES('aa🤡aa', 6, 1, 1, 0)`));
      assertEqual([ 'a🤡'  ], getQueryResults(`RETURN SUBSTRING_BYTES('aa🤡aa', 1, 1, 0, 1)`));
      assertEqual([ '💩🤡ユ'  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡💩🤡ユ🤡', 8, 4, 1, 1)`));
      assertEqual([ 'bcx🤡ユf'  ], getQueryResults(`RETURN SUBSTRING_BYTES('abcx🤡ユf🤡', 4, 4, 3, 2)`));
      assertEqual([ 'abcde'  ], getQueryResults(`RETURN SUBSTRING_BYTES('abcde', 2, 1, 10)`));
      assertEqual([ '🤡'  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', 0, 4)`));
      assertEqual([ '🤡'  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', 0)`));
      assertEqual([ '🤡f' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', 0, 5)`));
      assertEqual([ 'fo'  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', 4, 2)`));
      assertEqual([ ''  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', 42, 2)`));
      assertEqual([ '' ], getQueryResults(`RETURN SUBSTRING_BYTES('', 0, 0)`));
      assertEqual([ '' ], getQueryResults(`RETURN SUBSTRING_BYTES('', 0, 1)`));

      // negative offsets
      assertEqual([ 'o' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -1, 1)`));
      assertEqual([ 'vo' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡fvo', -2, 2)`));
      assertEqual([ '🤡foo' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -42)`));
      assertEqual([ '🤡foo' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', 0)`));
      assertEqual([ 'fo' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -3, 2)`));
      assertEqual([ '🤡' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -42, 4)`));
      assertEqual([ '🤡' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', null, 4)`));
      assertEqual([ '🤡' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', 'foobar', 4)`));

      // negative length
      assertEqual([ '' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -2, -2)`));
      assertEqual([ '' ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -2, "foobar")`));

      // invalid utf8 offset
      assertEqual([ null ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', 1, 2)`)); 
      assertEqual([ null ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', 0, 2)`));
      assertEqual([ null ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -4, 2)`));
      assertEqual([ null ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡foo', -42, 2)`));

      // invalid argument types
      assertEqual([ null  ], getQueryResults(`RETURN SUBSTRING_BYTES(['🤡'], 1, 2)`)); 
      assertEqual([ null  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', [1], 2)`)); 
      assertEqual([ null  ], getQueryResults(`RETURN SUBSTRING_BYTES('🤡', 1, [2])`)); 

      // invalid number of arguments
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN SUBSTRING_BYTES()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SUBSTRING_BYTES("yes")`);
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, `RETURN SUBSTRING_BYTES("yes", 0, 2, "yes", 9, 3)`);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test hash function
// //////////////////////////////////////////////////////////////////////////////

    testHash: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HASH()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HASH(1, 2)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN HASH(1, 2, 3)");
      assertEqual([ 675717317264138 ], getQueryResults("RETURN HASH(null)"));
      assertEqual([ 1217335385489389 ], getQueryResults("RETURN HASH(false)"));
      assertEqual([ 57801618404459 ], getQueryResults("RETURN HASH(true)"));
      assertEqual([ 675717317264138 ], getQueryResults("RETURN HASH(1 / 0)"));
      assertEqual([ 2964198978643 ], getQueryResults("RETURN HASH(0)"));
      assertEqual([ 2964198978643 ], getQueryResults("RETURN HASH(0.0)"));
      assertEqual([ 464020872367562 ], getQueryResults("RETURN HASH(0.00001)"));
      assertEqual([ 652971229830707 ], getQueryResults("RETURN HASH(1)"));
      assertEqual([ 510129580600084 ], getQueryResults("RETURN HASH(-1)"));
      assertEqual([ 24372339383975 ], getQueryResults("RETURN HASH(-10.5)"));
      assertEqual([ 1041198105137773 ], getQueryResults("RETURN HASH(123452532322453)"));
      assertEqual([ 876255539722551 ], getQueryResults("RETURN HASH(123452532322454)"));
      assertEqual([ 1277486662998285 ], getQueryResults("RETURN HASH(123452532322454.434)"));
      assertEqual([ 210539478145939 ], getQueryResults("RETURN HASH(-123452532322454)"));
      assertEqual([ 261745517313272 ], getQueryResults("RETURN HASH(-9999999999999.999)"));
      assertEqual([ 441814588996558 ], getQueryResults("RETURN HASH('')"));
      assertEqual([ 1112732548475941 ], getQueryResults("RETURN HASH(' ')"));
      assertEqual([ 246233608921999 ], getQueryResults("RETURN HASH('  ')"));
      assertEqual([ 1542381651001813 ], getQueryResults("RETURN HASH('a')"));
      assertEqual([ 843602980995939 ], getQueryResults("RETURN HASH('A')"));
      assertEqual([ 1618092585478118 ], getQueryResults("RETURN HASH(' a')"));
      assertEqual([ 725364078947946 ], getQueryResults("RETURN HASH(' A')"));
      assertEqual([ 736233736371291 ], getQueryResults("RETURN HASH(' foobar')"));
      assertEqual([ 360657200843601 ], getQueryResults("RETURN HASH('this is a string test. please ignore.')"));
      assertEqual([ 828085160327326 ], getQueryResults("RETURN HASH('this is a string test. please Ignore.')"));
      assertEqual([ 2072438876063292 ], getQueryResults("RETURN HASH('a string is a string is a string of course. even longer strings can be hashed. isn\\'t this fantastic? let\\'s see if we can cross the short-string bounds with it...')"));
      assertEqual([ 181227890622943 ], getQueryResults("RETURN HASH([])"));
      assertEqual([ 346113245898278 ], getQueryResults("RETURN HASH([0])"));
      assertEqual([ 785599515440277 ], getQueryResults("RETURN HASH([1])"));
      assertEqual([ 1295855700045140 ], getQueryResults("RETURN HASH([1,2])"));
      assertEqual([ 1295855700045140 ], getQueryResults("RETURN HASH(1..2)"));
      assertEqual([ 1255602544875390 ], getQueryResults("RETURN HASH([2,1])"));
      assertEqual([ 1255602544875390 ], getQueryResults("RETURN HASH(2..1)"));
      assertEqual([ 1625466870434085 ], getQueryResults("RETURN HASH([1,2,3])"));
      assertEqual([ 1625466870434085 ], getQueryResults("RETURN HASH(1..3)"));
      assertEqual([ 1657598895986170 ], getQueryResults("RETURN HASH([1,2,3,4])"));
      assertEqual([ 1657598895986170 ], getQueryResults("RETURN HASH(1..4)"));
      assertEqual([ 1580543009747638 ], getQueryResults("RETURN HASH([1,2,4,3])"));
      assertEqual([ 157821093310761 ], getQueryResults("RETURN HASH([1,2,3,2])"));
      assertEqual([ 1032992608692014 ], getQueryResults("RETURN HASH([1,2,3,2,1])"));
      assertEqual([ 2051766968908771 ], getQueryResults("RETURN HASH(1..1000)"));
      assertEqual([ 1954991255293719 ], getQueryResults("RETURN HASH({})"));
      assertEqual([ 1294634865089389 ], getQueryResults("RETURN HASH({a:1})"));
      assertEqual([ 1451630758438458 ], getQueryResults("RETURN HASH({a:2})"));
      assertEqual([ 402003666669761 ], getQueryResults("RETURN HASH({a:1,b:1})"));
      assertEqual([ 529935412783457 ], getQueryResults("RETURN HASH({a:1,b:2})"));
      assertEqual([ 402003666669761 ], getQueryResults("RETURN HASH({b:1,a:1})"));
      assertEqual([ 529935412783457 ], getQueryResults("RETURN HASH({b:2,a:1})"));
      assertEqual([ 1363279506864914 ], getQueryResults("RETURN HASH({b:1,a:2})"));
      assertEqual([ 1363279506864914 ], getQueryResults("RETURN HASH({a:2,b:1})"));
      assertEqual([ 1685918180496814 ], getQueryResults("RETURN HASH({a:2,b:'1'})"));
      assertEqual([ 874128984798182 ], getQueryResults("RETURN HASH({a:2,b:null})"));
      assertEqual([ 991653416476703 ], getQueryResults("RETURN HASH({A:1,B:2})"));
      assertEqual([ 502569457877206 ], getQueryResults("RETURN HASH({a:'A',b:'B'})"));
      assertEqual([ 1154380811055928 ], getQueryResults("RETURN HASH({a:'a',b:'b'})"));
      assertEqual([ 416732334603048 ], getQueryResults("RETURN HASH({a:['a'],b:['b']})"));
      assertEqual([ 176300349653218 ], getQueryResults("RETURN HASH({a:1,b:-1})"));
      assertEqual([ 1460607510107728 ], getQueryResults("RETURN HASH({_id:'foo',_key:'bar',_rev:'baz'})"));
      assertEqual([ 1271501175803754 ], getQueryResults("RETURN HASH({_id:'foo',_key:'bar',_rev:'baz',bar:'bark'})"));

      // order does not matter
      assertEqual(getQueryResults("RETURN HASH({a:1,b:2})"), getQueryResults("RETURN HASH({b:2,a:1})"));
      assertNotEqual(getQueryResults("RETURN HASH({a:1,b:2})"), getQueryResults("RETURN HASH({a:2,b:1})"));
      // order matters
      assertNotEqual(getQueryResults("RETURN HASH([1,2,3])"), getQueryResults("RETURN HASH([3,2,1])"));
      // arrays and ranges
      assertEqual(getQueryResults("RETURN HASH([1,2,3])"), getQueryResults("RETURN HASH(1..3)"));
      // arrays and subqueries
      assertEqual(getQueryResults("RETURN HASH([1,2,3])"), getQueryResults("RETURN HASH(FOR i IN [1,2,3] RETURN i)"));
    },
      
// //////////////////////////////////////////////////////////////////////////////
// / @brief test ip4_to_number function
// //////////////////////////////////////////////////////////////////////////////

    testIp4ToNumber: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IPV4_TO_NUMBER()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IPV4_TO_NUMBER("foo", 2)');
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(12)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(12.345)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER(['127.0.0.1'])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER({})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('foobar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('0')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('000')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('000.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('000.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('000.000.000.000')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1.1.256')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1.256.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.256.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('256.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('123456789')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('-1.-1.-1.-1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('12.34.56.789')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('256.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('a1.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1.1.1a')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('a1.1.1.1a')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.1a.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_TO_NUMBER('1.a1.1.1')");
      
      assertEqual([ 0 ], getQueryResults(`RETURN IPV4_TO_NUMBER('0.0.0.0')`));
      assertEqual([ 16843009 ], getQueryResults(`RETURN IPV4_TO_NUMBER('1.1.1.1')`));
      assertEqual([ 167773449 ], getQueryResults(`RETURN IPV4_TO_NUMBER('10.0.5.9')`));
      assertEqual([ 2130706433 ], getQueryResults(`RETURN IPV4_TO_NUMBER('127.0.0.1')`));
      assertEqual([ 3232235520 ], getQueryResults(`RETURN IPV4_TO_NUMBER('192.168.0.0')`));
      assertEqual([ 3232235521 ], getQueryResults(`RETURN IPV4_TO_NUMBER('192.168.0.1')`));
      assertEqual([ 3232235777 ], getQueryResults(`RETURN IPV4_TO_NUMBER('192.168.1.1')`));
      assertEqual([ 3232236033 ], getQueryResults(`RETURN IPV4_TO_NUMBER('192.168.2.1')`));
      assertEqual([ 4278190079 ], getQueryResults(`RETURN IPV4_TO_NUMBER('254.255.255.255')`));
      assertEqual([ 4294967295 ], getQueryResults(`RETURN IPV4_TO_NUMBER('255.255.255.255')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test ip4_from_number function
// //////////////////////////////////////////////////////////////////////////////

    testIp4FromNumber: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IPV4_FROM_NUMBER()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IPV4_FROM_NUMBER("foo", 2)');
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(-1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(-1000)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(4294967296)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(' ')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('0')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('-1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('12345')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('abcd')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER(['127.0.0.1'])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER({})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('foobar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('0')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('123456789')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1.1.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('-1.-1.-1.-1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('12.34.56.789')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('256.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('a1.1.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1.1.1.1a')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('a1.1.1.1a')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1.1a.1.1')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IPV4_FROM_NUMBER('1.a1.1.1')");
      
      assertEqual([ '0.0.0.0' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(0)`));
      assertEqual([ '0.0.0.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(1)`));
      assertEqual([ '0.0.1.0' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(256)`));
      assertEqual([ '0.0.1.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(257)`));
      assertEqual([ '0.1.0.0' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(65536)`));
      assertEqual([ '0.1.0.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(65537)`));
      assertEqual([ '1.1.1.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(16843009)`));
      assertEqual([ '10.0.5.9' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(167773449)`));
      assertEqual([ '127.0.0.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(2130706433)`));
      assertEqual([ '192.168.0.0' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(3232235520)`));
      assertEqual([ '192.168.0.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(3232235521)`));
      assertEqual([ '192.168.1.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(3232235777)`));
      assertEqual([ '192.168.2.1' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(3232236033)`));
      assertEqual([ '254.255.255.255' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(4278190079)`));
      assertEqual([ '255.255.255.255' ], getQueryResults(`RETURN IPV4_FROM_NUMBER(4294967295)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test is_ip4 function
// //////////////////////////////////////////////////////////////////////////////

    testIsIp4: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IS_IPV4()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN IS_IPV4("foo", 2)');
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(null)`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(false)`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(true)`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(12)`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4([])`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(["127.0.0.1"])`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4({})`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(' ')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('foobar')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('0')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('-1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('123456789')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.256')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.256.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.256.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('256.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('00.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.00.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.00.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.00')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('000.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('-1.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.-1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.1.')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('.1.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('12.34.56.789')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('256.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('a1.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.1a')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('a1.1.1.1a')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1a.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.a1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4(' 1.1.1.1')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('1.1.1.1 ')`));

      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('0.0.0.0')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('1.1.1.1')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('8.8.8.8')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('127.0.0.1')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('192.168.0.1')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('255.255.255.255')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('0.0.0.255')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('255.0.0.0')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('0.255.0.0')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('0.0.255.0')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('0.0.0.255')`));
      assertEqual([ true ], getQueryResults(`RETURN IS_IPV4('17.17.17.17')`));
      
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('01.01.01.01')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('000.000.000.000')`));
      assertEqual([ false ], getQueryResults(`RETURN IS_IPV4('001.001.001.001')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test md5 function
// //////////////////////////////////////////////////////////////////////////////

    testMd5: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN MD5()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN MD5("foo", 2)');
      assertEqual([ 'd41d8cd98f00b204e9800998ecf8427e' ], getQueryResults(`RETURN MD5('')`));
      assertEqual([ '7215ee9c7d9dc229d2921a40e899ec5f' ], getQueryResults(`RETURN MD5(' ')`));
      assertEqual([ 'cfcd208495d565ef66e7dff9f98764da' ], getQueryResults(`RETURN MD5('0')`));
      assertEqual([ 'c4ca4238a0b923820dcc509a6f75849b' ], getQueryResults(`RETURN MD5('1')`));
      assertEqual([ '6bb61e3b7bce0931da574d19d1d82c88' ], getQueryResults(`RETURN MD5('-1')`));
      assertEqual([ '0bad51c0b9b2ba77c19bf6bfbbf88dc3' ], getQueryResults(`RETURN MD5(' 0')`));
      assertEqual([ '2e5751b7cfd7f053cd29e946fb2649a4' ], getQueryResults(`RETURN MD5('0 ')`));
      assertEqual([ 'acbd18db4cc2f85cedef654fccc4a4d8' ], getQueryResults(`RETURN MD5('foo')`));
      assertEqual([ '901890a8e9c8cf6d5a1a542b229febff' ], getQueryResults(`RETURN MD5('FOO')`));
      assertEqual([ '1356c67d7ad1638d816bfb822dd2c25d' ], getQueryResults(`RETURN MD5('Foo')`));
      assertEqual([ 'f32a26e2a3a8aa338cd77b6e1263c535' ], getQueryResults(`RETURN MD5('FooBar')`));
      assertEqual([ 'c639efc1e98762233743a75e7798dd9c' ], getQueryResults(`RETURN MD5('This is a test string')`));
      assertEqual([ 'f9a70133b9fe5fa12acd30056bf4aa26' ], getQueryResults(`RETURN MD5('With\r\nLinebreaks\n')`));
      assertEqual([ '1441a7909c087dbbe7ce59881b9df8b9' ], getQueryResults(`RETURN MD5('[object Object]')`));
      assertEqual([ 'cfcd208495d565ef66e7dff9f98764da' ], getQueryResults(`RETURN MD5(0)`));
      assertEqual([ 'c4ca4238a0b923820dcc509a6f75849b' ], getQueryResults(`RETURN MD5(1)`));
      assertEqual([ '6bb61e3b7bce0931da574d19d1d82c88' ], getQueryResults(`RETURN MD5(-1)`));
      assertEqual([ 'd41d8cd98f00b204e9800998ecf8427e' ], getQueryResults(`RETURN MD5(null)`));
      assertEqual([ 'd751713988987e9331980363e24189ce' ], getQueryResults(`RETURN MD5([])`));
      assertEqual([ '8d5162ca104fa7e79fe80fd92bb657fb' ], getQueryResults(`RETURN MD5([0])`));
      assertEqual([ '35dba5d75538a9bbe0b4da4422759a0e' ], getQueryResults(`RETURN MD5('[1]')`));
      assertEqual([ 'f79408e5ca998cd53faf44af31e6eb45' ], getQueryResults(`RETURN MD5([1,2])`));
      assertEqual([ '99914b932bd37a50b983c5e7c90ae93b' ], getQueryResults(`RETURN MD5({ })`));
      assertEqual([ '99914b932bd37a50b983c5e7c90ae93b' ], getQueryResults(`RETURN MD5({})`));
      assertEqual([ '608de49a4600dbb5b173492759792e4a' ], getQueryResults(`RETURN MD5({a:1,b:2})`));
      assertEqual([ 'c7cb8c1df686c0219d540849efe3bce3' ], getQueryResults(`RETURN MD5('[1,2,4,7,11,16,22,29,37,46,56,67,79,92,106,121,137,154,172,191,211,232,254,277,301,326,352,379,407,436,466,497,529,562,596,631,667,704,742,781,821,862,904,947,991,1036,1082,1129,1177,1226,1276,1327,1379,1432,1486,1541,1597,1654,1712,1771,1831,1892,1954,2017,2081,2146,2212,2279,2347,2416,2486,2557,2629,2702,2776,2851,2927,3004,3082,3161,3241,3322,3404,3487,3571,3656,3742,3829,3917,4006,4096,4187,4279,4372,4466,4561,4657,4754,4852,4951]')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test sha1 function
// //////////////////////////////////////////////////////////////////////////////

    testSha1: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA1()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA1("foo", 2)');
      assertEqual([ 'da39a3ee5e6b4b0d3255bfef95601890afd80709' ], getQueryResults(`RETURN SHA1('')`));
      assertEqual([ 'b858cb282617fb0956d960215c8e84d1ccf909c6' ], getQueryResults(`RETURN SHA1(' ')`));
      assertEqual([ 'b6589fc6ab0dc82cf12099d1c2d40ab994e8410c' ], getQueryResults(`RETURN SHA1('0')`));
      assertEqual([ '356a192b7913b04c54574d18c28d46e6395428ab' ], getQueryResults(`RETURN SHA1('1')`));
      assertEqual([ '7984b0a0e139cabadb5afc7756d473fb34d23819' ], getQueryResults(`RETURN SHA1('-1')`));
      assertEqual([ '7ae5a5c19b16f9ee3b00ca36fc729536fb5e7307' ], getQueryResults(`RETURN SHA1(' 0')`));
      assertEqual([ '1ee9183b1f737da4d348ea42281bd1dd682c5d52' ], getQueryResults(`RETURN SHA1('0 ')`));
      assertEqual([ '0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33' ], getQueryResults(`RETURN SHA1('foo')`));
      assertEqual([ 'feab40e1fca77c7360ccca1481bb8ba5f919ce3a' ], getQueryResults(`RETURN SHA1('FOO')`));
      assertEqual([ '201a6b3053cc1422d2c3670b62616221d2290929' ], getQueryResults(`RETURN SHA1('Foo')`));
      assertEqual([ 'eb8fc41f9d9ae5855c4d801355075e4ccfb22808' ], getQueryResults(`RETURN SHA1('FooBar')`));
      assertEqual([ 'e2f67c772368acdeee6a2242c535c6cc28d8e0ed' ], getQueryResults(`RETURN SHA1('This is a test string')`));
      assertEqual([ 'a90b947dd16a53e717451d3c536d445ece647c52' ], getQueryResults(`RETURN SHA1('With\r\nLinebreaks\n')`));
      assertEqual([ '2be88ca4242c76e8253ac62474851065032d6833' ], getQueryResults(`RETURN SHA1('null')`));
      assertEqual([ 'f629ae44b7b3dcfed444d363e626edf411ec69a8' ], getQueryResults(`RETURN SHA1('[1]')`));
      assertEqual([ 'c1d44ff03aff1372856c281854f454e2e1d15b7c' ], getQueryResults(`RETURN SHA1('[object Object]')`));
      assertEqual([ '888227c44807b86059eb36f9fe0fc602a9b16fab' ], getQueryResults(`RETURN SHA1('[1,2,4,7,11,16,22,29,37,46,56,67,79,92,106,121,137,154,172,191,211,232,254,277,301,326,352,379,407,436,466,497,529,562,596,631,667,704,742,781,821,862,904,947,991,1036,1082,1129,1177,1226,1276,1327,1379,1432,1486,1541,1597,1654,1712,1771,1831,1892,1954,2017,2081,2146,2212,2279,2347,2416,2486,2557,2629,2702,2776,2851,2927,3004,3082,3161,3241,3322,3404,3487,3571,3656,3742,3829,3917,4006,4096,4187,4279,4372,4466,4561,4657,4754,4852,4951]')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test sha256 function
// //////////////////////////////////////////////////////////////////////////////

    testSha256: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA256()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA256("foo", 2)');

      assertEqual([ 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855' ], getQueryResults(`RETURN SHA256('')`));
      assertEqual([ '36a9e7f1c95b82ffb99743e0c5c4ce95d83c9a430aac59f84ef3cbfab6145068' ], getQueryResults(`RETURN SHA256(' ')`));
      assertEqual([ '5feceb66ffc86f38d952786c6d696c79c2dbc239dd4e91b46729d73a27fb57e9' ], getQueryResults(`RETURN SHA256('0')`));
      assertEqual([ '6b86b273ff34fce19d6b804eff5a3f5747ada4eaa22f1d49c01e52ddb7875b4b' ], getQueryResults(`RETURN SHA256('1')`));
      assertEqual([ '1bad6b8cf97131fceab8543e81f7757195fbb1d36b376ee994ad1cf17699c464' ], getQueryResults(`RETURN SHA256('-1')`));
      assertEqual([ 'f00d9ca07f97df0310dc4cf04a2eb3a328a0e370b65a73ec10b58d91c2e870d4' ], getQueryResults(`RETURN SHA256(' 0')`));
      assertEqual([ '7daed43814b633951fa277cd01695574df6e05a9cb10523f1763e842b06be0ff' ], getQueryResults(`RETURN SHA256('0 ')`));
      assertEqual([ '2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae' ], getQueryResults(`RETURN SHA256('foo')`));
      assertEqual([ '9520437ce8902eb379a7d8aaa98fc4c94eeb07b6684854868fa6f72bf34b0fd3' ], getQueryResults(`RETURN SHA256('FOO')`));
      assertEqual([ '1cbec737f863e4922cee63cc2ebbfaafcd1cff8b790d8cfd2e6a5d550b648afa' ], getQueryResults(`RETURN SHA256('Foo')`));
      assertEqual([ '0d749abe1377573493e0df74df8d1282e46967754a1ebc7cc6323923a788ad5c' ], getQueryResults(`RETURN SHA256('FooBar')`));
      assertEqual([ '717ac506950da0ccb6404cdd5e7591f72018a20cbca27c8a423e9c9e5626ac61' ], getQueryResults(`RETURN SHA256('This is a test string')`));
      assertEqual([ 'c5b04eef316ae860f267ba80df7c5d4ad2a3be81c4667b2bb93e3ae295ae68f1' ], getQueryResults(`RETURN SHA256('With\r\nLinebreaks\n')`));
      assertEqual([ '74234e98afe7498fb5daf1f36ac2d78acc339464f950703b8c019892f982b90b' ], getQueryResults(`RETURN SHA256('null')`));
      assertEqual([ '080a9ed428559ef602668b4c00f114f1a11c3f6b02a435f0bdc154578e4d7f22' ], getQueryResults(`RETURN SHA256('[1]')`));
      assertEqual([ '65e7dfd119aa5aec1125270d667fd9381365167473273c60f14f78039af6bf73' ], getQueryResults(`RETURN SHA256('[object Object')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test sha512 function
// //////////////////////////////////////////////////////////////////////////////

    testSha512: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA512()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN SHA512("foo", 2)');
      assertEqual([ 'cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e' ], getQueryResults(`RETURN SHA512('')`));
      assertEqual([ 'f90ddd77e400dfe6a3fcf479b00b1ee29e7015c5bb8cd70f5f15b4886cc339275ff553fc8a053f8ddc7324f45168cffaf81f8c3ac93996f6536eef38e5e40768' ], getQueryResults(`RETURN SHA512(' ')`));
      assertEqual([ '31bca02094eb78126a517b206a88c73cfa9ec6f704c7030d18212cace820f025f00bf0ea68dbf3f3a5436ca63b53bf7bf80ad8d5de7d8359d0b7fed9dbc3ab99' ], getQueryResults(`RETURN SHA512('0')`));
      assertEqual([ '4dff4ea340f0a823f15d3f4f01ab62eae0e5da579ccb851f8db9dfe84c58b2b37b89903a740e1ee172da793a6e79d560e5f7f9bd058a12a280433ed6fa46510a' ], getQueryResults(`RETURN SHA512('1')`));
      assertEqual([ '4fcdd8c15addb15f1e994008677c740848168cd8d32e92d44301ea12b37a93fbd9f0a0468d04789e1f387b395509bd3b998e8aad5e02dd2625f0aac661fb1100' ], getQueryResults(`RETURN SHA512('-1')`));
      assertEqual([ '5364d359dbe42bfb870d304e46cfefa1d438779ed425e30a3de0948a3e99594e58dfcca0e0da47e561f0718ffaabfe68f50515d2dca1f715acd7fca6808f87e8' ], getQueryResults(`RETURN SHA512(' 0')`));
      assertEqual([ '3595817cf0e1f1852bc3d279f38df6f899ca963dedd143af810d3c50844a7ca3e0c25be6d3761e9a7010641756110c344ab57e6e5fe3e89a4cb6532705a8c47d' ], getQueryResults(`RETURN SHA512('0 ')`));
      assertEqual([ 'f7fbba6e0636f890e56fbbf3283e524c6fa3204ae298382d624741d0dc6638326e282c41be5e4254d8820772c5518a2c5a8c0c7f7eda19594a7eb539453e1ed7' ], getQueryResults(`RETURN SHA512('foo')`));
      assertEqual([ '9840f9826bba3ddfc3c4872884f51dcbe915a2d42c6a4d0d59ce564e7fe541f15b9a4271554065379709932bc99a71d85f05aacd62457fce5fd131f847de99ec' ], getQueryResults(`RETURN SHA512('FOO')`));
      assertEqual([ '4abcd2639957cb23e33f63d70659b602a5923fafcfd2768ef79b0badea637e5c837161aa101a557a1d4deacbd912189e2bb11bf3c0c0c70ef7797217da7e8207' ], getQueryResults(`RETURN SHA512('Foo')`));
      assertEqual([ '0d6fc00052fee1d3ae1880d5dc59b74dd50449ffdea399449223d2c5b792395ce64153b150fc0fc01bfeed30c7347411cfb8a3b17b51fd8aa6c03acfbcd09e7b' ], getQueryResults(`RETURN SHA512('FooBar')`));
      assertEqual([ 'b8ee69b29956b0b56e26d0a25c6a80713c858cf2902a12962aad08d682345646b2d5f193bbe03997543a9285e5932f34baf2c85c89459f25ba1cf43c4410793c' ], getQueryResults(`RETURN SHA512('This is a test string')`));
      assertEqual([ '80e0e9a3b8b4019546bb30538b4c8934c1afcb98c70a67001ca3a5eb1c243ec217f5e022edfa6aa0afb25a62ac5eef1a87108a3912e015da7bf519d08b4ea704' ], getQueryResults(`RETURN SHA512('With\r\nLinebreaks\n')`));
      assertEqual([ '04f8ff2682604862e405bf88de102ed7710ac45c1205957625e4ee3e5f5a2241e453614acc451345b91bafc88f38804019c7492444595674e94e8cf4be53817f' ], getQueryResults(`RETURN SHA512('null')`));
      assertEqual([ 'cb0b42c73bc373fa2c695ffb4d4c801571f23397aead7c5b793269801e740ed666f83149b1eed6eaab5f07876236f166da5ae3deb8ab52e85dfd9bdd7c3c9432' ], getQueryResults(`RETURN SHA512('[1]')`));
      assertEqual([ '1dccad3fad058a29ccef8e003fa71bbabf587431ac5a55fb36268bf7958c5f3cb31116ac9e855ec61bb9b72ecbd484f704bee032707fb0ead24ad2bee97b9a39' ], getQueryResults(`RETURN SHA512('[object Object]')`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test crc32 function
// //////////////////////////////////////////////////////////////////////////////

    testCrc32: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CRC32()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN CRC32("foo", 2)');
      assertEqual([ '0' ], getQueryResults(`RETURN CRC32('')`));
      assertEqual([ '72C0DD8F' ], getQueryResults(`RETURN CRC32(' ')`));
      assertEqual([ '629E1AE0' ], getQueryResults(`RETURN CRC32('0')`));
      assertEqual([ '90F599E3' ], getQueryResults(`RETURN CRC32('1')`));
      assertEqual([ '89614B3B' ], getQueryResults(`RETURN CRC32('This is a test string')`));
      assertEqual([ 'CC778246' ], getQueryResults(`RETURN CRC32('With\r\nLinebreaks\n')`));
      assertEqual([ '0' ], getQueryResults(`RETURN CRC32(null)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test fnv64 function
// //////////////////////////////////////////////////////////////////////////////

    testFnv64: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN FNV64()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN FNV64("foo", 2)');
      assertEqual([ 'CBF29CE484222325' ], getQueryResults(`RETURN FNV64('')`));
      assertEqual([ 'AF639D4C8601817F' ], getQueryResults(`RETURN FNV64(' ')`));
      assertEqual([ 'AF63AD4C86019CAF' ], getQueryResults(`RETURN FNV64('0')`));
      assertEqual([ 'AF63AC4C86019AFC' ], getQueryResults(`RETURN FNV64('1')`));
      assertEqual([ 'D94D32CC2E5C3409' ], getQueryResults(`RETURN FNV64('This is a test string')`));
      assertEqual([ 'AF870D34E69ABE0A' ], getQueryResults(`RETURN FNV64('With\r\nLinebreaks\n')`));
      assertEqual([ 'CBF29CE484222325' ], getQueryResults(`RETURN FNV64(null)`));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test random_token function
// //////////////////////////////////////////////////////////////////////////////

    testRandomToken: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN RANDOM_TOKEN()');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 'RETURN RANDOM_TOKEN(1, 2)');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 'RETURN RANDOM_TOKEN(-1)');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 'RETURN RANDOM_TOKEN(-10)');
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 'RETURN RANDOM_TOKEN(65537)');

      var actual = getQueryResults('FOR i IN [ 1, 10, 100, 1000, 0 ] RETURN RANDOM_TOKEN(i)');
      assertEqual(5, actual.length);
      assertEqual(1, actual[0].length);
      assertEqual(10, actual[1].length);
      assertEqual(100, actual[2].length);
      assertEqual(1000, actual[3].length);
      assertEqual(0, actual[4].length);
    },
// //////////////////////////////////////////////////////////////////////////////
// / @brief test ngram_similarity function
// //////////////////////////////////////////////////////////////////////////////
    testNGramSimilarityFunction : function() {
      assertQueryWarningAndNull(errors.ERROR_BAD_PARAMETER.code, 
        "RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow', 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY(123, 'Jack Sparow', 4)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow', 123, 4)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow', '4')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_SIMILARITY()");
      {
        let res = getQueryResults("RETURN NGRAM_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow', 2)");
        assertEqual(1, res.length);  
        assertEqual(1, res[0]);
      }
      {
        let res = getQueryResults("RETURN NGRAM_SIMILARITY('abcdefgh', 'abdef', 2)");
        assertEqual(1, res.length);  
        assertEqual(0.75, res[0]);
      }
      {
        let res = getQueryResults("RETURN NGRAM_SIMILARITY('abcd', 'aecf', 2)");
        assertEqual(1, res.length);  
        assertEqual(0, res[0]);
      }
    },
// //////////////////////////////////////////////////////////////////////////////
// / @brief test ngram_positional_similarity function
// //////////////////////////////////////////////////////////////////////////////
    testNGramPositionalSimilarityFunction : function() {
      assertQueryWarningAndNull(errors.ERROR_BAD_PARAMETER.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow', 0)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY(123, 'Jack Sparow', 4)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow', 123, 4)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow', '4')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparow')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
        "RETURN NGRAM_POSITIONAL_SIMILARITY()");
      {
        let res = getQueryResults("RETURN NGRAM_POSITIONAL_SIMILARITY('Capitan Jack Sparrow', 'Jack Sparrow', 5)");
        assertEqual(1, res.length);  
        assertEqual(0.5, res[0]);
      }
      {
        let res = getQueryResults("RETURN NGRAM_POSITIONAL_SIMILARITY('abcdefgh', 'abdef', 2)");
        assertEqual(1, res.length);  
        assertEqual(0.5, res[0]);
      }
      {
        let res = getQueryResults("RETURN NGRAM_POSITIONAL_SIMILARITY('abcd', 'aecf', 2)");
        assertEqual(1, res.length);  
        assertEqual(0.5, res[0]);
      }
      {
        let res = getQueryResults("RETURN NGRAM_POSITIONAL_SIMILARITY('abcd', 'abc', 10)");
        assertEqual(1, res.length);  
        assertEqual(0.75, res[0]);
      }
    },
  };
}

jsunity.run(ahuacatlStringFunctionsTestSuite);

return jsunity.done();

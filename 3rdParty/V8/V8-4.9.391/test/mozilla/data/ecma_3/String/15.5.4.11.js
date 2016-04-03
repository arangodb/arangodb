/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   <x00000000@freenet.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTestfile = '15.5.4.11.js';
var BUGNUMBER = 392378;
var summary = '15.5.4.11 - String.prototype.replace';
var rex, f, a, i;

reportCompare(
  2,
  String.prototype.replace.length,
  "Section 1"
);

reportCompare(
  "321",
  String.prototype.replace.call(123, "123", "321"),
  "Section 2"
);

reportCompare(
  "ok",
  "ok".replace(),
  "Section 3"
);

reportCompare(
  "undefined**",
  "***".replace("*"),
  "Section 4"
);

reportCompare(
  "xnullz",
  "xyz".replace("y", null),
  "Section 5"
);

reportCompare(
  "x123",
  "xyz".replace("yz", 123),
  "Section 6"
);

reportCompare(
  "/x/g/x/g/x/g",
  "xxx".replace(/x/g, /x/g),
  "Section 7"
);

reportCompare(
  "ok",
  "undefined".replace(undefined, "ok"),
  "Section 8"
);

reportCompare(
  "ok",
  "null".replace(null, "ok"),
  "Section 9"
);

reportCompare(
  "ok",
  "123".replace(123, "ok"),
  "Section 10"
);

reportCompare(
  "xzyxyz",
  "xyzxyz".replace("yz", "zy"),
  "Section 11"
);

reportCompare(
  "ok",
  "(xyz)".replace("(xyz)", "ok"),
  "Section 12"
);

reportCompare(
  "*$&yzxyz",
  "xyzxyz".replace("x", "*$$&"),
  "Section 13"
);

reportCompare(
  "xy*z*",
  "xyz".replace("z", "*$&*"),
  "Section 14"
);

reportCompare(
  "xyxyzxyz",
  "xyzxyzxyz".replace("zxy", "$`"),
  "Section 15"
);

reportCompare(
  "zxyzxyzzxyz",
  "xyzxyz".replace("xy", "$'xyz"),
  "Section 16"
);

reportCompare(
  "$",
  "xyzxyz".replace("xyzxyz", "$"),
  "Section 17"
);

reportCompare(
  "x$0$00xyz",
  "xyzxyz".replace("yz", "$0$00"),
  "Section 18"
);

// Result for $1/$01 .. $99 is implementation-defined if searchValue is no
// regular expression. $+ is a non-standard Mozilla extension.

reportCompare(
  "$!$\"$-1$*$#$.$xyz$$",
  "xyzxyz$$".replace("xyz", "$!$\"$-1$*$#$.$"),
  "Section 19"
);

reportCompare(
  "$$$&$$$&$&",
  "$$$&".replace("$$", "$$$$$$&$&$$&"),
  "Section 20"
);

reportCompare(
  "yxx",
  "xxx".replace(/x/, "y"),
  "Section 21"
);

reportCompare(
  "yyy",
  "xxx".replace(/x/g, "y"),
  "Section 22"
);

rex = /x/, rex.lastIndex = 1;
reportCompare(
  "yxx1",
  "xxx".replace(rex, "y") + rex.lastIndex,
  "Section 23"
);

rex = /x/g, rex.lastIndex = 1;
reportCompare(
  "yyy0",
  "xxx".replace(rex, "y") + rex.lastIndex,
  "Section 24"
);

rex = /y/, rex.lastIndex = 1;
reportCompare(
  "xxx1",
  "xxx".replace(rex, "y") + rex.lastIndex,
  "Section 25"
);

rex = /y/g, rex.lastIndex = 1;
reportCompare(
  "xxx0",
  "xxx".replace(rex, "y") + rex.lastIndex,
  "Section 26"
);

rex = /x?/, rex.lastIndex = 1;
reportCompare(
  "(x)xx1",
  "xxx".replace(rex, "($&)") + rex.lastIndex,
  "Section 27"
);

rex = /x?/g, rex.lastIndex = 1;
reportCompare(
  "(x)(x)(x)()0",
  "xxx".replace(rex, "($&)") + rex.lastIndex,
  "Section 28"
);

rex = /y?/, rex.lastIndex = 1;
reportCompare(
  "()xxx1",
  "xxx".replace(rex, "($&)") + rex.lastIndex,
  "Section 29"
);

rex = /y?/g, rex.lastIndex = 1;
reportCompare(
  "()x()x()x()0",
  "xxx".replace(rex, "($&)") + rex.lastIndex,
  "Section 30"
);

reportCompare(
  "xy$0xy$zxy$zxyz$zxyz",
  "xyzxyzxyz".replace(/zxy/, "$0$`$$$&$$$'$"),
  "Section 31"
);

reportCompare(
  "xy$0xy$zxy$zxyz$$0xyzxy$zxy$z$z",
  "xyzxyzxyz".replace(/zxy/g, "$0$`$$$&$$$'$"),
  "Section 32"
);

reportCompare(
  "xyxyxyzxyxyxyz",
  "xyzxyz".replace(/(((x)(y)()()))()()()(z)/g, "$01$2$3$04$5$6$7$8$09$10"),
  "Section 33"
);

rex = RegExp(
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()(y)");
reportCompare(
  "x(y)z",
  "xyz".replace(rex, "($99)"),
  "Section 34"
);

rex = RegExp(
  "()()()()()()()()()(x)" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()(y)");
reportCompare(
  "(x0)z",
  "xyz".replace(rex, "($100)"),
  "Section 35"
);

reportCompare(
  "xyz(XYZ)",
  "xyzXYZ".replace(/XYZ/g, "($&)"),
  "Section 36"
);

reportCompare(
  "(xyz)(XYZ)",
  "xyzXYZ".replace(/xYz/gi, "($&)"),
  "Section 37"
);

reportCompare(
  "xyz\rxyz\n",
  "xyz\rxyz\n".replace(/xyz$/g, "($&)"),
  "Section 38"
);

reportCompare(
  "(xyz)\r(xyz)\n",
  "xyz\rxyz\n".replace(/xyz$/gm, "($&)"),
  "Section 39"
);

f = function () { return "failure" };

reportCompare(
  "ok",
  "ok".replace("x", f),
  "Section 40"
);

reportCompare(
  "ok",
  "ok".replace(/(?=k)ok/, f),
  "Section 41"
);

reportCompare(
  "ok",
  "ok".replace(/(?!)ok/, f),
  "Section 42"
);

reportCompare(
  "ok",
  "ok".replace(/ok(?!$)/, f),
  "Section 43"
);

f = function (sub, offs, str) {
  return ["", sub, typeof sub, offs, typeof offs, str, typeof str, ""]
    .join("|");
};

reportCompare(
  "x|y|string|1|number|xyz|string|z",
  "xyz".replace("y", f),
  "Section 44"
);

reportCompare(
  "x|(y)|string|1|number|x(y)z|string|z",
  "x(y)z".replace("(y)", f),
  "Section 45"
);

reportCompare(
  "x|y*|string|1|number|xy*z|string|z",
  "xy*z".replace("y*", f),
  "Section 46"
);

reportCompare(
  "12|3|string|2|number|12345|string|45",
  String.prototype.replace.call(1.2345e4, 3, f),
  "Section 47"
);

reportCompare(
  "|x|string|0|number|xxx|string|xx",
  "xxx".replace(/^x/g, f),
  "Section 48"
);

reportCompare(
  "xx|x|string|2|number|xxx|string|",
  "xxx".replace(/x$/g, f),
  "Section 49"
);

f = function (sub, paren, offs, str) {
  return ["", sub, typeof sub, paren, typeof paren, offs, typeof offs,
    str, typeof str, ""].join("|");
};

reportCompare(
  "xy|z|string|z|string|2|number|xyz|string|",
  "xyz".replace(/(z)/g, f),
  "Section 50"
);

reportCompare(
  "xyz||string||string|3|number|xyz|string|",
  "xyz".replace(/($)/g, f),
  "Section 51"
);

reportCompare(
  "|xy|string|y|string|0|number|xyz|string|z",
  "xyz".replace(/(?:x)(y)/g, f),
  "Section 52"
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|yz",
  "xyz".replace(/((?=xy)x)/g, f),
  "Section 53"
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|yz",
  "xyz".replace(/(x(?=y))/g, f),
  "Section 54"
);

reportCompare(
  "x|y|string|y|string|1|number|xyz|string|z",
  "xyz".replace(/((?!x)y)/g, f),
  "Section 55"
);

reportCompare(
  "|x|string|x|string|0|number|xyz|string|" +
    "|y|string||undefined|1|number|xyz|string|z",
  "xyz".replace(/y|(x)/g, f),
  "Section 56"
);

reportCompare(
  "xy|z|string||string|2|number|xyz|string|",
  "xyz".replace(/(z?)z/, f),
  "Section 57"
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/(z)?z/, f),
  "Section 58"
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/(z)?\1z/, f),
  "Section 59"
);

reportCompare(
  "xy|z|string||undefined|2|number|xyz|string|",
  "xyz".replace(/\1(z)?z/, f),
  "Section 60"
);

reportCompare(
  "xy|z|string||string|2|number|xyz|string|",
  "xyz".replace(/(z?\1)z/, f),
  "Section 61"
);

f = function (sub, paren1, paren2, offs, str) {
  return ["", sub, typeof sub, paren1, typeof paren1, paren2, typeof paren2,
    offs, typeof offs, str, typeof str, ""].join("|");
};

reportCompare(
  "x|y|string|y|string||undefined|1|number|xyz|string|z",
  "xyz".replace(/(y)(\1)?/, f),
  "Section 62"
);

reportCompare(
  "x|yy|string|y|string|y|string|1|number|xyyz|string|z",
  "xyyz".replace(/(y)(\1)?/g, f),
  "Section 63"
);

reportCompare(
  "x|y|string|y|string||undefined|1|number|xyyz|string|" +
    "|y|string|y|string||undefined|2|number|xyyz|string|z",
  "xyyz".replace(/(y)(\1)??/g, f),
  "Section 64"
);

reportCompare(
  "x|y|string|y|string|y|string|1|number|xyz|string|z",
  "xyz".replace(/(?=(y))(\1)?/, f),
  "Section 65"
);

reportCompare(
  "xyy|z|string||undefined||string|3|number|xyyz|string|",
  "xyyz".replace(/(?!(y)y)(\1)z/, f),
  "Section 66"
);

rex = RegExp(
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()" +
  "()()()()()()()()()()(z)?(y)");
a = ["sub"];
for (i = 1; i <= 102; ++i)
  a[i] = "p" + i;
a[103] = "offs";
a[104] = "str";
a[105] = "return ['', sub, typeof sub, offs, typeof offs, str, typeof str, " +
  "p100, typeof p100, p101, typeof p101, p102, typeof p102, ''].join('|');";
f = Function.apply(null, a);
reportCompare(
  "x|y|string|1|number|xyz|string||string||undefined|y|string|z",
  "xyz".replace(rex, f),
  "Section 67"
);

reportCompare(
  "undefined",
  "".replace(/.*/g, function () {}),
  "Section 68"
);

reportCompare(
  "nullxnullynullznull",
  "xyz".replace(/.??/g, function () { return null; }),
  "Section 69"
);

reportCompare(
  "111",
  "xyz".replace(/./g, function () { return 1; }),
  "Section 70"
);

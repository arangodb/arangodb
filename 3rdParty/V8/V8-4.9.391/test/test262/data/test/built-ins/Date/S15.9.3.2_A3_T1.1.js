// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The [[Class]] property of the newly constructed object
    is set to "Date"
es5id: 15.9.3.2_A3_T1.1
description: Test based on delete prototype.toString
includes:
    - Date_constants.js
---*/

var x1 = new Date(date_1899_end);
if (Object.prototype.toString.call(x1) !== "[object Date]") {
  $ERROR("#1: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x2 = new Date(date_1900_start);
if (Object.prototype.toString.call(x2) !== "[object Date]") {
  $ERROR("#2: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x3 = new Date(date_1969_end);
if (Object.prototype.toString.call(x3) !== "[object Date]") {
  $ERROR("#3: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x4 = new Date(date_1970_start);
if (Object.prototype.toString.call(x4) !== "[object Date]") {
  $ERROR("#4: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x5 = new Date(date_1999_end);
if (Object.prototype.toString.call(x5) !== "[object Date]") {
  $ERROR("#5: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x6 = new Date(date_2000_start);
if (Object.prototype.toString.call(x6) !== "[object Date]") {
  $ERROR("#6: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x7 = new Date(date_2099_end);
if (Object.prototype.toString.call(x7) !== "[object Date]") {
  $ERROR("#7: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x8 = new Date(date_2100_start);
if (Object.prototype.toString.call(x8) !== "[object Date]") {
  $ERROR("#8: The [[Class]] property of the newly constructed object is set to 'Date'");
}

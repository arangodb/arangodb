// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The [[Value]] property of the newly constructed object
    with supplied "undefined" argument should be NaN
es5id: 15.9.3.1_A6_T1
description: 2 arguments, (year, month)
---*/

function DateValue(year, month, date, hours, minutes, seconds, ms){
  return new Date(year, month, date, hours, minutes, seconds, ms).valueOf();
}

if (!isNaN(DateValue(1899, 11))) {
  $ERROR("#1: The value should be NaN");
}

if (!isNaN(DateValue(1899, 12))) {
  $ERROR("#2: The value should be NaN");
}

if (!isNaN(DateValue(1900, 0))) {
  $ERROR("#3: The value should be NaN");
}

if (!isNaN(DateValue(1969, 11))) {
  $ERROR("#4: The value should be NaN");
}

if (!isNaN(DateValue(1969, 12))) {
  $ERROR("#5: The value should be NaN");
}

if (!isNaN(DateValue(1970, 0))) {
  $ERROR("#6: The value should be NaN");
}

if (!isNaN(DateValue(1999, 11))) {
  $ERROR("#7: The value should be NaN");
}

if (!isNaN(DateValue(1999, 12))) {
  $ERROR("#8: The value should be NaN");
}

if (!isNaN(DateValue(2000, 0))) {
  $ERROR("#9: The value should be NaN");
}

if (!isNaN(DateValue(2099, 11))) {
  $ERROR("#10: The value should be NaN");
}

if (!isNaN(DateValue(2099, 12))) {
  $ERROR("#11: The value should be NaN");
}

if (!isNaN(DateValue(2100, 0))) {
  $ERROR("#12: The value should be NaN");
}

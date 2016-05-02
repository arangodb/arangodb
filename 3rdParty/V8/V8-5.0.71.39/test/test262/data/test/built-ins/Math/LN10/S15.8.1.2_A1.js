// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Math.LN10 is approximately 2.302585092994046
es5id: 15.8.1.2_A1
description: Comparing Math.LN10 with 2.302585092994046
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
if (!isEqual(Math.LN10, 2.302585092994046)) {
  $ERROR('#1: \'Math.LN10 is not approximately equal to 2.302585092994046\'');
}

// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Math.LN2 is approximately 0.6931471805599453
es5id: 15.8.1.3_A1
description: Comparing Math.LN2 with 0.6931471805599453
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
if (!isEqual(Math.LN2, 0.6931471805599453)) {
  $ERROR('#1: \'Math.LN2 is not approximately equal to 0.6931471805599453\'');
}

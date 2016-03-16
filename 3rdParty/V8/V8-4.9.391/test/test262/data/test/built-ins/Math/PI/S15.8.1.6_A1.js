// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Math.PI is approximately 3.1415926535897932
es5id: 15.8.1.6_A1
description: Comparing Math.PI with 3.1415926535897932
includes:
    - math_precision.js
    - math_isequal.js
---*/

// CHECK#1
if (!isEqual(Math.PI, 3.1415926535897932)) {
  $ERROR('#1: \'Math.PI is not approximatley equal to 3.1415926535897932\'');
}

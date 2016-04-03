// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Number constructor has the property "NaN"
es5id: 15.7.3_A4
description: Checking existence of the property "NaN"
---*/

if(!Number.hasOwnProperty("NaN")){
  $ERROR('#1: The Number constructor has the property "NaN"');
}

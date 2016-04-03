// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Number constructor has the property "MAX_VALUE"
es5id: 15.7.3_A2
description: Checking existence of the property "MAX_VALUE"
---*/

if(!Number.hasOwnProperty("MAX_VALUE")){
  $ERROR('#1: The Number constructor has the property "MAX_VALUE"');
}

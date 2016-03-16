// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date constructor has the property "parse"
es5id: 15.9.4_A2
description: Checking existence of the property "parse"
---*/

if(!Date.hasOwnProperty("parse")){
  $ERROR('#1: The Date constructor has the property "parse"');
}

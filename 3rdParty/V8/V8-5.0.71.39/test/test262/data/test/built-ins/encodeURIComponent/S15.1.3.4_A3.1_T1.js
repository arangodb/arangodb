// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: unescapedURIComponentSet not containing uriReserved
es5id: 15.1.3.4_A3.1_T1
description: Complex tests
---*/

var uriReserved = ["%3B", "%2F", "%3F", "%3A", "%40", "%26", "%3D", "%2B", "%24", "%2C"];
var uriReserved_ = [";", "/", "?", ":", "@", "&", "=", "+", "$", ","];
for (var indexC = 0; indexC < 10; indexC++) {
  var str = uriReserved_[indexC];
  if (encodeURIComponent(str) !== uriReserved[indexC]) {
    $ERROR('#' + (indexC + 1) + ': unescapedURIComponentSet not containing' + str);
  }  
}

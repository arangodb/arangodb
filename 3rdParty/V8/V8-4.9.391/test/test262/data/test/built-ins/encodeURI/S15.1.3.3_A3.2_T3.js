// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    unescapedURISet containing one instance of each character valid in
    uriUnescaped
es5id: 15.1.3.3_A3.2_T3
description: "Complex tests, uriUnescaped :: uriMark"
---*/

var uriMark = ["-", "_", ".", "!", "~", "*", "'", "(", ")"];
for (var indexC = 0; indexC < uriMark.length; indexC++) {
  var str = uriMark[indexC];    
  if (encodeURI(str) !== str) {
    $ERROR('#' + (indexC + 1) + ': unescapedURISet containing' + str);
  }  
}

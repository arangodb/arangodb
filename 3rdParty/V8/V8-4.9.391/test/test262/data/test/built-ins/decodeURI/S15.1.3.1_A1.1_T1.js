// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If string.charAt(k) equal "%" and k + 2 >= string.length, throw URIError
es5id: 15.1.3.1_A1.1_T1
description: Complex tests
---*/

var result = true;

//CHECK#1
try {
  decodeURI("%");
  result = false;
} catch(e) {
  if ((e instanceof URIError) !== true) {
    result = false;
  }
}

//CHECK#2
try {
  decodeURI("%A");
  result = false;
} catch(e) {
  if ((e instanceof URIError) !== true) {
    result = false;
  }
}

//CHECK#3
try {
  decodeURI("%1");
  result = false;
} catch(e) {
  if ((e instanceof URIError) !== true) {
    result = false;
  }
}

//CHECK#4
try {
  decodeURI("% ");
  result = false;
} catch(e) {
  if ((e instanceof URIError) !== true) {
    result = false;
  }
}

if (result !== true) {
  $ERROR('#1: If string.charAt(k) equal "%" and k + 2 >= string.length, throw URIError');
}

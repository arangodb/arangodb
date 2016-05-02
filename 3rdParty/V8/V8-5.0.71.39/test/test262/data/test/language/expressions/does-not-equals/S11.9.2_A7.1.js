// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Type(x) and Type(y) are Object-s.
    Return true, if x and y are references to the same Object; otherwise, return false
es5id: 11.9.2_A7.1
description: >
    Checking Boolean object, Number object, String object, Object
    object
---*/

//CHECK#1
if ((new Boolean(true) != new Boolean(true)) !== true) {
  $ERROR('#1: (new Boolean(true) != new Boolean(true)) === true');
}

//CHECK#2
if ((new Number(1) != new Number(1)) !== true) {
  $ERROR('#2: (new Number(1) != new Number(1)) === true');
}

//CHECK#3
if ((new String("x") != new String("x")) !== true) {
  $ERROR('#3: (new String("x") != new String("x")) === true');
}

//CHECK#4
if ((new Object() != new Object()) !== true) {
  $ERROR('#4: (new Object() != new Object()) === true');
}

//CHECK#5
var x, y;
x = {}; 
y = x;
if ((x != y) !== false) {
  $ERROR('#5: x = {}; y = x; (x != y) === false');
}

//CHECK#6
if ((new Boolean(true) != new Number(1)) !== true) {
  $ERROR('#6 (new Boolean(true) != new Number(1)) === true');
}

//CHECK#7
if ((new Number(1) != new String("1")) !== true) {
  $ERROR('#7: (new Number(1) != new String("1")) === true');
}

//CHECK#8
if ((new String("1") != new Boolean(true)) !== true) {
  $ERROR('#8: (new String("x") != new Boolean(true)) === true');
}

// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the propertyIsEnumerable method is called with argument V, the following steps are taken:
    i) Let O be this object
    ii) Call ToString(V)
    iii) If O doesn't have a property with the name given by Result(ii), return false
    iv) If the property has the DontEnum attribute, return false
    v) Return true
es5id: 15.2.4.7_A2_T2
description: >
    Argument of the propertyIsEnumerable method is a custom boolean
    property
---*/

//CHECK#1
if (typeof Object.prototype.propertyIsEnumerable !== "function") {
  $ERROR('#1: propertyIsEnumerable method is defined');
}

var obj = {the_property:true};

//CHECK#2
if (typeof obj.propertyIsEnumerable !== "function") {
  $ERROR('#2: propertyIsEnumerable method is accessed');
}

//CHECK#3
if (!(obj.propertyIsEnumerable("the_property"))) {
  $ERROR('#3: propertyIsEnumerable method works properly');
}

//CHECK#4
var accum="";
for(var prop in obj) {
  accum+=prop;
}
if (accum.indexOf("the_property")!==0) {
  $ERROR('#4: enumerating works properly');
}
//

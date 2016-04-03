// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Evaluate the production ObjectLiteral: { PropertyName :
    AssignmentExpression }
es5id: 11.1.5_A2
description: Creating property "prop" of various types(boolean, number and etc.)
---*/

//CHECK#1
var x = true;
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#1: var x = true; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#2
var x = new Boolean(true);
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#2: var x = new Boolean(true); var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#3
var x = 1;
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#3: var x = 1; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#4
var x = new Number(1);
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#4: var x = new Number(1); var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#5
var x = "1";
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#5: var x = "1"; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#6
var x = new String(1);
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#6: var x = new String(1); var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#7
var x = undefined;
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#7: var x = undefined; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#8
var x = null;
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#8: var x = null; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#9
var x = {};
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#9: var x = {}; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#10
var x = [1,2];
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#10: var x = [1,2]; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#11
var x = function() {};
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#11: var x = function() {}; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

//CHECK#12
var x = this;
var object = {prop : x}; 
if (object.prop !== x) {
  $ERROR('#12: var x = this; var object = {prop : x}; object.prop === x. Actual: ' + (object.prop));
}

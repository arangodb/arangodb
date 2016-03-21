// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    A property is created with name callee with property
    attributes { DontEnum } and no others
es5id: 10.6_A3_T1
description: Checking existence of arguments.callee property
---*/

//CHECK#1
function f1(){
  return arguments.hasOwnProperty("callee");
}
try{
  if(f1() !== true){
    $ERROR("#1: arguments object doesn't contains property 'callee'");
  }
}
catch(e){
  $ERROR("#1: arguments object doesn't exists");
}

//CHECK#2
var f2 = function(){return arguments.hasOwnProperty("callee");};
try{
  if(f2() !== true){
    $ERROR("#2: arguments object doesn't contains property 'callee'");
  }
}
catch(e){
  $ERROR("#2: arguments object doesn't exists");
}

// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The scope chain is initialised to contain the same objects,
    in the same order, as the calling context's scope chain
es5id: 10.4.2_A1.2_T11
description: eval within global execution context
flags: [noStrict]
---*/

function f(){
  var i;
  var j;
  str1 = '';
  str2 = '';

  for(i in this){
    str1+=i;
  }

  eval('for(j in this){\nstr2+=j;\n}');

  return (str1 === str2);

  this.x = 1;
  this.y = 2;
}

if(!f()){
  $ERROR("#1: scope chain must contain same objects in the same order as the calling context");
}

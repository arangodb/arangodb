// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.15-8-4
description: Array.prototype.lastIndexOf must return correct index(undefined)
---*/

  var obj = {toString:function (){return undefined;}};
  var _undefined1 = undefined;
  var _undefined2;
  var a = new Array(_undefined1,_undefined2,undefined,true,0,false,null,1,"undefined",obj,1);

assert.sameValue(a.lastIndexOf(undefined), 2, 'a.lastIndexOf(undefined)');

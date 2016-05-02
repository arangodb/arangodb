// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.match (regexp)
es5id: 15.5.4.10_A1_T3
description: Checking by using eval
includes: [fnGlobalObject.js]
---*/

var match = String.prototype.match.bind(fnGlobalObject());

try {
    fnGlobalObject().toString = Object.prototype.toString;
} catch (e) { ; }

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if ((fnGlobalObject().toString === Object.prototype.toString)  && //Ensure we could overwrite global obj's toString
    (match(eval("\"bj\""))[0] !== "bj")) {
  $ERROR('#1: match = String.prototype.match.bind(this); match(eval("\\"bj\\""))[0] === "bj". Actual: '+match(eval("\"bj\""))[0] );
}
//
//////////////////////////////////////////////////////////////////////////////

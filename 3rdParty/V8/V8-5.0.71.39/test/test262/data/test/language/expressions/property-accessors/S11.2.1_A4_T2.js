// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check type of various properties
es5id: 11.2.1_A4_T2
description: Checking properties and methods of Object objects
---*/

//CHECK#1-8
if (typeof Object.prototype  !== "object")  $ERROR('#1: typeof Object.prototype === "object". Actual: ' + (typeof Object.prototype ));
if (typeof Object['prototype'] !== "object")  $ERROR('#2: typeof Object["prototype"] === "object". Actual: ' + (typeof Object["prototype"] ));
if (typeof Object.toString !== "function")  $ERROR('#3: typeof Object.toString === "function". Actual: ' + (typeof Object.toString ));
if (typeof Object['toString'] !== "function")  $ERROR('#4: typeof Object["toString"] === "function". Actual: ' + (typeof Object["toString"] ));
if (typeof Object.valueOf !== "function")  $ERROR('#5: typeof Object.valueOf === "function". Actual: ' + (typeof Object.valueOf ));
if (typeof Object['valueOf'] !== "function")  $ERROR('#6: typeof Object["valueOf"] === "function". Actual: ' + (typeof Object["valueOf"] ));
if (typeof Object.constructor  !== "function")  $ERROR('#7: typeof Object.constructor === "function". Actual: ' + (typeof Object.constructor ));
if (typeof Object['constructor'] !== "function")  $ERROR('#8: typeof Object["constructor"] === "function". Actual: ' + (typeof Object["constructor"] ));

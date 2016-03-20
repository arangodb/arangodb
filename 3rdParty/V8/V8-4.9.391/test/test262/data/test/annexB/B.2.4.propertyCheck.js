// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check type of various properties
es5id: B.2.4
description: Checking properties of the Date object (getYear)
---*/

if (typeof Date.prototype.getYear !== "function")  $ERROR('#1: typeof Date.prototype.getYear === "function". Actual: ' + (typeof Date.prototype.getYear ));
if (typeof Date.prototype['getYear'] !== "function")  $ERROR('#2: typeof Date.prototype["getYear"] === "function". Actual: ' + (typeof Date.prototype["getYear"] ));

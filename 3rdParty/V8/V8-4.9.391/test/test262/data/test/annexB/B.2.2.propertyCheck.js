// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check type of various properties
es5id: B.2.2
description: Checking properties of this object (unescape)
---*/

if (typeof this.unescape  === "undefined")  $ERROR('#1: typeof this.unescape !== "undefined"');
if (typeof this['unescape'] === "undefined")  $ERROR('#2: typeof this["unescape"] !== "undefined"');

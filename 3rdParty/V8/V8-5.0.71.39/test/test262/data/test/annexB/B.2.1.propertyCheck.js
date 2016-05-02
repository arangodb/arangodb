// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check type of various properties
es5id: B.2.1
description: Checking properties of this object (escape)
---*/

if (typeof this.escape  === "undefined")  $ERROR('#1: typeof this.escape !== "undefined"');
if (typeof this['escape'] === "undefined")  $ERROR('#2: typeof this["escape"] !== "undefined"');

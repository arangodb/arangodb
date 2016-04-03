// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-141
description: >
    Object.defineProperties - 'writable' property of 'descObj' is
    inherited data property (8.10.5 step 6.a)
includes: [propertyHelper.js]
---*/

var obj = {};

var proto = {
    writable: false
};

var Con = function () { };
Con.prototype = proto;

var descObj = new Con();

Object.defineProperties(obj, {
    property: descObj
});

assert(obj.hasOwnProperty("property"));
verifyNotWritable(obj, "property");

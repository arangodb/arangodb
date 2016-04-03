// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.8-2-b-2
description: >
    Object.seal - the [[Configurable]] attribute of own accessor
    property of 'O' is set from true to false and other attributes of
    the property are unaltered
includes: [propertyHelper.js]
---*/

var obj = {};
obj.variableForHelpVerify = "data";

function setFunc(value) {
    obj.variableForHelpVerify = value;
}
function getFunc() {
    return 10;
}
Object.defineProperty(obj, "foo", {
    get: getFunc,
    set: setFunc,
    enumerable: true,
    configurable: true
});
var preCheck = Object.isExtensible(obj);
Object.seal(obj);

if (!preCheck) {
    $ERROR('Expected preCheck to be true, actually ' + preCheck);
}

verifyEqualTo(obj, "foo", getFunc());

verifyWritable(obj, "foo", "variableForHelpVerify");

verifyEnumerable(obj, "foo");

verifyNotConfigurable(obj, "foo");

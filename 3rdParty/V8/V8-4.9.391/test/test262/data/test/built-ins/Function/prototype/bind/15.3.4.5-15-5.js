// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-15-5
description: >
    Function.prototype.bind - The [[Configurable]] attribute of length
    property in F set as false
---*/

        var canConfigurable = false;
        var hasProperty = false;
        function foo() { }
        var obj = foo.bind({});
        hasProperty = obj.hasOwnProperty("length");
        delete obj.caller;
        canConfigurable = !obj.hasOwnProperty("length");

assert(hasProperty, 'hasProperty !== true');
assert.sameValue(canConfigurable, false, 'canConfigurable');

// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.11
description: >
    Trap returns an iterator whose IteratorResult does not contain a value
    property.
info: >
    [[Enumerate]] ()

    11. Return trapResult
---*/

var x;
var p = new Proxy([1,2,3], {
    enumerate: function(t) {
        return {next: function() { return { done:true }; } };
    }
});

for (x in p) {
    $ERROR("returned iterable interface from trap is flagged as done.");
}

// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.8
description: Templates are cached according to their "raw" representation
info: >
    The internal template registry should be queried according to the "raw"
    strings of the tagged template.
---*/
var previousObject = null;
var firstObject = null;
function tag(templateObject) {
  previousObject = templateObject;
}

tag`\uc548\ub155`;

assert(previousObject !== null);
firstObject = previousObject;
previousObject = null;

tag`안녕`;

assert(previousObject !== null);
assert(firstObject !== previousObject);

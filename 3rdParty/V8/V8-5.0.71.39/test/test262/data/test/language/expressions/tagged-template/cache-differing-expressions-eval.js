// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 12.2.8
description: Template caching using distinct expressions within `eval`
info: >
    Previously-created template objects should be retrieved from the internal
    template registry when their source is identical but their expressions
    evaluate to different values and the tagged template is being evaluated in
    an `eval` context.
---*/
function tag(templateObject) {
  previousObject = templateObject;
}
var a = 1;
var b = 2;
var firstObject = null;
var previousObject = null;

tag`head${a}tail`;
firstObject = previousObject;
assert(firstObject !== null);
previousObject = null;

eval('tag`head${b}tail`');
assert.sameValue(previousObject, firstObject);

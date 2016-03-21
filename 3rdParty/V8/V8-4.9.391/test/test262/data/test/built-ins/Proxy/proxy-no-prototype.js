// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 26.2.2
description: >
    The Proxy constructor does not have a prototype property because
    proxy exotic objects do not have a [[Prototype]] internal slot
    that requires initialization.
---*/

assert.sameValue(Object.hasOwnProperty.call(Proxy, 'prototype'), false);

// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat descriptor
---*/
var desc = Object.getOwnPropertyDescriptor(Array.prototype, "concat");
assert.sameValue(desc.enumerable, false);

// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-11-9
description: JSON.stringify correctly works on top level Boolean objects.
---*/

assert.sameValue(JSON.stringify(new Boolean(false)), 'false', 'JSON.stringify(new Boolean(false))');

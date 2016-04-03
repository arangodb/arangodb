// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-11-14
description: Applying JSON.stringify to a  function returns undefined.
---*/

assert.sameValue(JSON.stringify(function() {}), undefined, 'JSON.stringify(function() {})');

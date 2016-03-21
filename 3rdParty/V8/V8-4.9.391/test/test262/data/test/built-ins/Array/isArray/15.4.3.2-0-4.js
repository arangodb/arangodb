// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.3.2-0-4
description: Array.isArray return false if its argument is not an Array
---*/

  var b_num   = Array.isArray(42);
  var b_undef = Array.isArray(undefined);
  var b_bool  = Array.isArray(true);
  var b_str   = Array.isArray("abc");
  var b_obj   = Array.isArray({});
  var b_null  = Array.isArray(null);
  

assert.sameValue(b_num, false, 'b_num');
assert.sameValue(b_undef, false, 'b_undef');
assert.sameValue(b_bool, false, 'b_bool');
assert.sameValue(b_str, false, 'b_str');
assert.sameValue(b_obj, false, 'b_obj');
assert.sameValue(b_null, false, 'b_null');

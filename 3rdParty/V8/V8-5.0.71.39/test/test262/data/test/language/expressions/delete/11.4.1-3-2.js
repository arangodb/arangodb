// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-3-2
description: >
    delete operator throws ReferenceError when deleting an explicitly
    qualified yet unresolvable reference (base obj undefined)
---*/

  // just cooking up a long/veryLikely unique name
assert.throws(ReferenceError, function() {
    var d = delete __ES3_1_test_suite_test_11_4_1_3_unique_id_2__.x;
});

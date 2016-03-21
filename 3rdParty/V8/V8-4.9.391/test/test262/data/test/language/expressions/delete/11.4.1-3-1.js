// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-3-1
description: >
    delete operator returns true when deleting an unresolvable
    reference
flags: [noStrict]
---*/

  // just cooking up a long/veryLikely unique name
  var d = delete __ES3_1_test_suite_test_11_4_1_3_unique_id_0__;

assert.sameValue(d, true, 'd');

// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-3-3
description: >
    delete operator returns true when deleting an explicitly qualified
    yet unresolvable reference (property undefined for base obj)
---*/

  var __ES3_1_test_suite_test_11_4_1_3_unique_id_3__ = {};
  var d = delete __ES3_1_test_suite_test_11_4_1_3_unique_id_3__.x;

assert.sameValue(d, true, 'd');

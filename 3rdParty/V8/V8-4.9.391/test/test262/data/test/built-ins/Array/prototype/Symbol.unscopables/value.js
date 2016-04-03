// Copyright (C) 2015 Mike Pennisi. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.31
description: >
    Initial value of `Symbol.unscopables` property
info: >
    1. Let blackList be ObjectCreate(null).
    2. Perform CreateDataProperty(blackList, "copyWithin", true).
    3. Perform CreateDataProperty(blackList, "entries", true).
    4. Perform CreateDataProperty(blackList, "fill", true).
    5. Perform CreateDataProperty(blackList, "find", true).
    6. Perform CreateDataProperty(blackList, "findIndex", true).
    7. Perform CreateDataProperty(blackList, "keys", true).
    8. Perform CreateDataProperty(blackList, "values", true).
    9. Assert: Each of the above calls will return true.
    10. Return blackList.
includes: [propertyHelper.js]
features: [Symbol.unscopables]
---*/

var unscopables = Array.prototype[Symbol.unscopables];

assert.sameValue(Object.getPrototypeOf(unscopables), null);

assert.sameValue(unscopables.copyWithin, true, '`copyWithin` property value');
verifyEnumerable(unscopables, 'copyWithin');
verifyWritable(unscopables, 'copyWithin');
verifyConfigurable(unscopables, 'copyWithin');

assert.sameValue(unscopables.entries, true, '`entries` property value');
verifyEnumerable(unscopables, 'entries');
verifyWritable(unscopables, 'entries');
verifyConfigurable(unscopables, 'entries');

assert.sameValue(unscopables.fill, true, '`fill` property value');
verifyEnumerable(unscopables, 'fill');
verifyWritable(unscopables, 'fill');
verifyConfigurable(unscopables, 'fill');

assert.sameValue(unscopables.find, true, '`find` property value');
verifyEnumerable(unscopables, 'find');
verifyWritable(unscopables, 'find');
verifyConfigurable(unscopables, 'find');

assert.sameValue(unscopables.findIndex, true, '`findIndex` property value');
verifyEnumerable(unscopables, 'findIndex');
verifyWritable(unscopables, 'findIndex');
verifyConfigurable(unscopables, 'findIndex');

assert.sameValue(unscopables.keys, true, '`keys` property value');
verifyEnumerable(unscopables, 'keys');
verifyWritable(unscopables, 'keys');
verifyConfigurable(unscopables, 'keys');

assert.sameValue(unscopables.values, true, '`values` property value');
verifyEnumerable(unscopables, 'values');
verifyWritable(unscopables, 'values');
verifyConfigurable(unscopables, 'values');

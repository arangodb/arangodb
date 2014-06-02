var Decimal = Decimal || require('../lib/decimal.js');
var assert = assert || require('assert');

assert.equal(Decimal(5) instanceof Decimal, true);
assert.equal(Decimal(5), '5');
assert.equal(Decimal('5'), '5');
assert.equal(Decimal(5.1), '5.1');
assert.equal(Decimal(Decimal(5.1)) instanceof Decimal, true);

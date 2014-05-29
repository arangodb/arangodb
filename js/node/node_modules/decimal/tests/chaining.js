var Decimal = Decimal || require('../lib/decimal.js');
var assert = assert || require('assert');


assert.equal(Decimal(1.005).mul(50.01).mul(0.03), "1.5078015");
assert.equal(Decimal(1.005).div(50.01).add(0.03), "0.05009598080383923");

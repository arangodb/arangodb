var Decimal = Decimal || require('../lib/decimal.js');
var assert = assert || require('assert');


assert.equal(Decimal('123000').sub('123.456'), '122876.544');
assert.equal(Decimal('123.456').sub('123000'), '-122876.544');
assert.equal(Decimal('100.2').sub('1203.12'), '-1102.92');
assert.equal(Decimal('1203.12').sub('100.2'), '1102.92');
assert.equal(Decimal('-1203.12').sub('-100.2'), '-1102.92');

assert.equal(Decimal('123.456').sub('123000'), Decimal.sub('123.456','123000').toString());
assert.equal(Decimal('100.2').sub('1203.12'), Decimal.sub('100.2', '1203.12').toString());

var Decimal = Decimal || require('../lib/decimal.js');
var assert = assert || require('assert');


assert.equal(Decimal('123000').add('123.456'), '123123.456');
assert.equal(Decimal('123.456').add('123000'), '123123.456');
assert.equal(Decimal('100.2').add('1203.12'), '1303.32');
assert.equal(Decimal('1203.12').add('100.2'), '1303.32');
assert.equal(Decimal('123000').add('-123.456'), '122876.544');
assert.equal(Decimal('123.456').add('-123000'), '-122876.544');
assert.equal(Decimal('100.2').add('-1203.12'), '-1102.92');
assert.equal(Decimal('1203.12').add('-100.2'), '1102.92');
assert.equal(Decimal('1.2').add('1000'), '1001.2');
assert.equal(Decimal('1.245').add('1010'), '1011.245');
assert.equal(Decimal('5.1').add('1.901'), '7.001');
assert.equal(Decimal('1001.0').add('7.12'), '1008.12');

assert.equal(Decimal('1001.0').add('7.12'), Decimal.add('1001.0', '7.12').toString());
assert.equal(Decimal('1001.0').add('-7.12'), Decimal.add('1001.0', '-7.12').toString());

var Decimal = Decimal || require('../lib/decimal.js');
var assert = assert || require('assert');


assert.equal(Decimal('50').div('2.901'), '17.23543605653223');
assert.equal(Decimal('20.3344').div('2.901'), '7.00944501895898');

assert.equal(Decimal('1.125').div('0.1201'), Decimal.div('1.125', '0.1201').toString());
assert.equal(Decimal('01.125').div('0.1201'), Decimal.div('01.125', '0.1201').toString());

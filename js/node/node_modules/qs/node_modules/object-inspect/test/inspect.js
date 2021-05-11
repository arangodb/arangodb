var test = require('tape');
var hasSymbols = require('has-symbols/shams')();
var utilInspect = require('../util.inspect');
var repeat = require('string.prototype.repeat');

var inspect = require('..');

test('inspect', function (t) {
    t.plan(4);
    var obj = [{ inspect: function xyzInspect() { return '!XYZ¡'; } }, []];
    t.equal(inspect(obj), '[ !XYZ¡, [] ]');
    t.equal(inspect(obj, { customInspect: true }), '[ !XYZ¡, [] ]');
    t.equal(inspect(obj, { customInspect: false }), '[ { inspect: [Function: xyzInspect] }, [] ]');
    t['throws'](
        function () { inspect(obj, { customInspect: 'not a boolean' }); },
        TypeError,
        '`customInspect` must be a boolean'
    );
});

test('inspect custom symbol', { skip: !hasSymbols || !utilInspect || !utilInspect.custom }, function (t) {
    t.plan(3);

    var obj = { inspect: function stringInspect() { return 'string'; } };
    obj[utilInspect.custom] = function custom() { return 'symbol'; };

    t.equal(inspect([obj, []]), '[ ' + (utilInspect.custom ? 'symbol' : 'string') + ', [] ]');
    t.equal(inspect([obj, []], { customInspect: true }), '[ ' + (utilInspect.custom ? 'symbol' : 'string') + ', [] ]');
    t.equal(
        inspect([obj, []], { customInspect: false }),
        '[ { inspect: [Function: stringInspect]' + (utilInspect.custom ? ', [' + inspect(utilInspect.custom) + ']: [Function: custom]' : '') + ' }, [] ]'
    );
});

test('symbols', { skip: !hasSymbols }, function (t) {
    t.plan(2);

    var obj = { a: 1 };
    obj[Symbol('test')] = 2;
    obj[Symbol.iterator] = 3;
    Object.defineProperty(obj, Symbol('non-enum'), {
        enumerable: false,
        value: 4
    });

    if (typeof Symbol.iterator === 'symbol') {
        t.equal(inspect(obj), '{ a: 1, [Symbol(test)]: 2, [Symbol(Symbol.iterator)]: 3 }', 'object with symbols');
        t.equal(inspect([obj, []]), '[ { a: 1, [Symbol(test)]: 2, [Symbol(Symbol.iterator)]: 3 }, [] ]', 'object with symbols in array');
    } else {
        // symbol sham key ordering is unreliable
        t.match(
            inspect(obj),
            /^(?:{ a: 1, \[Symbol\(test\)\]: 2, \[Symbol\(Symbol.iterator\)\]: 3 }|{ a: 1, \[Symbol\(Symbol.iterator\)\]: 3, \[Symbol\(test\)\]: 2 })$/,
            'object with symbols (nondeterministic symbol sham key ordering)'
        );
        t.match(
            inspect([obj, []]),
            /^\[ (?:{ a: 1, \[Symbol\(test\)\]: 2, \[Symbol\(Symbol.iterator\)\]: 3 }|{ a: 1, \[Symbol\(Symbol.iterator\)\]: 3, \[Symbol\(test\)\]: 2 }), \[\] \]$/,
            'object with symbols in array (nondeterministic symbol sham key ordering)'
        );
    }
});

test('maxStringLength', function (t) {
    t['throws'](
        function () { inspect('', { maxStringLength: -1 }); },
        TypeError,
        'maxStringLength must be >= 0, or Infinity, not negative'
    );

    var str = repeat('a', 1e8);

    t.equal(
        inspect([str], { maxStringLength: 10 }),
        '[ \'aaaaaaaaaa\'... 99999990 more characters ]',
        'maxStringLength option limits output'
    );

    t.equal(
        inspect(['f'], { maxStringLength: null }),
        '[ \'\'... 1 more character ]',
        'maxStringLength option accepts `null`'
    );

    t.equal(
        inspect([str], { maxStringLength: Infinity }),
        '[ \'' + str + '\' ]',
        'maxStringLength option accepts ∞'
    );

    t.end();
});

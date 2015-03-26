describe('extendable', function () {
  'use strict';

  var assume = require('assume')
    , extend = require('./');

  it('is exported as function', function () {
    assume(extend).is.a('function');
  });

  it('returns the newly extended function', function () {
    function foo() {} foo.extend = extend;

    var bar = foo.extend();
    assume(bar).does.not.equal(foo);
  });

  it('can override properties and methods', function () {
    function foo() {} foo.extend = extend;
    foo.prototype.bar = function () { return true; };
    foo.prototype.prop = 10;

    var Bar = foo.extend({
      bar: function () {
        return false;
      },
      prop: 'foo'
    });

    var bar = new Bar();

    assume(bar.bar()).is.false();
    assume(bar.prop).equals('foo');
  });
});

describe('extendible', function () {
  // 'use strict'; // Disabled for strict-mode vs global mode tests;
  var assume = require('assume')
    , extend = require('./')
    , Foo;

  beforeEach(function each() {
    Foo = function Foo() {};
    Foo.extend = extend;
  });

  it('is exported as function', function () {
    assume(extend).is.a('function');
  });

  it('returns the newly extended function', function () {
    var Bar = Foo.extend();
    assume(Bar).does.not.equal(Foo);
  });

  it('can override properties and methods', function () {
    /* istanbul ignore next */
    Foo.prototype.bar = function () { return true; };
    Foo.prototype.prop = 10;

    var Bar = Foo.extend({
      bar: function () {
        return false;
      },
      prop: 'foo'
    });

    var bar = new Bar();

    assume(bar.bar()).is.false();
    assume(bar.prop).equals('foo');
  });

  it('correctly inherits and overrides getters', function () {
    var Bar = Foo.extend({
      get hello() {
        return 'hi';
      }
    });

    var Baz = Bar.extend({
      yes: false
    });

    var bar = new Baz();

    assume(bar.yes).equals(false);
    assume(bar.hello).equals('hi');

    var Override = Bar.extend({
      hello: 'world'
    });

    var override = new Override();
    assume(override.hello).equals('world');
  });

  it('returns the value from the parent constructor', function () {
    var Baz = function () {
      return {
        my: 'name is '+ this.name
      };
    };

    Baz.prototype.name = 'baz';
    Baz.extend = extend;

    var Bob = Baz.extend({
      name: 'bob'
    });

    var bob = new Bob()
      , baz = new Baz();

    assume(baz.my).equals('name is baz');
    assume(bob.my).equals('name is bob');
  });

  it('runs in strict mode if the parent runs in strict mode', function (next) {
    next = assume.plan(2, next);

    var Baz = function () {
      'use strict';

      assume(this).equals(undefined);
    };

    Baz.extend = extend;

    var Bar = Baz.extend()
      , bar = Bar()
      , baz = Baz();

    next();
  });

  it('runs in does not run in strict mode if the parent isnt', function (next) {
    next = assume.plan(2, next);

    var Baz = function () {
      assume(this).equals(global);
    };

    Baz.extend = extend;

    var Bar = Baz.extend()
      , bar = Bar()
      , baz = Baz();

    next();
  });
});

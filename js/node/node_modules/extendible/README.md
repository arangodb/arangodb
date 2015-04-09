# extendible

[![From bigpipe.io][from]](http://bigpipe.io)[![Version npm][version]](http://browsenpm.org/package/extendible)[![Build Status][build]](https://travis-ci.org/bigpipe/extendible)[![Dependencies][david]](https://david-dm.org/bigpipe/extendible)[![Coverage Status][cover]](https://coveralls.io/r/bigpipe/extendible?branch=master)

[from]: https://img.shields.io/badge/from-bigpipe.io-9d8dff.svg?style=flat-square
[version]: http://img.shields.io/npm/v/extendible.svg?style=flat-square
[build]: http://img.shields.io/travis/bigpipe/extendible/master.svg?style=flat-square
[david]: https://img.shields.io/david/bigpipe/extendible.svg?style=flat-square
[cover]: http://img.shields.io/coveralls/bigpipe/extendible/master.svg?style=flat-square

Extend your JavaScript constructor in the same way as would extend Backbone.js
Models using the `extend` method. Extending and inheritance has never been
easier and more developer friendly. While the original version was straight port
of the Backbone's source it has been re factored and rewritten many times to the
project it is today. It features:

- Backbone compatible `extend` API.
- Support for EcmaScript 5 getters and setters.
- Understands the difference between constructors that are created in `use
  strict` mode.

## Installation

The module is intended for server-side and browser usage. We use feature
detection to ensure compatibility with EcmaScript 3 based environments. The
module is distributed through `npm` and can be installed using:

```
npm install --save extendible
```

## Usage

In all example code we assume that you've already required the `extendible`
module and saved it as the `extend` variable as shown in the following example:

```js
var extend = require('extendible');
```

The extend method should be on the constructor as `.extend` method:

```js
function Word() {
  this.foo = 'bar';
}

//
// It should be added on the constructor, not as property on the prototype!
//
Word.extend = extend;
```

To create a new **Foo** class of your own you call the `Foo.extend` method with
2 arguments:

1. `Object` properties and methods that should be added to your extended class
   prototype. These will override existing properties, but it would not override
   the properties on the parent/root class that you extend on.
2. `Object` properties and methods that should added on the `constructor`
   directly. So instead of being introduced on the `.prototype` it's directly
   added to the returned Function.

As the properties of the prototype and constructor are inherited from the
parent/root constructor you can further extended using the same **extend**
method:

```js
var Hello = Word.extend({
  name: 'hello',
  say: function update() {
    console.log('the word is: '+ this.name);
  }
});

var World = Hello.extend({
  name: 'world'
});

var world = new World();
world.say(); // 'the word is: world'
```

### License

MIT

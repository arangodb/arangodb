# Extend your JavaScript constructors in the same as you are used to in backbone.js

[![Build Status](https://travis-ci.org/3rd-Eden/extendable.png)](https://travis-ci.org/3rd-Eden/extendable)

```js
var extend = require('extendible')
  , EventEmitter = require('events').EventEmitter;

function Awesomeness() {
  var self = this;

  setTimeout(function () {
    self.render(self.data);
  }, 100);

  EventEmitter.call(this);
}

Awesomeness.prototype = new EventEmitter;
Awesomeness.prototype.constructor = Awesomeness;

Awesomeness.prototype.data = 'bar';
Awesomeness.prototype.render = function render() {
  // does nothing
};

Awesomeness.extend = extend;
```

And you can now use it

```js
var SuperAwesome = Awesomeness.extend({
    data: 'trololol'

  , render: function render(data) {
      console.log(data);
    }
});

new SuperAwesome();
// outputs "trololo" after 100 ms
```

### License

MIT

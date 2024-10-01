var extend = require('../')
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

var SuperAwesome = Awesomeness.extend({
    data: 'trololol'

  , render: function render(data) {
      console.log(data);
    }
});

new SuperAwesome();

'use strict';


var util = require('util');
var yaml = require('../../lib/js-yaml');


function Tag1(parameters) {
  this.x = parameters.x;
  this.y = parameters.y || 0;
  this.z = parameters.z || 0;
}


function Tag2() {
  Tag1.apply(this, arguments);
}
util.inherits(Tag2, Tag1);


function Tag3() {
  Tag2.apply(this, arguments);
}
util.inherits(Tag3, Tag2);


function Foo(parameters) {
  this.myParameter        = parameters.myParameter;
  this.myAnotherParameter = parameters.myAnotherParameter;
}


var TEST_SCHEMA = yaml.Schema.create([
  // NOTE: Type order matters!
  // Inherited classes must precede their parents because the dumper
  // doesn't inspect class inheritance and just picks first suitable
  // class from this array.
  new yaml.Type('!tag3', {
    kind: 'mapping',
    resolve: function (data) {
      if (null === data) {
        return false;
      }
      if (!Object.prototype.hasOwnProperty.call(data, '=') &&
          !Object.prototype.hasOwnProperty.call(data, 'x')) {
        return false;
      }
      if (!Object.keys(data).every(function (k) { return '=' === k || 'x' === k || 'y' === k || 'z' === k; })) {
        return false;
      }
      return true;
    },
    construct: function (data) {
      return new Tag3({ x: (data['='] || data.x), y: data.y, z: data.z });
    },
    instanceOf: Tag3,
    represent: function (object) {
      return { '=': object.x, y: object.y, z: object.z };
    }
  }),
  new yaml.Type('!tag2', {
    kind: 'scalar',
    construct: function (data) {
      return new Tag2({ x: ('number' === typeof data) ? data : parseInt(data, 10) });
    },
    instanceOf: Tag2,
    represent: function (object) {
      return String(object.x);
    }
  }),
  new yaml.Type('!tag1', {
    kind: 'mapping',
    resolve: function (data) {
      if (null === data) {
        return false;
      }
      if (!Object.prototype.hasOwnProperty.call(data, 'x')) {
        return false;
      }
      if (!Object.keys(data).every(function (k) { return 'x' === k || 'y' === k || 'z' === k; })) {
        return false;
      }
      return true;
    },
    construct: function (data) {
      return new Tag1({ x: data.x, y: data.y, z: data.z });
    },
    instanceOf: Tag1
  }),
  new yaml.Type('!foo', {
    kind: 'mapping',
    resolve: function (data) {
      if (null === data) {
        return false;
      }
      if (!Object.keys(data).every(function (k) { return 'my-parameter' === k || 'my-another-parameter' === k; })) {
        return false;
      }
      return true;
    },
    construct: function (data) {
      return new Foo({
        myParameter:        data['my-parameter'],
        myAnotherParameter: data['my-another-parameter']
      });
    },
    instanceOf: Foo,
    represent: function (object) {
      return {
        'my-parameter':         object.myParameter,
        'my-another-parameter': object.myAnotherParameter
      };
    }
  })
]);


module.exports.Tag1 = Tag1;
module.exports.Tag2 = Tag2;
module.exports.Tag3 = Tag3;
module.exports.Foo = Foo;
module.exports.TEST_SCHEMA = TEST_SCHEMA;

////////////////////////////////////////////////////////////////////////////////
/// @brief remove last occurrence of element from an array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "removeLastOccurrenceOf", {
  value: function (element) {
    var index = this.lastIndexOf(element);
    return this.splice(index, 1);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief return the union with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "unite", {
  value: function (other_array) {
    return other_array.concat(this.filter(function (element) {
      return (other_array.indexOf(element) === -1);
    }));
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief return the intersection with another array
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Array.prototype, "intersect", {
  value: function (other_array) {
    return this.filter(function (element) {
      return (other_array.indexOf(element) > -1);
    });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copy properties
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "shallowCopy", {
  get: function() {
    var shallow = {},
      key;

    for (key in this) {
      if (this.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
        shallow[key] = this[key];
      }
    }

    return shallow;
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief property keys
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, "propertyKeys", {
  get: function() {
    var keys = [],
      key;

    for (key in this) {
      if (this.hasOwnProperty(key) && key[0] !== '_' && key[0] !== '$') {
        keys.push(key);
      }
    }

    return keys;
  }
});


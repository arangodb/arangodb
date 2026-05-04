'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.GraphQLError = undefined;

var _language = require('../language');

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }
/**
 *  Copyright (c) 2015, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

var GraphQLError = exports.GraphQLError = function (_Error) {
  _inherits(GraphQLError, _Error);

  function GraphQLError(message,
  // A flow bug keeps us from declaring nodes as an array of Node
  nodes, stack, source, positions, path, originalError) {
    _classCallCheck(this, GraphQLError);

    var _this = _possibleConstructorReturn(this, _Error.call(this, message));

    Object.defineProperty(_this, 'message', {
      value: message,
      // By being enumerable, JSON.stringify will include `message` in the
      // resulting output. This ensures that the simplist possible GraphQL
      // service adheres to the spec.
      enumerable: true,
      // Note: you really shouldn't overwrite message, but it enables
      // Error brand-checking.
      writable: true
    });

    Object.defineProperty(_this, 'stack', {
      value: stack || message,
      // Note: stack should not really be writable, but some libraries (such
      // as bluebird) use Error brand-checking which specifically looks to see
      // if stack is a writable property.
      writable: true
    });

    Object.defineProperty(_this, 'nodes', { value: nodes });

    // Note: flow does not yet know about Object.defineProperty with `get`.
    Object.defineProperty(_this, 'source', {
      get: function get() {
        if (source) {
          return source;
        }
        if (nodes && nodes.length > 0) {
          var node = nodes[0];
          return node && node.loc && node.loc.source;
        }
      }
    });

    Object.defineProperty(_this, 'positions', {
      get: function get() {
        if (positions) {
          return positions;
        }
        if (nodes) {
          var nodePositions = nodes.map(function (node) {
            return node.loc && node.loc.start;
          });
          if (nodePositions.some(function (p) {
            return p;
          })) {
            return nodePositions;
          }
        }
      }
    });

    Object.defineProperty(_this, 'locations', {
      get: function get() {
        var _positions = this.positions;
        var _source = this.source;
        if (_positions && _positions.length > 0 && _source) {
          return _positions.map(function (pos) {
            return (0, _language.getLocation)(_source, pos);
          });
        }
      },

      // By being enumerable, JSON.stringify will include `locations` in the
      // resulting output. This ensures that the simplist possible GraphQL
      // service adheres to the spec.
      enumerable: true
    });

    Object.defineProperty(_this, 'path', {
      value: path,
      // By being enumerable, JSON.stringify will include `path` in the
      // resulting output. This ensures that the simplist possible GraphQL
      // service adheres to the spec.
      enumerable: true
    });

    Object.defineProperty(_this, 'originalError', {
      value: originalError
    });
    return _this;
  }

  return GraphQLError;
}(Error);
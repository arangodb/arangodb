'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.buildASTSchema = buildASTSchema;

var _find = require('../jsutils/find');

var _find2 = _interopRequireDefault(_find);

var _invariant = require('../jsutils/invariant');

var _invariant2 = _interopRequireDefault(_invariant);

var _keyMap = require('../jsutils/keyMap');

var _keyMap2 = _interopRequireDefault(_keyMap);

var _keyValMap = require('../jsutils/keyValMap');

var _keyValMap2 = _interopRequireDefault(_keyValMap);

var _valueFromAST = require('./valueFromAST');

var _values = require('../execution/values');

var _kinds = require('../language/kinds');

var _schema = require('../type/schema');

var _scalars = require('../type/scalars');

var _definition = require('../type/definition');

var _directives = require('../type/directives');

var _introspection = require('../type/introspection');

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 *  Copyright (c) 2015, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */

function buildWrappedType(innerType, inputTypeAST) {
  if (inputTypeAST.kind === _kinds.LIST_TYPE) {
    return new _definition.GraphQLList(buildWrappedType(innerType, inputTypeAST.type));
  }
  if (inputTypeAST.kind === _kinds.NON_NULL_TYPE) {
    var wrappedType = buildWrappedType(innerType, inputTypeAST.type);
    (0, _invariant2.default)(!(wrappedType instanceof _definition.GraphQLNonNull), 'No nesting nonnull.');
    return new _definition.GraphQLNonNull(wrappedType);
  }
  return innerType;
}

function getNamedTypeAST(typeAST) {
  var namedType = typeAST;
  while (namedType.kind === _kinds.LIST_TYPE || namedType.kind === _kinds.NON_NULL_TYPE) {
    namedType = namedType.type;
  }
  return namedType;
}

/**
 * This takes the ast of a schema document produced by the parse function in
 * src/language/parser.js.
 *
 * Given that AST it constructs a GraphQLSchema. As constructed
 * they are not particularly useful for non-introspection queries
 * since they have no resolve methods.
 */
function buildASTSchema(ast) {
  if (!ast || ast.kind !== _kinds.DOCUMENT) {
    throw new Error('Must provide a document ast.');
  }

  var schemaDef = void 0;

  var typeDefs = [];
  var directiveDefs = [];
  for (var i = 0; i < ast.definitions.length; i++) {
    var d = ast.definitions[i];
    switch (d.kind) {
      case _kinds.SCHEMA_DEFINITION:
        if (schemaDef) {
          throw new Error('Must provide only one schema definition.');
        }
        schemaDef = d;
        break;
      case _kinds.SCALAR_TYPE_DEFINITION:
      case _kinds.OBJECT_TYPE_DEFINITION:
      case _kinds.INTERFACE_TYPE_DEFINITION:
      case _kinds.ENUM_TYPE_DEFINITION:
      case _kinds.UNION_TYPE_DEFINITION:
      case _kinds.INPUT_OBJECT_TYPE_DEFINITION:
        typeDefs.push(d);
        break;
      case _kinds.DIRECTIVE_DEFINITION:
        directiveDefs.push(d);
        break;
    }
  }

  if (!schemaDef) {
    throw new Error('Must provide a schema definition.');
  }

  var queryTypeName = void 0;
  var mutationTypeName = void 0;
  var subscriptionTypeName = void 0;
  schemaDef.operationTypes.forEach(function (operationType) {
    var typeName = operationType.type.name.value;
    if (operationType.operation === 'query') {
      if (queryTypeName) {
        throw new Error('Must provide only one query type in schema.');
      }
      queryTypeName = typeName;
    } else if (operationType.operation === 'mutation') {
      if (mutationTypeName) {
        throw new Error('Must provide only one mutation type in schema.');
      }
      mutationTypeName = typeName;
    } else if (operationType.operation === 'subscription') {
      if (subscriptionTypeName) {
        throw new Error('Must provide only one subscription type in schema.');
      }
      subscriptionTypeName = typeName;
    }
  });

  if (!queryTypeName) {
    throw new Error('Must provide schema definition with query type.');
  }

  var astMap = (0, _keyMap2.default)(typeDefs, function (d) {
    return d.name.value;
  });

  if (!astMap[queryTypeName]) {
    throw new Error('Specified query type "' + queryTypeName + '" not found in document.');
  }

  if (mutationTypeName && !astMap[mutationTypeName]) {
    throw new Error('Specified mutation type "' + mutationTypeName + '" not found in document.');
  }

  if (subscriptionTypeName && !astMap[subscriptionTypeName]) {
    throw new Error('Specified subscription type "' + subscriptionTypeName + '" not found in document.');
  }

  var innerTypeMap = {
    String: _scalars.GraphQLString,
    Int: _scalars.GraphQLInt,
    Float: _scalars.GraphQLFloat,
    Boolean: _scalars.GraphQLBoolean,
    ID: _scalars.GraphQLID,
    __Schema: _introspection.__Schema,
    __Directive: _introspection.__Directive,
    __DirectiveLocation: _introspection.__DirectiveLocation,
    __Type: _introspection.__Type,
    __Field: _introspection.__Field,
    __InputValue: _introspection.__InputValue,
    __EnumValue: _introspection.__EnumValue,
    __TypeKind: _introspection.__TypeKind
  };

  var types = typeDefs.map(function (def) {
    return typeDefNamed(def.name.value);
  });

  var directives = directiveDefs.map(getDirective);

  // If specified directives were not explicitly declared, add them.
  if (!directives.some(function (directive) {
    return directive.name === 'skip';
  })) {
    directives.push(_directives.GraphQLSkipDirective);
  }

  if (!directives.some(function (directive) {
    return directive.name === 'include';
  })) {
    directives.push(_directives.GraphQLIncludeDirective);
  }

  if (!directives.some(function (directive) {
    return directive.name === 'deprecated';
  })) {
    directives.push(_directives.GraphQLDeprecatedDirective);
  }

  return new _schema.GraphQLSchema({
    query: getObjectType(astMap[queryTypeName]),
    mutation: mutationTypeName ? getObjectType(astMap[mutationTypeName]) : null,
    subscription: subscriptionTypeName ? getObjectType(astMap[subscriptionTypeName]) : null,
    types: types,
    directives: directives
  });

  function getDirective(directiveAST) {
    return new _directives.GraphQLDirective({
      name: directiveAST.name.value,
      locations: directiveAST.locations.map(function (node) {
        return node.value;
      }),
      args: directiveAST.arguments && makeInputValues(directiveAST.arguments)
    });
  }

  function getObjectType(typeAST) {
    var type = typeDefNamed(typeAST.name.value);
    (0, _invariant2.default)(type instanceof _definition.GraphQLObjectType, 'AST must provide object type.');
    return type;
  }

  function produceType(typeAST) {
    var typeName = getNamedTypeAST(typeAST).name.value;
    var typeDef = typeDefNamed(typeName);
    return buildWrappedType(typeDef, typeAST);
  }

  function produceInputType(typeAST) {
    var type = produceType(typeAST);
    (0, _invariant2.default)((0, _definition.isInputType)(type), 'Expected Input type.');
    return type;
  }

  function produceOutputType(typeAST) {
    var type = produceType(typeAST);
    (0, _invariant2.default)((0, _definition.isOutputType)(type), 'Expected Output type.');
    return type;
  }

  function produceObjectType(typeAST) {
    var type = produceType(typeAST);
    (0, _invariant2.default)(type instanceof _definition.GraphQLObjectType, 'Expected Object type.');
    return type;
  }

  function produceInterfaceType(typeAST) {
    var type = produceType(typeAST);
    (0, _invariant2.default)(type instanceof _definition.GraphQLInterfaceType, 'Expected Object type.');
    return type;
  }

  function typeDefNamed(typeName) {
    if (innerTypeMap[typeName]) {
      return innerTypeMap[typeName];
    }

    if (!astMap[typeName]) {
      throw new Error('Type "' + typeName + '" not found in document.');
    }

    var innerTypeDef = makeSchemaDef(astMap[typeName]);
    if (!innerTypeDef) {
      throw new Error('Nothing constructed for "' + typeName + '".');
    }
    innerTypeMap[typeName] = innerTypeDef;
    return innerTypeDef;
  }

  function makeSchemaDef(def) {
    if (!def) {
      throw new Error('def must be defined');
    }
    switch (def.kind) {
      case _kinds.OBJECT_TYPE_DEFINITION:
        return makeTypeDef(def);
      case _kinds.INTERFACE_TYPE_DEFINITION:
        return makeInterfaceDef(def);
      case _kinds.ENUM_TYPE_DEFINITION:
        return makeEnumDef(def);
      case _kinds.UNION_TYPE_DEFINITION:
        return makeUnionDef(def);
      case _kinds.SCALAR_TYPE_DEFINITION:
        return makeScalarDef(def);
      case _kinds.INPUT_OBJECT_TYPE_DEFINITION:
        return makeInputObjectDef(def);
      default:
        throw new Error('Type kind "' + def.kind + '" not supported.');
    }
  }

  function makeTypeDef(def) {
    var typeName = def.name.value;
    var config = {
      name: typeName,
      fields: function fields() {
        return makeFieldDefMap(def);
      },
      interfaces: function interfaces() {
        return makeImplementedInterfaces(def);
      }
    };
    return new _definition.GraphQLObjectType(config);
  }

  function makeFieldDefMap(def) {
    return (0, _keyValMap2.default)(def.fields, function (field) {
      return field.name.value;
    }, function (field) {
      return {
        type: produceOutputType(field.type),
        args: makeInputValues(field.arguments),
        deprecationReason: getDeprecationReason(field.directives)
      };
    });
  }

  function makeImplementedInterfaces(def) {
    return def.interfaces && def.interfaces.map(function (iface) {
      return produceInterfaceType(iface);
    });
  }

  function makeInputValues(values) {
    return (0, _keyValMap2.default)(values, function (value) {
      return value.name.value;
    }, function (value) {
      var type = produceInputType(value.type);
      return { type: type, defaultValue: (0, _valueFromAST.valueFromAST)(value.defaultValue, type) };
    });
  }

  function makeInterfaceDef(def) {
    var typeName = def.name.value;
    var config = {
      name: typeName,
      resolveType: function resolveType() {
        return null;
      },
      fields: function fields() {
        return makeFieldDefMap(def);
      }
    };
    return new _definition.GraphQLInterfaceType(config);
  }

  function makeEnumDef(def) {
    var enumType = new _definition.GraphQLEnumType({
      name: def.name.value,
      values: (0, _keyValMap2.default)(def.values, function (enumValue) {
        return enumValue.name.value;
      }, function (enumValue) {
        return {
          deprecationReason: getDeprecationReason(enumValue.directives)
        };
      })
    });

    return enumType;
  }

  function makeUnionDef(def) {
    return new _definition.GraphQLUnionType({
      name: def.name.value,
      resolveType: function resolveType() {
        return null;
      },
      types: def.types.map(function (t) {
        return produceObjectType(t);
      })
    });
  }

  function makeScalarDef(def) {
    return new _definition.GraphQLScalarType({
      name: def.name.value,
      serialize: function serialize() {
        return null;
      },
      // Note: validation calls the parse functions to determine if a
      // literal value is correct. Returning null would cause use of custom
      // scalars to always fail validation. Returning false causes them to
      // always pass validation.
      parseValue: function parseValue() {
        return false;
      },
      parseLiteral: function parseLiteral() {
        return false;
      }
    });
  }

  function makeInputObjectDef(def) {
    return new _definition.GraphQLInputObjectType({
      name: def.name.value,
      fields: function fields() {
        return makeInputValues(def.fields);
      }
    });
  }
}

function getDeprecationReason(directives) {
  var deprecatedAST = directives && (0, _find2.default)(directives, function (directive) {
    return directive.name.value === _directives.GraphQLDeprecatedDirective.name;
  });
  if (!deprecatedAST) {
    return;
  }

  var _getArgumentValues = (0, _values.getArgumentValues)(_directives.GraphQLDeprecatedDirective.args, deprecatedAST.arguments);

  var reason = _getArgumentValues.reason;

  return reason;
}
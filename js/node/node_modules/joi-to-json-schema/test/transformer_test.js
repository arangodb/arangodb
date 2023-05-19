'use strict';

/*
 * Created by raisch on 3/17/15.
 */

/*jshint mocha:true, node:true, bitwise:true, camelcase:false, curly:true, undef:false, unused:false, eqeqeq:true, shadow:true */
/* global suite, test */

//@formatter:off
var Joi         = require('joi'),
    convert     = require('../src/index'),
    assert      = require('assert'),
    jsonSchema  = require('json-schema'),
    _           = require('lodash');
//@formatter:on

/**
 * Throws if schema !== expected or if schema fails to jsonSchema.validate()
 * @param {object} schema
 * @param {object} expected
 */
assert.validate = function (schema, expected) {
  var result = jsonSchema.validate(schema);
  assert.deepEqual(schema, expected);
  if ('object' === typeof result && Array.isArray(result.errors) && 0 === result.errors.length) {
    return;
  }
  throw new Error('json-schema validation failed: %s', result.errors.join(','));
};

/**
 * Removes null values from all arrays.
 * @param {object} obj - transformable object
 * @returns {*}
 */
var removeNullsFromArrays = function (obj) {
  var result;
  if (_.isArray(obj)) {
    result = [];
    for (var i = 0, len = obj.length; i < len; i++) {
      var val = obj[i];
      if (null !== val) {
        result.push(removeNullsFromArrays(val));
      }
    }
    return result;
  }
  else if (_.isObject(obj)) {
    result = {};
    _.keys(obj).forEach(function (key) {
      result[key] = removeNullsFromArrays(obj[key]);
    });
    return result;
  }
  else {
    return obj;
  }
};

suite('transform', function () {

  test('object defaults', function () {
    var joi = Joi.object(),
        transformer = function (obj) {
          obj.additionalProperties = true;
          return obj;
        },
        schema = convert(joi, transformer),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: true // false
        };
    assert.validate(schema, expected);
  });

  test('complicated', function () {

    var joi = Joi.object({
          name: Joi.string().required(),
          options: Joi.alternatives()
              .when('name', {
                is: 'foo',
                then: Joi.alternatives().try([
                  Joi.object({
                    name: Joi.string().allow([null, '', 'foo']).required(),
                    size: Joi.string().valid('2x4').required(),
                    value: Joi.string().required()
                  }),
                  Joi.object({
                    name: Joi.string().valid('foo').required(),
                    size: Joi.string().valid('4x8').required(),
                    value: Joi.number().min(11).max(20).required()
                  })
                ])
              })
              .when('name', {
                is: 'bar',
                then: Joi.string().regex(/^[a-z]+$/)
              })
              .when('name', {
                is: 'baz',
                then: Joi.string().regex(/^[A-Z]+$/)
              })
              .required()
        }),
        schema = convert(joi, removeNullsFromArrays),
        expected = require('./fixtures/transform.json');


    assert.validate(schema, expected);
  });

  test('recursive transforms', function () {
    var joi = Joi.object().keys({
          foo: Joi.object().keys({
            bar: Joi.string().meta({ jsonSchema: { propertyOrder: 100 } }),
          }),
        }),
        transformer = function (obj, joi) {
          if (joi._meta) {
            joi._meta.forEach(function(meta) {
              if (meta.jsonSchema) {
                obj = Object.assign({}, obj, meta.jsonSchema)
              }
            })
          }

          return obj;
        },
        schema = convert(joi, transformer),
        expected = {
          type: 'object',
          properties: {
            foo: {
              type: "object",
              properties: {
                bar: {
                  type: "string",
                  propertyOrder: 100
                }
              },
              patterns: [],
              additionalProperties: false
            },
          },
          patterns: [],
          additionalProperties: false
        };
    assert.validate(schema, expected);
  });
});

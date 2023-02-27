//@formatter:off
var Joi         = require('joi'),
    convert     = require('../src/index'),
    assert      = require('assert'),
    jsonSchema  = require('json-schema');
//@formatter:on

/* jshint mocha:true */
/* global suite, test */

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

suite('convert', function () {

  test('object defaults', function () {
    var joi = Joi.object(),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: false,
        };
    assert.validate(schema, expected);
  });

  test('object label', function () {
    var joi = Joi.object().label('Title'),
        schema = convert(joi),
        expected = {
          type: 'object',
          title: 'Title',
          properties: {},
          patterns: [],
          additionalProperties: false,
        };
    assert.validate(schema, expected);
  });

  test('object options language label', function () {
    var joi = Joi.object().options({language:{label: 'Title'}}),
        schema = convert(joi),
        expected = {
          type: 'object',
          title: 'Title',
          properties: {},
          patterns: [],
          additionalProperties: false,
        };
    assert.validate(schema, expected);
  });

  test('object description', function () {
    var joi = Joi.object().description('woot'),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: false,
          description: 'woot'
        };
    assert.validate(schema, expected);
  });

  test('object example', function () {
    var joi = Joi.object().example({ key: 'value' }),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: false,
          example: { key: 'value' },
          examples: [{ key: 'value' }]
        };
    assert.validate(schema, expected);
  });

  test('object without unknown keys', function () {
    var joi = Joi.object().unknown(false),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: false,
        };
    assert.validate(schema, expected);
  });

  test('object allow unknown', function () {
    var joi = Joi.object().unknown(true),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {},
          patterns: [],
          additionalProperties: true
        };
    assert.validate(schema, expected);
  });

  test('object', function () {
    let joi = Joi.object().keys({
          string: Joi.string(),
          'string default': Joi.string().default('bar').description('bar desc'),
          'number': Joi.number(),
          'boolean': Joi.boolean()
        }),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {
            'string': {
              type: 'string'
            },
            'string default': {
              type: 'string',
              'default': 'bar',
              description: 'bar desc'
            },
            'number': {
              type: 'number'
            },
            'boolean': {
              type: 'boolean'
            }
          },
          patterns: [],
          additionalProperties: false
        };
    assert.validate(schema, expected);
  });

  test('object property required', function () {
    let joi = Joi.object().keys({
          string: Joi.string(),
          'string default': Joi.string().default('bar').description('bar desc'),
          'number': Joi.number(),
          'boolean required': Joi.boolean().required()
        }),
        schema = convert(joi),
        expected = {
          type: 'object',
          required: ['boolean required'],
          properties: {
            'string': {
              type: 'string'
            },
            'string default': {
              type: 'string',
              'default': 'bar',
              description: 'bar desc'
            },
            'number': {
              type: 'number'
            },
            'boolean required': {
              type: 'boolean'
            }
          },
          patterns: [],
          additionalProperties: false
        };
    assert.validate(schema, expected);
  });

  test('object property forbidden', function(){
    let joi = Joi.object().keys({
          string: Joi.string(),
          'string default': Joi.string().default('bar').description('bar desc'),
          'number forbidden': Joi.number().forbidden(),
          'boolean required': Joi.boolean().required()
        }),
        schema = convert(joi),
        expected = {
          type: 'object',
          required: ['boolean required'],
          properties: {
            'string': {
              type: 'string'
            },
            'string default': {
              type: 'string',
              'default': 'bar',
              description: 'bar desc'
            },
            'boolean required': {
              type: 'boolean'
            }
          },
          patterns: [],
          additionalProperties: false,
        };
    assert.validate(schema, expected);
  });

  test('type: array', function () {
    var joi = Joi.array(),
        schema = convert(joi),
        expected = {
          type: 'array'
        };
    assert.validate(schema, expected);
  });

  test('enum', function () {
    var joi = Joi.string().valid(['a', 'b']),
        schema = convert(joi),
        expected = {
          'type': 'string',
          'enum': ['a', 'b']
        };
    //console.log('.enum: %s', util.inspect({type: joi._type, schema: schema}, {depth: 10}));
    assert.validate(schema, expected);
  });

  test('alternatives -> oneOf', function () {

    let joi = Joi.object().keys({
          value: Joi.alternatives().try(
              Joi.string().valid('a'),
              Joi.number().valid(100)
          )
        }),
        schema = convert(joi),
        expected = {
          type: 'object',
          patterns: [],
          additionalProperties: false,
          properties: {
            value: {
              oneOf: [
                {
                  type: 'string',
                  'enum': ['a']
                },
                {
                  type: 'number',
                  'enum': [100]
                }
              ]
            }
          }
        };

    //console.log('alt -> oneOf: %s', util.inspect({type: joi._type, schema: schema}, {depth: 10}));
    assert.validate(schema, expected);
  });

  test('string min/max', function () {
    var joi = Joi.string().min(5).max(100),
        schema = convert(joi),
        expected = {
          type: 'string',
          minLength: 5,
          maxLength: 100
        };
    assert.validate(schema, expected);
  });

  test('string -> maxLength', function () {
    var joi = Joi.string().length(5),
        schema = convert(joi),
        expected = {
          type: 'string',
          maxLength: 5,
          minLength: 5
        };
    assert.validate(schema, expected);
  });

  test('string email', function () {
    var joi = Joi.string().email(),
        schema = convert(joi),
        expected = {
          type: 'string',
          format: 'email'
        };
    assert.validate(schema, expected);
  });

  test('string uri', function () {
    var joi = Joi.string().uri(),
        schema = convert(joi),
        expected = {
          type: 'string',
          format: 'uri'
        };
    assert.validate(schema, expected);
  });

  test('date', function () {
    var joi = Joi.date(),
        schema = convert(joi),
        expected = {
          type: 'string',
          format: 'date-time'
        };
    assert.validate(schema, expected);
  });

  test('date (javascript timestamp)', function () {
    var joi = Joi.date().timestamp(),
        schema = convert(joi),
        expected = {
          type: 'integer',
        };
    assert.validate(schema, expected);
  });

  test('date (unix timestamp)', function () {
    var joi = Joi.date().timestamp('unix'),
      schema = convert(joi),
      expected = {
        type: 'integer',
      };
    assert.validate(schema, expected);
  });

  test('string regex -> pattern', function () {
    let joi = Joi.string().regex(/^[a-z]$/),
        schema = convert(joi),
        expected = {
          type: 'string',
          pattern: '^[a-z]$'
        };
    assert.validate(schema, expected);
  });

  test('string allow', function () {
    let joi = Joi.string().allow(['a', 'b', '', null]),
        schema = convert(joi),
        expected = {
          "anyOf": [
            {
              enum: [
                'a',
                'b',
                '',
                null,
              ],
              type: 'string'
            },
            {
              type: 'string'
            }
          ]
        };
    //console.log('string allow: %s', util.inspect({type: joi._type, joi:joi, schema: schema}, {depth: 10}));
    assert.validate(schema, expected);
  });

  test('number min/max', function () {
    let joi = Joi.number().min(0).max(100),
        schema = convert(joi),
        expected = {
          type: 'number',
          minimum: 0,
          maximum: 100
        };
    assert.validate(schema, expected);
  });

  test('number greater/less', function () {
    let joi = Joi.number().greater(0).less(100),
        schema = convert(joi),
        expected = {
          type: 'number',
          minimum: 0,
          exclusiveMinimum: true,
          maximum: 100,
          exclusiveMaximum: true
        };
    assert.validate(schema, expected);
  });

  test('number precision', function () {
    let joi = Joi.number().precision(2),
        schema = convert(joi),
        expected = {
          type: 'number',
          multipleOf: 0.01
        };
    assert.validate(schema, expected);
  });

  test('integer', function () {
    var joi = Joi.number().integer(),
        schema = convert(joi),
        expected = {
          type: 'integer'
        };
    assert.validate(schema, expected);
  });

  test('array min/max', function () {
    let joi = Joi.array().min(5).max(100),
        schema = convert(joi),
        expected = {
          type: 'array',
          minItems: 5,
          maxItems: 100
        };
    assert.validate(schema, expected);
  });

  test('array length', function () {
    let joi = Joi.array().length(100),
        schema = convert(joi),
        expected = {
          type: 'array',
          minItems: 100,
          maxItems: 100
        };
    assert.validate(schema, expected);
  });

  test('array unique', function () {
    let joi = Joi.array().unique(),
        schema = convert(joi),
        expected = {
          type: 'array',
          uniqueItems: true
        };
    assert.validate(schema, expected);
  });

  test('array inclusions', function () {
    let joi = Joi.array().items(Joi.string()),
        schema = convert(joi),
        expected = {
          type: 'array',
          items: {type: 'string'}
        };
    assert.validate(schema, expected);
  });

  test('array ordered (tuple-like)', function () {
    let joi = Joi.array().ordered(Joi.string().required(), Joi.number().optional()),
        schema = convert(joi),
        expected = {
          type: 'array',
          ordered: [{type: 'string'}, {type: 'number'}]
        };
    assert.validate(schema, expected);
  });

  test('joi any', function () {
    let joi = Joi.any(),
        schema = convert(joi),
        expected = {
          type: ['array', 'boolean', 'number', 'object', 'string', 'null']
        };
    assert.validate(schema, expected);
  });

  test('joi binary with content encoding', function () {
    let joi = Joi.binary().encoding('base64'),
      schema = convert(joi),
      expected = {
        type: 'string',
        contentMediaType: 'text/plain',
        contentEncoding: 'base64'
      };
    assert.validate(schema, expected);
  });

  test('joi binary with content type', function () {
    let joi = Joi.binary().meta({ contentMediaType: 'image/png' }),
      schema = convert(joi),
      expected = {
        type: 'string',
        contentMediaType: 'image/png',
        contentEncoding: 'binary'
      };
    assert.validate(schema, expected);
  });

  test('big and complicated', function () {
    let joi = Joi.object({
          aFormattedString: Joi.string().regex(/^[ABC]_\w+$/),
          aFloat: Joi.number().default(0.8).min(0.0).max(1.0),
          anInt: Joi.number().required().precision(0).greater(0),
          aForbiddenString: Joi.string().forbidden(),
          anArrayOfFloats: Joi.array().items(Joi.number().default(0.8).min(0.0).max(1.0)),
          anArrayOfNumbersOrStrings: Joi.array().items(Joi.alternatives(Joi.number(), Joi.string()))
        }),
        schema = convert(joi),
        expected = require('./fixtures/complicated.json');

    assert.validate(schema, expected);

    // now make it fail
    expected.properties.aForbiddenString={type:'string'};

    try {
      assert.validate(schema,expected);
    }
    catch(e){
      //console.warn(e);
      if(e.name !== 'AssertionError' && e.operator !== 'deepEqual'){
        throw e;
      }
    }

  });

  test('joi.when', function () {
    let joi = Joi.object({
          'a': Joi.boolean().required().default(false),
          'b': Joi.alternatives().when('a', {
            is: true,
            then: Joi.string().default('a is true'),
            otherwise: Joi.number().default(0)
          })
        }),
        schema = convert(joi),
        expected = {
          type: 'object',
          properties: {
            a: {
              type: 'boolean',
              default: false
            },
            b: {
              oneOf: [
                {
                  'default': 'a is true',
                  type: 'string'
                }, {
                  type: 'number',
                  default: 0
                }
              ]
            }
          },
          patterns: [],
          additionalProperties: false,
          required: ['a']
        };
    //console.log('when: %s', util.inspect({type:joi._type,schema:schema}, {depth: 10}));
    assert.validate(schema, expected);
  });

});

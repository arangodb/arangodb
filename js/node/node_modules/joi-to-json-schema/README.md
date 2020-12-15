# joi-to-json-schema

The goal is to provide best effort conversion from Joi objects to JSON
Schema (draft-04) with the understanding that only some of Joi's schematics 
can be converted directly. Primarily this module exists to convert Joi schema 
objects for existing tools which happen to currently consume JSON Schema.

[![npm version](https://badge.fury.io/js/joi-to-json-schema.svg)](http://badge.fury.io/js/joi-to-json-schema)
[![Build Status](https://travis-ci.org/lightsofapollo/joi-to-json-schema.svg?branch=master)](https://travis-ci.org/lightsofapollo/joi-to-json-schema)
[![Dependencies Status](https://david-dm.org/lightsofapollo/joi-to-json-schema.svg)](https://david-dm.org/lightsofapollo/joi-to-json-schema)
[![DevDependencies Status](https://david-dm.org/lightsofapollo/joi-to-json-schema/dev-status.svg)](https://david-dm.org/lightsofapollo/joi-to-json-schema#info=devDependencies)

[![NPM](https://nodei.co/npm/joi-to-json-schema.png)](https://nodei.co/npm/joi-to-json-schema/)
[![NPM](https://nodei.co/npm-dl/joi-to-json-schema.png)](https://nodei.co/npm-dl/joi-to-json-schema/)


## Installation
> npm install joi-to-json-schema



## Usage

```js
var joi = require('joi'),
    convert = require('joi-to-json-schema'),
    joiSchema = joi.object({
      'name': joi.string().required().regex(/^\w+$/),
      'description': joi.string().optional().default('no description provided'),
      'a': joi.boolean().required().default(false),
      'b': joi.alternatives().when('a', {
        is: true,
        then: joi.string().default('a is true'),
        otherwise: joi.number().default(0)
      })
    });

convert(joiSchema);
```

which will produce:

```js
{ type: 'object',
  properties: 
   { name: { type: 'string', pattern: '^\\w+$' },
     description: { default: 'no description provided', type: 'string' },
     a: { type: 'boolean', default: false },
     b: { oneOf: [ { default: 'a is true', type: 'string' }, { type: 'number', default: 0 } ] } },
  additionalProperties: false,
  required: [ 'name', 'a' ] }
```

## JSDOC

```javascript
 /**
  * Converts the supplied joi validation object into a JSON schema object,
  * optionally applying a transformation.
  *
  * @param {JoiValidation} joi
  * @param {TransformFunction} [transformer=null]
  * @returns {JSONSchema}
  */
 export default function convert(joi,transformer=null) {
   // ...
 };

 /**
  * Joi Validation Object
  * @typedef {object} JoiValidation
  */

 /**
  * Transformation Function - applied just before `convert()` returns and called as `function(object):object`
  * @typedef {function} TransformFunction
  */

 /**
  * JSON Schema Object
  * @typedef {object} JSONSchema
  */
```

## Notes

Joi's conditional form, i.e. `.when('name',{is:cond,then:joi,otherwise:joi})`, is evaluated at runtime 
and since, from the perspective of the schema, there is no way of knowing what the condition might resolve to, this
module takes the position that it should provide _all possible resolutions_ in a JSON Schema `oneOf:[]` clause.

## Testing

All tests cases are first checked against expected results and then validated using
Kris Zyp's excellent [json-schema](https://github.com/kriszyp/json-schema)

## References

- [JSON Schema - Draft 4 from json-schema.org](http://json-schema.org/documentation.html)
- [IETF Draft: JSON Schema: core definitions and terminology - draft-zyp-json-schema-04](https://tools.ietf.org/html/draft-zyp-json-schema-04)
- [Understanding JSON Schema](http://spacetelescope.github.io/understanding-json-schema/UnderstandingJSONSchema.pdf)(pdf)

## LICENSE

Copyright 2014, Mozilla Foundation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

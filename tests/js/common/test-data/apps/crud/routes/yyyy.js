'use strict';
const dd = require('dedent');
const joi = require('joi');
const httpError = require('http-errors');
const status = require('statuses');
const errors = require('@arangodb').errors;
const createRouter = require('@arangodb/foxx/router');
const Yyyy = require('../models/yyyy');

const yyyyItems = module.context.collection('yyyy');
const keySchema = joi.string().required()
.description('The key of the yyyy');

const ARANGO_NOT_FOUND = errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;
const ARANGO_DUPLICATE = errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
const ARANGO_CONFLICT = errors.ERROR_ARANGO_CONFLICT.code;
const HTTP_NOT_FOUND = status('not found');
const HTTP_CONFLICT = status('conflict');

const router = createRouter();
module.exports = router;


router.tag('yyyy');


const NewYyyy = Object.assign({}, Yyyy, {
  schema: Object.assign({}, Yyyy.schema, {
    _from: joi.string(),
    _to: joi.string()
  })
});


router.get(function (req, res) {
  res.send(yyyyItems.all());
}, 'list')
.response([Yyyy], 'A list of yyyyItems.')
.summary('List all yyyyItems')
.description(dd`
  Retrieves a list of all yyyyItems.
`);


router.post(function (req, res) {
  const yyyy = req.body;
  let meta;
  try {
    meta = yyyyItems.save(yyyy._from, yyyy._to, yyyy);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_DUPLICATE) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  Object.assign(yyyy, meta);
  res.status(201);
  res.set('location', req.makeAbsolute(
    req.reverse('detail', {key: yyyy._key})
  ));
  res.send(yyyy);
}, 'create')
.body(NewYyyy, 'The yyyy to create.')
.response(201, Yyyy, 'The created yyyy.')
.error(HTTP_CONFLICT, 'The yyyy already exists.')
.summary('Create a new yyyy')
.description(dd`
  Creates a new yyyy from the request body and
  returns the saved document.
`);


router.get(':key', function (req, res) {
  const key = req.pathParams.key;
  let yyyy
  try {
    yyyy = yyyyItems.document(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    throw e;
  }
  res.send(yyyy);
}, 'detail')
.pathParam('key', keySchema)
.response(Yyyy, 'The yyyy.')
.summary('Fetch a yyyy')
.description(dd`
  Retrieves a yyyy by its key.
`);


router.put(':key', function (req, res) {
  const key = req.pathParams.key;
  const yyyy = req.body;
  let meta;
  try {
    meta = yyyyItems.replace(key, yyyy);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    if (e.isArangoError && e.errorNum === ARANGO_CONFLICT) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  Object.assign(yyyy, meta);
  res.send(yyyy);
}, 'replace')
.pathParam('key', keySchema)
.body(Yyyy, 'The data to replace the yyyy with.')
.response(Yyyy, 'The new yyyy.')
.summary('Replace a yyyy')
.description(dd`
  Replaces an existing yyyy with the request body and
  returns the new document.
`);


router.patch(':key', function (req, res) {
  const key = req.pathParams.key;
  const patchData = req.body;
  let yyyy;
  try {
    yyyyItems.update(key, patchData);
    yyyy = yyyyItems.document(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    if (e.isArangoError && e.errorNum === ARANGO_CONFLICT) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  res.send(yyyy);
}, 'update')
.pathParam('key', keySchema)
.body(joi.object().description('The data to update the yyyy with.'))
.response(Yyyy, 'The updated yyyy.')
.summary('Update a yyyy')
.description(dd`
  Patches a yyyy with the request body and
  returns the updated document.
`);


router.delete(':key', function (req, res) {
  const key = req.pathParams.key;
  try {
    yyyyItems.remove(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    throw e;
  }
}, 'delete')
.pathParam('key', keySchema)
.response(null)
.summary('Remove a yyyy')
.description(dd`
  Deletes a yyyy from the database.
`);

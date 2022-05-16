'use strict';
const dd = require('dedent');
const joi = require('joi');
const httpError = require('http-errors');
const status = require('statuses');
const errors = require('@arangodb').errors;
const createRouter = require('@arangodb/foxx/router');
const Xxx = require('../models/xxx');

const xxxItems = module.context.collection('xxx');
const keySchema = joi.string().required()
.description('The key of the xxx');

const ARANGO_NOT_FOUND = errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;
const ARANGO_DUPLICATE = errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
const ARANGO_CONFLICT = errors.ERROR_ARANGO_CONFLICT.code;
const HTTP_NOT_FOUND = status('not found');
const HTTP_CONFLICT = status('conflict');

const router = createRouter();
module.exports = router;


router.tag('xxx');


router.get(function (req, res) {
  res.send(xxxItems.all());
}, 'list')
.response([Xxx], 'A list of xxxItems.')
.summary('List all xxxItems')
.description(dd`
  Retrieves a list of all xxxItems.
`);


router.post(function (req, res) {
  const xxx = req.body;
  let meta;
  try {
    meta = xxxItems.save(xxx);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_DUPLICATE) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  Object.assign(xxx, meta);
  res.status(201);
  res.set('location', req.makeAbsolute(
    req.reverse('detail', {key: xxx._key})
  ));
  res.send(xxx);
}, 'create')
.body(Xxx, 'The xxx to create.')
.response(201, Xxx, 'The created xxx.')
.error(HTTP_CONFLICT, 'The xxx already exists.')
.summary('Create a new xxx')
.description(dd`
  Creates a new xxx from the request body and
  returns the saved document.
`);


router.get(':key', function (req, res) {
  const key = req.pathParams.key;
  let xxx
  try {
    xxx = xxxItems.document(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    throw e;
  }
  res.send(xxx);
}, 'detail')
.pathParam('key', keySchema)
.response(Xxx, 'The xxx.')
.summary('Fetch a xxx')
.description(dd`
  Retrieves a xxx by its key.
`);


router.put(':key', function (req, res) {
  const key = req.pathParams.key;
  const xxx = req.body;
  let meta;
  try {
    meta = xxxItems.replace(key, xxx);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    if (e.isArangoError && e.errorNum === ARANGO_CONFLICT) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  Object.assign(xxx, meta);
  res.send(xxx);
}, 'replace')
.pathParam('key', keySchema)
.body(Xxx, 'The data to replace the xxx with.')
.response(Xxx, 'The new xxx.')
.summary('Replace a xxx')
.description(dd`
  Replaces an existing xxx with the request body and
  returns the new document.
`);


router.patch(':key', function (req, res) {
  const key = req.pathParams.key;
  const patchData = req.body;
  let xxx;
  try {
    xxxItems.update(key, patchData);
    xxx = xxxItems.document(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    if (e.isArangoError && e.errorNum === ARANGO_CONFLICT) {
      throw httpError(HTTP_CONFLICT, e.message);
    }
    throw e;
  }
  res.send(xxx);
}, 'update')
.pathParam('key', keySchema)
.body(joi.object().description('The data to update the xxx with.'))
.response(Xxx, 'The updated xxx.')
.summary('Update a xxx')
.description(dd`
  Patches a xxx with the request body and
  returns the updated document.
`);


router.delete(':key', function (req, res) {
  const key = req.pathParams.key;
  try {
    xxxItems.remove(key);
  } catch (e) {
    if (e.isArangoError && e.errorNum === ARANGO_NOT_FOUND) {
      throw httpError(HTTP_NOT_FOUND, e.message);
    }
    throw e;
  }
}, 'delete')
.pathParam('key', keySchema)
.response(null)
.summary('Remove a xxx')
.description(dd`
  Deletes a xxx from the database.
`);

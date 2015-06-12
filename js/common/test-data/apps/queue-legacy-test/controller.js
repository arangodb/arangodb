'use strict';
var Foxx = require('org/arangodb/foxx');
var queue = Foxx.queues.get('default');
var ctrl = new Foxx.Controller(applicationContext);

ctrl.post('/', function (req, res) {
  try {
    var jobId = queue.push('queue-legacy-test', req.body(), {
      allowUnknown: req.parameters.allowUnknown,
      success: function (id, data) {
        var collectionName = 'test__queue_test_data';
        var db = require('org/arangodb').db;
        db._collection(collectionName).save(data);
      }
    });
    res.json({success: true, job: jobId});
  } catch (e) {
    res.status = 418;
    res.json({success: false, error: e.name, stacktrace: e.stack});
  }
});

ctrl.get('/', function (req, res) {
  var db = require('org/arangodb').db;
  var collectionName = 'test__queue_test_data';
  res.json(db._collection(collectionName).all().toArray());
});
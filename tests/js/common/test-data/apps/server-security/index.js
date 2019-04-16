'use strict';
const router = require('@arangodb/foxx/router')();
const internal = require('internal');
module.context.use(router);

router.get('/pid', function (req, res) {
  res.json(internal.getPid());
});

router.get('/passwd', function (req, res) {
  res.json(require('fs').read('/etc/passwd'));
});

router.get('/dl-heise', function (req, res) {
  res.json(internal.download("https://heise.de:443"));
});

router.get('/test-port', function (req, res) {
  res.json(internal.testPort("tcp://localhost:111"));
});

router.get('/get-tmp-path', function (req, res) {
  res.json(require('fs').getTempPath());
});

router.get('/get-tmp-file', function (req, res) {
  res.json(require('fs').getTempFile());
});

router.get('/write-tmp-file', function (req, res) {
  const fs = require('fs');
  let file = fs.getTempFile();
  fs.write(file,"ULF");
  res.json(true);
});

router.get('/process-statistics', function (req, res) {
  res.json(internal.processStatistics());
});

router.get('/execute-external', function (req, res) {
  res.json(internal.executeExternal('/bin/ls'));
});

router.get('/environment-variables-get-path', function (req, res) {
  let env = require('process').env;
  res.json(env['PATH']);
});

router.get('/environment-variables-set-path', function (req, res) {
  let env = require('process').env;
  env['PATH'] = 'der reiher';
  res.json(true);
});

router.get('/startup-options-log-file', function (req, res) {
  res.json(internal.options()['log.file']);
});


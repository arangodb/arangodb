#!/usr/bin/env node


'use strict';

var fs   = require('fs');
var yaml = require('../');

var data = fs.readFileSync(__dirname +'/samples/document_nodeca_application.yaml', 'utf8');

for (var i=0; i<10000; i++) {
  yaml.load(data);
}

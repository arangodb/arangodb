#!/usr/bin/env node

'use strict';


var path      = require('path');
var fs        = require('fs');
var util      = require('util');
var Benchmark = require('benchmark');
var ansi      = require('ansi');
var cursor    = ansi(process.stdout);


var IMPLS_DIRECTORY = path.join(__dirname, 'implementations');
var IMPLS_PATHS = {};
var IMPLS = [];


fs.readdirSync(IMPLS_DIRECTORY).sort().forEach(function (name) {
  var file = path.join(IMPLS_DIRECTORY, name),
      code = require(file);

  IMPLS_PATHS[name] = file;
  IMPLS.push({
    name: name,
    run: code.load
  });
});


var SAMPLES_DIRECTORY = path.join(__dirname, 'samples');
var SAMPLES = [];


fs.readdirSync(SAMPLES_DIRECTORY).sort().forEach(function (sample) {
  var filepath = path.join(SAMPLES_DIRECTORY, sample),
      extname  = path.extname(filepath),
      basename = path.basename(filepath, extname),
      content  = fs.readFileSync(filepath, 'utf8'),
      title    = util.format('%s (%d characters)', sample, content.length);

  function onComplete() {
    cursor.write('\n');
  }

  var suite = new Benchmark.Suite(title, {

    onStart: function onStart() {
      console.log('\nSample: %s', title);
    },

    onComplete: onComplete

  });


  IMPLS.forEach(function (impl) {
    suite.add(impl.name, {

      onCycle: function onCycle(event) {
        cursor.horizontalAbsolute();
        cursor.eraseLine();
        cursor.write(' > ' + event.target);
      },

      onComplete: onComplete,

      fn: function () { impl.run(content); }
    });
  });


  SAMPLES.push({
    name: basename,
    title: title,
    content: content,
    suite: suite
  });
});


function select(patterns) {
  var result = [];

  if (!(patterns instanceof Array)) {
    patterns = [ patterns ];
  }

  function checkName(name) {
    return patterns.length === 0 || patterns.some(function (regexp) {
      return regexp.test(name);
    });
  }

  SAMPLES.forEach(function (sample) {
    if (checkName(sample.name)) {
      result.push(sample);
    }
  });

  return result;
}


function run(files) {
  var selected = select(files);

  if (selected.length > 0) {
    console.log('Selected samples: (%d of %d)', selected.length, SAMPLES.length);
    selected.forEach(function (sample) {
      console.log(' > %s', sample.name);
    });
  } else {
    console.log("There isn't any sample matches any of these patterns: %s", util.inspect(files));
  }

  selected.forEach(function (sample) {
    sample.suite.run();
  });
}


run(process.argv.slice(2).map(function (source) {
  return new RegExp(source, 'i');
}));

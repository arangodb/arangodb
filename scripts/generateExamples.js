/*jshint globalstrict:false, unused:false */
/*global start_pretty_print */

var fs = require("fs");
var internal = require("internal");
var executeExternalAndWait = internal.executeExternalAndWait;
var print = internal.print;

var documentationSourceDirs = [
  fs.join(fs.makeAbsolute(''), "Documentation/Examples/setup-arangosh.js"),
  fs.join(fs.makeAbsolute(''), "js/actions"),
  fs.join(fs.makeAbsolute(''), "js/client"),
  fs.join(fs.makeAbsolute(''), "js/common"),
  fs.join(fs.makeAbsolute(''), "js/server"),
  fs.join(fs.makeAbsolute(''), "js/apps/system/_api/gharial/APP"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/Users"),
  fs.join(fs.makeAbsolute(''), "arangod/RestHandler"),
  fs.join(fs.makeAbsolute(''), "arangod/V8Server")];


	

var theScript = 'Documentation/Scripts/generateExamples.py';

var scriptArguments = {
  'outputDir': fs.join(fs.makeAbsolute(''), "Documentation/Examples"),
  'outputFile': '/tmp/arangosh.examples.js'
};

function main (argv) {
  "use strict";
  var thePython = 'python';
  var test = argv[1];
  var options = {};
  var serverEndpoint = '';
  var runLocally = true;

  try {
    options = internal.parseArgv(argv, 1);
  }
  catch (x) {
    print("failed to parse the options: " + x.message);
    return -1;
  }

  if (options.hasOwnProperty('withPython')) {
    thePython = options.withPython;
  }

  if (options.hasOwnProperty('onlyThisOne')) {
    scriptArguments.onlyThisOne = options.onlyThisOne;
  }

  if (options.hasOwnProperty('server.endpoint')) {
    runLocally = false;
    serverEndpoint = options['server.endpoint'];
  }
  var args = [theScript].concat(internal.toArgv(scriptArguments));
  args = args.concat(['--arangoshSetup']);
  args = args.concat(documentationSourceDirs);

  internal.print(JSON.stringify(args));

  var res = executeExternalAndWait(thePython, args);


  var arangoshArgs = {
    'configuration': fs.join(fs.makeAbsolute(''), 'etc', 'relative', 'arangosh.conf'),
    'server.password': "",
    'server.endpoint': serverEndpoint,
    'javascript.execute': scriptArguments.outputFile
  };

  res = executeExternalAndWait('bin/arangosh', internal.toArgv(arangoshArgs));
  return 0;
}

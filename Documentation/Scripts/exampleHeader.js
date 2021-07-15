/*jshint strict:false, esnext:true  */
/*global db */
// 'use strict'
/*exported
 time,
 fs,
 ArangoshOutput,
 testFunc,
 countErrors,
 rc,
 log,
 logCurlRequest,
 curlRequest,
 logJsonResponse,
 logRawResponse,
 logErrorResponse,
 globalAssert,
 runTestLine,
 runTestFunc,
 runTestFuncCatch,
 addIgnoreCollection,
 removeIgnoreCollection,
 checkIgnoreCollectionAlreadyThere,
 text
*/

var internal = require('internal');
var print = require('@arangodb').print;
var errors = require("@arangodb").errors;
var time = require("internal").time;
var fs = require('fs');
var hashes = '################################################################################';
var ArangoshOutput = {};
var allErrors = '';
var output = '';
var XXX = '';
var testFunc;
var countErrors;
var collectionAlreadyThere = [];
var ignoreCollectionAlreadyThere = [];
var rc;
var j;

const exds = require("@arangodb/examples/examples").Examples;

const AU = require('ansi_up');
const ansi_up = new AU.default;

const hljs = require('highlight.js');

const MAP = {
    'py': 'python',
    'js': 'javascript',
    'json': 'javascript',
    'rb': 'ruby',
    'csharp': 'cs',
};

function normalize(lang) {
    if(!lang) { return null; }

    var lower = lang.toLowerCase();
    return MAP[lower] || lower;
}

function highlight(lang, code) {
  if(!lang) {
    return code;
  }
  // Normalize lang
  lang = normalize(lang);

  try {
    return hljs.highlight(lang, code).value;
  } catch(e) { }

  return code;
}


internal.startPrettyPrint(true);
internal.stopColorPrint(true);
var appender = function(text) {
  output += text;
};
const ansiAppender = (text) => {
  output += ansi_up.ansi_to_html(text);
};
const jsonAppender = function(text) {
  output += highlight("js", text);
};
const jsonLAppender = function(text) {
  output += highlight("js", text) + "&#x21A9;\n" ;
};
const htmlAppender = function(text) {
  output += highlight("html", text);
};
const rawAppender = function(text) {
  output += text;
};

const plainAppender = function(text) {
  // do we have a line that could be json? try to parse & format it.
  if (text.match(/^{.*}$/) || text.match(/^[.*]$/)) {
    try {
      let parsed = JSON.parse(text);
      output += highlight("js", internal.inspect(parsed)) + "&#x21A9;\n" ;
    } catch (x) {
      // fallback to plain text.
      output += text;
    }
  } else {
    output += text;
  }
};

const shellAppender = function(text) {
  output += highlight("shell", text);
};
const log = function (a) {
  internal.startCaptureMode();
  print(a);
  appender(internal.stopCaptureMode());
};

var logCurlRequestRaw = internal.appendCurlRequest(shellAppender, jsonAppender, rawAppender);
var logCurlRequestPlain = internal.appendCurlRequest(shellAppender, jsonAppender, plainAppender);
var logCurlRequest = function () {
  if ((arguments.length > 1) &&
      (arguments[1] !== undefined) &&
      (arguments[1].length > 0) &&
      (arguments[1][0] !== '/')) {
      throw new Error("your URL doesn't start with a /! the example will be broken. [" + arguments[1] + "]");
  }
  var r = logCurlRequestRaw.apply(logCurlRequestRaw, arguments);
  db._collections();
  return r;
};

var swallowText = function () {};
var curlRequestRaw = internal.appendCurlRequest(swallowText, swallowText, swallowText);
var curlRequest = function () {
  rc = curlRequestRaw.apply(curlRequestRaw, arguments);
  if (rc.code != 200) {
    expectRC = arguments["4"];
    if (typeof expectRC !== undefined) {
      if (expectRC.indexOf(rc.code) >=0) {
        return rc;
      }
    }
    throw rc.code + " " + rc.errorMessage
  }
  return rc
};
var logJsonResponse = internal.appendJsonResponse(rawAppender, jsonAppender);
var logJsonLResponse = internal.appendJsonLResponse(rawAppender, jsonLAppender);
var logHtmlResponse = internal.appendRawResponse(rawAppender, htmlAppender);
var logRawResponse = internal.appendRawResponse(rawAppender, rawAppender);
var logPlainResponse = internal.appendPlainResponse(plainAppender, plainAppender);
var logErrorResponse = function (response) {
    allErrors += "Server reply was: " + JSON.stringify(response) + "\n";
};
var globalAssert = function(condition, testname, sourceFile) {
  if (! condition) {
    internal.output(hashes + '\nASSERTION FAILED: ' + testname + ' in file ' + sourceFile + '\n' + hashes + '\n');
    throw new Error('assertion ' + testname + ' in file ' + sourceFile + ' failed');
  }
};

var createErrorMessage = function(err, line, testName, sourceFN, sourceLine, lineCount, msg) {
 allErrors += '\n' + hashes + '\n';
 allErrors += "While executing '" + line + "' - " +
      testName + 
      "[" + sourceFN + ":" + sourceLine + "] Testline: " + lineCount +
      msg + "\n" + err + "\n" + err.stack;
};

var runTestLine = function(line, testName, sourceFN, sourceLine, lineCount, showCmd, expectError, isLoop, fakeVar) {
  XXX = undefined;
  if (showCmd) {
    print("arangosh> " + (fakeVar?"var ":"") + line.replace(/\n/g, '\n........> '));
  }
  if ((expectError !== undefined) && !errors.hasOwnProperty(expectError)) {
    createErrorMessage(new Error(), line, testName, sourceFN, sourceLine, lineCount,
                       " unknown Arangoerror " + expectError);
    return;
  }
  try {
    // Only care for result if we have to output it
/* jshint ignore:start */
    if (!showCmd || isLoop) {
      eval(line);
    } else {
      eval("XXX = " + line);
    }
/* jshint ignore:end */
    if (expectError !== undefined) {
      throw new Error("expected to throw with " + expectError + " but didn't!");
    }
  }
  catch (err) {
    if (expectError !== undefined) {
      if (err.errorNum === errors[expectError].code) {
        print(err);
      } else {
        print(err);
        createErrorMessage(err, line, testName, sourceFN, sourceLine, lineCount, " caught unexpected exception!");
      }
    } else {
      createErrorMessage(err, line, testName, sourceFN, sourceLine, lineCount, " caught an exception!\n");
      print(err);
    }
  }
  if (showCmd && XXX !== undefined) {
    print(XXX);
  }
};

var runTestFunc = function (execFunction, testName, sourceFile) {
  try {
    execFunction();
    return('done with  ' + testName);
  } catch (err) {
    allErrors += '\nRUN FAILED: ' + testName + ' from testfile: ' + sourceFile + ', ' + err + '\n' + err.stack + '\n';
    return hashes + '\nfailed with  ' + testName + ', ' + err + '\n' + hashes;
  }
};

var runTestFuncCatch = function (execFunction, testName, expectError) {
  try {
    execFunction();
    throw new Error(testName + ': expected to throw '+ expectError + ' but didn\'t throw');
  } catch (err) {
    if (err.num !== expectError.code) {
      allErrors += '\nRUN FAILED: ' + testName + ', ' + err + '\n' + err.stack + '\n';
      return hashes + '\nfailed with  ' + testName + ', ' + err + '\n' + hashes;
    }
  }
};

var checkForOrphanTestCollections = function(msg) {
  const colsAndViews = db._collections().concat(db._views());
  var cols = colsAndViews.map(function(c){
      return c.name();
  });
  var orphanColls = [];
  var i;
  for (i = 0; i < cols.length; i++) {
     if (cols[i][0] !== '_') {
       var found = false;
       var j = 0;
       for (j=0; j < collectionAlreadyThere.length; j++) {
         if (collectionAlreadyThere[j] === cols[i]) {
            found = true;
         }
       }
       if (!found) {
          orphanColls.push(cols[i]);
          collectionAlreadyThere.push(cols[i]);
       }
     }
  }

  if (orphanColls.length > 0) {
    allErrors += msg + ' - ' + JSON.stringify(orphanColls) + '\n';
  }
};

var addIgnoreCollection = function(collectionName) {
  // print("from now on ignoring this collection whether its dropped: "  + collectionName);
  collectionAlreadyThere.push(collectionName);
  ignoreCollectionAlreadyThere.push(collectionName);
};

var addIgnoreView = function(viewName) {
  addIgnoreCollection(viewName);
};

var removeIgnoreCollection = function(collectionName) {
  // print("from now on checking again whether this collection dropped: " + collectionName);
  for (j = 0; j < collectionAlreadyThere.length; j++) {
    if (collectionAlreadyThere[j] === collectionName) {
      collectionAlreadyThere[j] = undefined;
    }
  }
  for (j = 0; j < ignoreCollectionAlreadyThere.length; j++) {
    if (ignoreCollectionAlreadyThere[j] === collectionName) {
      ignoreCollectionAlreadyThere[j] = undefined;
    }
  }

};

var removeIgnoreView = function (viewName) {
  removeIgnoreCollection(viewName);
};

var checkIgnoreCollectionAlreadyThere = function () {
  if (ignoreCollectionAlreadyThere.length > 0) {
    allErrors += "some temporarily ignored collections haven't been cleaned up: " +
                 ignoreCollectionAlreadyThere;
  }

};

// Set the first available list of already there collections:
var err = allErrors;
checkForOrphanTestCollections('Collections or views already there which we will ignore from now on:');
print(allErrors + '\n');
allErrors = err;

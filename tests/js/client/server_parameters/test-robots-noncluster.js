/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');

const robotsContent = `
User-agent: Googlebot
Disallow: /nogooglebot/

User-agent: *
Allow: /
`;

if (getOptions === true) {
  let file = fs.getTempFile() + "-robots.txt";
  fs.writeFileSync(file, robotsContent);
  return {
    'server.robots-file': file,
  };
}

const jsunity = require('jsunity');

function TestSuite() {
  'use strict';

  return {
    testContent: function() {
      let res = arango.GET_RAW("/robots.txt");

      assertFalse(res.error);
      assertEqual(200, res.code);
      assertEqual(res.headers["content-type"], "text/plain");
      assertEqual(res.body. robotsContent);
    },

  };
}

jsunity.run(TestSuite);
return jsunity.done();

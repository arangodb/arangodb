/*global assertTrue */

"use strict";

var jsunity = require("jsunity");

var aqlFuncs = {
  "IS_NULL":".",
  "IS_BOOL":".",
  "IS_NUMBER":".",
  "IS_STRING":".",
  "IS_ARRAY":".",
  "IS_LIST":".",
  "IS_OBJECT":".",
  "IS_DOCUMENT":".",
  "IS_DATESTRING":".",
  "TO_NUMBER":".",
  "TO_STRING":".",
  "TO_BOOL":".",
  "TO_ARRAY":".",
  "TO_LIST":".",
  "CONCAT":"szl|+",
  "CHAR_LENGTH":"s",
  "LOWER":"s",
  "UPPER":"s",
  "SUBSTRING":"s,n|n",
  "CONTAINS":"s,s|b",
  "LIKE":"s,r|b",
  "LEFT":"s,n",
  "RIGHT":"s,n",
  "TRIM":"s|ns",
  "LTRIM":"s|s",
  "RTRIM":"s|s",
  "FIND_FIRST":"s,s|zn,zn",
  "FIND_LAST":"s,s|zn,zn",
  "SPLIT":"s|sl,n",
  "SUBSTITUTE":"s,las|lsn,n",
  "MD5":"s",
  "SHA1":"s",
  "RANDOM_TOKEN":"n",
  "FLOOR":"n",
  "CEIL":"n",
  "ROUND":"n",
  "ABS":"n",
  "RAND":"",
  "SQRT":"n",
  "POW":"n,n",
  "RANGE":"n,n|n",
  "UNION":"l,l|+",
  "UNION_DISTINCT":"l,l|+",
  "MINUS":"l,l|+",
  "INTERSECTION":"l,l|+",
  "FLATTEN":"l|n",
  "LENGTH":"las",
  "COUNT":"las",
  "MIN":"l",
  "MAX":"l",
  "SUM":"l",
  "MEDIAN":"l",
  "PERCENTILE":"l,n|s",
  "AVERAGE":"l",
  "AVG":"l",
  "VARIANCE_SAMPLE":"l",
  "VARIANCE_POPULATION":"l",
  "VARIANCE":"l",
  "STDDEV_SAMPLE":"l",
  "STDDEV_POPULATION":"l",
  "STDDEV":"l",
  "UNIQUE":"l",
  "SORTED_UNIQUE":"l",
  "SLICE":"l,n|n",
  "REVERSE":"ls",
  "FIRST":"l",
  "LAST":"l",
  "NTH":"l,n",
  "POSITION":"l,.|b",
  "CALL":"s|.+",
  "APPLY":"s|l",
  "PUSH":"l,.|b",
  "APPEND":"l,lz|b",
  "POP":"l",
  "SHIFT":"l",
  "UNSHIFT":"l,.|b",
  "REMOVE_VALUE":"l,.|n",
  "REMOVE_VALUES":"l,lz",
  "REMOVE_NTH":"l,n",
  "HAS":"az,s",
  "ATTRIBUTES":"a|b,b",
  "VALUES":"a|b",
  "MERGE":"la|+",
  "MERGE_RECURSIVE":"a,a|+",
  "DOCUMENT":"h.|.",
  "MATCHES":".,l|b",
  "UNSET":"a,sl|+",
  "UNSET_RECURSIVE":"a,sl|+",
  "KEEP":"a,sl|+",
  "TRANSLATE":".,a|.",
  "ZIP":"l,l",
  "NEAR":"hs,n,n|nz,s",
  "WITHIN":"hs,n,n,n|s",
  "WITHIN_RECTANGLE":"hs,d,d,d,d",
  "IS_IN_POLYGON":"l,ln|nb",
  "FULLTEXT":"hs,s,s|n",
  "DATE_NOW":"",
  "DATE_TIMESTAMP":"ns|ns,ns,ns,ns,ns,ns",
  "DATE_ISO8601":"ns|ns,ns,ns,ns,ns,ns",
  "DATE_DAYOFWEEK":"ns",
  "DATE_YEAR":"ns",
  "DATE_MONTH":"ns",
  "DATE_DAY":"ns",
  "DATE_HOUR":"ns",
  "DATE_MINUTE":"ns",
  "DATE_SECOND":"ns",
  "DATE_DAYOFYEAR":"ns",
  "DATE_ISOWEEK":"ns",
  "DATE_LEAPYEAR":"ns",
  "DATE_QUARTER":"ns",
  "DATE_DAYS_IN_MONTH":"ns",
  "DATE_ADD":"ns,ns|n",
  "DATE_SUBTRACT":"ns,ns|n",
  "DATE_DIFF":"ns,ns,s|b",
  "DATE_COMPARE":"ns,ns,s|s",
  "DATE_FORMAT":"ns,s",
  "FAIL":"|s",
  "PASSTHRU":".",
  "NOOPT":".",
  "V8":".",
  "SLEEP":"n",
  "COLLECTIONS":"",
  "NOT_NULL":".|+",
  "FIRST_LIST":".|+",
  "FIRST_DOCUMENT":".|+",
  "PARSE_IDENTIFIER":".",
  "CURRENT_USER":"",
  "CURRENT_DATABASE":"",
  "COLLECTION_COUNT":"chs",
};

let args = {
  "null": null,
  "usmallint" : 1,
  "smallint": -1,
  "double": Math.PI,
  "emptyobj": {},
  "flatobj": {"hans": null, "kanns": 8, "nicht": "..,-"},
  "nan": NaN,
  "inf": Infinity,
  "emptyarr": [],
  "arr": ["hund", 1, 0.9, null, "\0jaja"],
  "hihi": "😂",
  "regex": /haha/,
  "complexobject": {
    "hans": 17,
    "wurst": {
      "lala": 19001,
      "hund": null,
      "0byte": "\0hahah",
      "BATMAN": NaN,
      "inf": Infinity,
      "mett": [1,2, "POLIZEI", null, null, {}]
    },
    "SYM": Symbol("unicodekatze"),
    "HIHI": "😂",
  },
};


var db = require("@arangodb").db;

function stressTestSuite() {
  return {
    testMatrix: function() {
      var numSuccessful = 0;
      var keys = Object.keys(args);

      var generateAqlArgument = function(keyIndex, index) {
        return "@a" + index;
      };

      var argumentReducer = function(current, keyIndex, index) {
        current['a' + index] = args[keys[keyIndex]];
        return current;
      };

      Object.keys(aqlFuncs).forEach(function(aqlFunc) {
        var funcArguments = aqlFuncs[aqlFunc];
        var funcParts = funcArguments.split('|');

        var funcMinArguments = funcParts[0].length > 0 ? funcParts[0].split(',').length : 0;
        var funcMaxArguments = funcParts.length > 1
          ? funcMinArguments + funcParts[1].split(',').length
          : funcMinArguments;
        
        // test one off for every function
        var testMinArguments = funcMinArguments - 1;
        if (testMinArguments < 0) {
          testMinArguments = 0;
        }
        var testMaxArguments = funcMaxArguments + 1;

        // try up to 5 arguments for every query
        for (var i = testMinArguments; i <= testMaxArguments; i++) {
          // and brute force every combination :D
          // generate a matrix with indices pointing to our possible values
          // start with 0 in every cell
          var keyIndices = [];
          for (var j = 1; j <= i; j++) {
            keyIndices.push(0);
          }
           
          // mop: generate query for this argument count run
          var query = "RETURN " + aqlFunc + "(" + keyIndices.map(generateAqlArgument).join(",") + ")";
          
          // mop: start at the end, increment during loop and overflow to the front 
          var lastIndex =  keyIndices.length - 1;
          while (true) {
            var queryArgs = keyIndices.reduce(argumentReducer, {});
            //console.log(query, queryArgs);
            try {
              db._query({"query": query, "bindVars": queryArgs});
              numSuccessful++;
            } catch (e) {
            }
            
            var index = lastIndex;
            while (++keyIndices[index] >= keys.length) {
              keyIndices[index] = 0;
              index--;
              if (index < 0) {
                break;
              }
            }
            if (index < 0) {
              break;
            }
          }
        }
      });
      assertTrue(numSuccessful, "Not a single test has been successful. Something is broken :S");
    }
  };
}

jsunity.run(stressTestSuite);

return jsunity.done();

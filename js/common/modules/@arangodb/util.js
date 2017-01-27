'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB utilities
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Alan Plum
// / @author Copyright 2016, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const Chalk = require('chalk').constructor;
const chalk = new Chalk({enabled: true});
const dedent = require('dedent');
const codeFrame = require('babel-code-frame');
const stackParser = require('error-stack-parser');

exports.pluck = function (objs, key) {
  return _.map(objs, function (obj) {
    return _.get(obj, key);
  });
};

exports.union = function union () {
  const things = Array.prototype.slice.call(arguments);
  if (!things.slice(1).some(Boolean)) {
    return things[0];
  }
  let result;
  if (things[0] instanceof Map) {
    result = new Map();
    for (let map of things) {
      for (let entry of map.entries()) {
        result.set(entry[0], entry[1]);
      }
    }
  } else if (things[0] instanceof Set) {
    result = new Set();
    for (let set of things) {
      for (let value of set.values()) {
        result.add(value);
      }
    }
  } else if (Array.isArray(things[0])) {
    result = Array.prototype.concat.apply([], things);
  } else {
    things.unshift({});
    result = Object.assign(...things);
  }
  return result;
};

exports.drain = function (generator) {
  let results = [];
  for (let result of generator) {
    results.push(result);
  }
  return results;
};

exports.codeFrame = function (e, basePath, withColor = false) {
  try {
    let ctx;
    let err = e;
    while (err) {
      try {
        if (err instanceof SyntaxError) {
          const location = err.stack.split('\n')[0];
          const [fileName, lineNo, colNo] = stackParser.extractLocation(location);
          if (lineNo && colNo && (!basePath || fileName.indexOf(basePath) === 0)) {
            ctx = {
              fileName,
              lineNumber: Number(lineNo),
              columnNumber: Number(colNo)
            };
            break;
          }
        } else {
          const stack = stackParser.parse(err);
          for (const step of stack) {
            if (!basePath || step.fileName.indexOf(basePath) === 0) {
              ctx = step;
              break;
            }
          }
        }
      } catch (e) {}
      err = err.cause;
    }
    if (ctx) {
      const source = fs.readFileSync(ctx.fileName, 'utf-8');
      const frame = codeFrame(source, ctx.lineNumber, ctx.columnNumber, {
        highlightCode: withColor,
        forceColor: withColor
      });
      const location = `@ ${ctx.fileName}:${ctx.lineNumber}:${ctx.columnNumber}\n`;
      return (withColor ? chalk.grey(location) : location) + frame;
    }
  } catch (e) {}
  return null;
};

exports.inline = function (strs) {
  let str;
  if (typeof strs === 'string') {
    str = strs;
  } else {
    const strb = [strs[0]];
    const vars = Array.prototype.slice.call(arguments, 1);
    for (let i = 0; i < vars.length; i++) {
      strb.push(vars[i], strs[i + 1]);
    }
    str = strb.join('');
  }
  return str.replace(/\s*\n\s*/g, ' ').replace(/(^\s|\s$)/g, '');
};

exports.redent = function ([start, ...strs], ...vars) {
  const body = start.split('\n');
  for (let i = 0; i < vars.length; i++) {
    const current = body[body.length - 1];
    const match = current.match(/^(\s+)/);
    const padding = match ? match[1] : '';
    const lines = String(vars[i]).split('\n');
    body.push(body.pop() + lines[0]);
    if (lines.length > 1) {
      body.push(...lines.slice(1).map(line => padding + line));
    }
    const append = strs[i].split('\n');
    body.push(body.pop() + append[0]);
    if (append.length > 1) {
      body.push(...append.slice(1));
    }
  }
  return body.join('\n');
};

exports.dedent = function (...args) {
  return dedent(exports.redent(...args));
};

exports.propertyKeys = function (obj) {
  return Object.keys(obj).filter((key) => (
    key.charAt(0) !== '_' && key.charAt(0) !== '$'
  ));
};

exports.shallowCopy = function (src) {
  const dest = {};
  if (src === undefined || src === null) {
    return dest;
  }
  for (const key of exports.propertyKeys(src)) {
    dest[key] = src[key];
  }
  return dest;
};

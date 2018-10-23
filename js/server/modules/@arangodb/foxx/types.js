'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const mimeTypes = require('mime-types');
const querystring = require('querystring');
// const assert = require('assert')
// const contentDisposition = require('content-disposition')

const DEFAULT_CHARSET = 'utf-8';

module.exports = new Map([
  [mimeTypes.lookup('text'), {
    fromClient(body, req, type) {
      body = stringifyBuffer(body, type.parameters.charset);
      return body;
    },
    forClient(body) {
      return {
        data: String(body),
        headers: {
          'content-type': mimeTypes.contentType('text')
        }
      };
    }
  }],
  [mimeTypes.lookup('json'), {
    fromClient(body, req, type) {
      if (!body || !body.length) return undefined;
      body = stringifyBuffer(body, type.parameters.charset);
      return JSON.parse(body);
    },
    forClient(body) {
      return {
        data: JSON.stringify(body),
        headers: {
          'content-type': mimeTypes.contentType('json')
        }
      };
    }
  }],
  ['application/x-www-form-urlencoded', {
    fromClient(body, req, type) {
      body = stringifyBuffer(body, type.parameters.charset);
      return querystring.parse(body);
    },
    forClient(body) {
      return {
        data: querystring.stringify(body),
        headers: {
          'content-type': 'application/x-www-form-urlencoded'
        }
      };
    }
  }] /* ,
  ['multipart/form-data', {
    fromClient(body) {
      assert(
        Array.isArray(body) && body.every(function (part) {
          return (
            part && typeof part === 'object'
            && part.headers && typeof part.headers === 'object'
            && part.data instanceof Buffer
          )
        }),
        `Expecting a multipart array, not ${body ? typeof body : String(body)}`
      )
      const form = new Map()
      for (const part of body) {
        const dispositionHeader = part.headers && part.headers['content-disposition']
        if (!dispositionHeader) {
          continue
        }
        const disposition = contentDisposition.parse(dispositionHeader)
        const name = disposition.parameters.name
        if (disposition.type !== 'form-data' || !name) {
          continue
        }
        let value = part.data
        // TODO parse value according to headers?
        if (!form.has(name)) {
          form.set(name, value)
        } else if (!Array.isArray(form.get(name))) {
          form.set(name, [form.get(name), value])
        } else {
          form.get(name).push(value)
        }
      }
      return form
    }
  }]*/
]);

function stringifyBuffer (buf, charset) {
  if (!(buf instanceof Buffer)) {
    return buf;
  }
  charset = charset || DEFAULT_CHARSET;
  try {
    return buf.toString(charset);
  } catch (e) {
    if (charset === DEFAULT_CHARSET) {
      throw e;
    }
    console.warn(
      `Unable to parse buffer with charset "${charset}",`
      + ` falling back to default "${DEFAULT_CHARSET}"`
    );
    return buf.toString(DEFAULT_CHARSET);
  }
}

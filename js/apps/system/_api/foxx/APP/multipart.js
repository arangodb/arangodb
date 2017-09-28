'use strict';
const _ = require('lodash');
const assert = require('assert');
const contentDisposition = require('content-disposition');

module.exports = {
  fromClient (body) {
    assert(
      Array.isArray(body) && body.every((part) => (
        part && typeof part === 'object'
        && part.headers && typeof part.headers === 'object'
        && part.data instanceof Buffer
      )),
      `Expecting a multipart array, not ${body ? typeof body : String(body)}`
    );
    const parsedBody = {};
    for (const part of body) {
      const headers = {};
      for (const key of Object.keys(part.headers)) {
        headers[key.toLowerCase()] = part.headers[key];
      }
      const dispositionHeader = headers['content-disposition'];
      if (!dispositionHeader) {
        continue;
      }
      const disposition = contentDisposition.parse(dispositionHeader);
      if (disposition.type !== 'form-data' || !disposition.parameters.name) {
        continue;
      }
      const name = disposition.parameters.name;
      const filename = disposition.parameters.filename;
      const type = headers['content-type'];
      if (type || filename) {
        parsedBody[name] = Object.assign(part.data, {
          headers: _.omit(headers, ['content-disposition']),
          filename
        });
      } else {
        let value = part.data.toString('utf-8');
        try {
          value = JSON.parse(value);
        } catch (e) {
          // noop
        }
        parsedBody[name] = value;
      }
    }
    return parsedBody;
  }
};

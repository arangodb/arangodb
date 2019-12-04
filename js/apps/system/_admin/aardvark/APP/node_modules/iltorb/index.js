'use strict';

exports.compress = compress;
exports.decompress = decompress;
exports.compressSync = compressSync;
exports.decompressSync = decompressSync;
exports.compressStream = compressStream;
exports.decompressStream = decompressStream;

const iltorb = require('./build/bindings/iltorb.node');
const Transform = require('stream').Transform;

class TransformStreamEncode extends Transform {
  constructor(params, sync) {
    super(params);
    this.sync = sync || false;
    this.encoding = false;
    this.corked = false;
    this.flushing = false;
    this.encoder = new iltorb.StreamEncode(params || {});
  }

  _transform(chunk, encoding, next) {
    this.encoding = true;
    this.encoder.transform(chunk, (err, output) => {
      this.encoding = false;
      if (err) {
        return next(err);
      }
      this._push(output);
      next();
      if (this.flushing) {
        this.flush(true);
      }
    }, !this.sync);
  }

  _flush(done) {
    this.encoder.flush(true, (err, output) => {
      if (err) {
        return done(err);
      }
      this._push(output);
      done();
    }, !this.sync);
  }

  _push(output) {
    if (output) {
      for (let i = 0; i < output.length; i++) {
        this.push(output[i]);
      }
    }
  }

  flush(force) {
    if (this.flushing && !force) {
      return;
    }

    if (!this.corked) {
      this.cork();
    }
    this.corked = true;
    this.flushing = true;

    if (this.encoding) {
      return;
    }

    this.encoder.flush(false, (err, output) => {
      if (err) {
        this.emit('error', err);
      } else {
        this._push(output);
      }
      this.corked = false;
      this.flushing = false;
      this.uncork();
    }, true);
  }
}

class TransformStreamDecode extends Transform {
  constructor(params, sync) {
    super(params);
    this.sync = sync || false;
    this.decoder = new iltorb.StreamDecode(params || {});
  }

  _transform(chunk, encoding, next) {
    this.decoder.transform(chunk, (err, output) => {
      if (err) {
        return next(err);
      }
      this._push(output);
      next();
    }, !this.sync);
  }

  _flush(done) {
    this.decoder.flush((err, output) => {
      if (err) {
        return done(err);
      }
      this._push(output);
      done();
    }, !this.sync);
  }

  _push(output) {
    if (output) {
      for (let i = 0; i < output.length; i++) {
        this.push(output[i]);
      }
    }
  }
}

function compress(input, params, cb) {
  if (arguments.length === 2) {
    cb = params;
    params = {};
  }
  if (!Buffer.isBuffer(input)) {
    process.nextTick(cb, new Error('Brotli input is not a buffer.'));
    return;
  }
  if (typeof cb !== 'function') {
    process.nextTick(cb, new Error('Second argument is not a function.'));
    return;
  }
  if (typeof params !== 'object') {
    params = {};
  }
  params.size_hint = input.length;
  const stream = new TransformStreamEncode(params);
  const chunks = [];
  let length = 0;
  stream.on('error', cb);
  stream.on('data', function(c) {
    chunks.push(c);
    length += c.length;
  });
  stream.on('end', function() {
    cb(null, Buffer.concat(chunks, length));
  });
  stream.end(input);
}

function decompress(input, params, cb) {
  if (arguments.length === 2) {
    cb = params;
    params = {};
  }
  if (!Buffer.isBuffer(input)) {
    process.nextTick(cb, new Error('Brotli input is not a buffer.'));
    return;
  }
  if (typeof cb !== 'function') {
    process.nextTick(cb, new Error('Second argument is not a function.'));
    return;
  }
  const stream = new TransformStreamDecode(params);
  const chunks = [];
  let length = 0;
  stream.on('error', cb);
  stream.on('data', function(c) {
    chunks.push(c);
    length += c.length;
  });
  stream.on('end', function() {
    cb(null, Buffer.concat(chunks, length));
  });
  stream.end(input);
}

function compressSync(input, params) {
  if (!Buffer.isBuffer(input)) {
    throw new Error('Brotli input is not a buffer.');
  }
  if (typeof params !== 'object') {
    params = {};
  }
  params.size_hint = input.length;
  const stream = new TransformStreamEncode(params, true);
  const chunks = [];
  let length = 0;
  stream.on('error', function(e) {
    throw e;
  });
  stream.on('data', function(c) {
    chunks.push(c);
    length += c.length;
  });
  stream.end(input);
  return Buffer.concat(chunks, length);
}

function decompressSync(input, params) {
  if (!Buffer.isBuffer(input)) {
    throw new Error('Brotli input is not a buffer.');
  }
  const stream = new TransformStreamDecode(params, true);
  const chunks = [];
  let length = 0;
  stream.on('error', function(e) {
    throw e;
  });
  stream.on('data', function(c) {
    chunks.push(c);
    length += c.length;
  });
  stream.end(input);
  return Buffer.concat(chunks, length);
}

function compressStream(params) {
  return new TransformStreamEncode(params);
}

function decompressStream(params) {
  return new TransformStreamDecode(params);
}

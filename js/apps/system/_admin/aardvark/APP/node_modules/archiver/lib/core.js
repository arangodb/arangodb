/**
 * Archiver Core
 *
 * @ignore
 * @license [MIT]{@link https://github.com/archiverjs/node-archiver/blob/master/LICENSE}
 * @copyright (c) 2012-2014 Chris Talkington, contributors.
 */
var fs = require('fs');
var glob = require('glob');
var async = require('async');
var _ = require('lodash');
var path = require('path');
var walkdir = require('walkdir');
var util = require('archiver-utils');

var inherits = require('util').inherits;
var Transform = require('readable-stream').Transform;

var win32 = process.platform === 'win32';

if (process._loggedBulkDeprecation === undefined) {
  process._loggedBulkDeprecation = false;
}

/**
 * @constructor
 * @param {String} format The archive format to use.
 * @param {(CoreOptions|TransformOptions)} options See also {@link ZipOptions} and {@link TarOptions}.
 */
var Archiver = function(format, options) {
  if (!(this instanceof Archiver)) {
    return new Archiver(format, options);
  }

  if (typeof format !== 'string') {
    options = format;
    format = 'zip';
  }

  options = this.options = util.defaults(options, {
    highWaterMark: 1024 * 1024,
    statConcurrency: 4
  });

  Transform.call(this, options);

  this._entries = [];
  this._format = false;
  this._module = false;
  this._pending = 0;
  this._pointer = 0;

  this._queue = async.queue(this._onQueueTask.bind(this), 1);
  this._queue.drain = this._onQueueDrain.bind(this);

  this._statQueue = async.queue(this._onStatQueueTask.bind(this), options.statConcurrency);

  this._state = {
    aborted: false,
    finalize: false,
    finalizing: false,
    finalized: false,
    modulePiped: false
  };

  this._streams = [];
};

inherits(Archiver, Transform);

/**
 * Internal logic for `abort`.
 *
 * @private
 * @return void
 */
Archiver.prototype._abort = function() {
  this._state.aborted = true;
  this._queue.kill();
  this._statQueue.kill();

  if (this._queue.idle()) {
    this._shutdown();
  }
};

/**
 * Internal helper for appending files.
 *
 * @private
 * @param  {String} filepath The source filepath.
 * @param  {EntryData} data The entry data.
 * @return void
 */
Archiver.prototype._append = function(filepath, data) {
  data = data || {};

  var task = {
    source: null,
    filepath: filepath
  };

  if (!data.name) {
    data.name = filepath;
  }

  data.sourcePath = filepath;
  task.data = data;

  if (data.stats && data.stats instanceof fs.Stats) {
    task = this._updateQueueTaskWithStats(task, data.stats);
    this._queue.push(task);
  } else {
    this._statQueue.push(task);
  }
};

/**
 * Internal logic for `finalize`.
 *
 * @private
 * @return void
 */
Archiver.prototype._finalize = function() {
  if (this._state.finalizing || this._state.finalized || this._state.aborted) {
    return;
  }

  this._state.finalizing = true;

  this._moduleFinalize();

  this._state.finalizing = false;
  this._state.finalized = true;
};

/**
 * Checks the various state variables to determine if we can `finalize`.
 *
 * @private
 * @return {Boolean}
 */
Archiver.prototype._maybeFinalize = function() {
  if (this._state.finalizing || this._state.finalized || this._state.aborted) {
    return false;
  }

  if (this._state.finalize && this._pending === 0 && this._queue.idle() && this._statQueue.idle()) {
    this._finalize();
    return true;
  }

  return false;
};

/**
 * Appends an entry to the module.
 *
 * @private
 * @fires  Archiver#entry
 * @param  {(Buffer|Stream)} source
 * @param  {EntryData} data
 * @param  {Function} callback
 * @return void
 */
Archiver.prototype._moduleAppend = function(source, data, callback) {
  if (this._state.aborted) {
    callback();
    return;
  }

  this._module.append(source, data, function(err) {
    this._task = null;

    if (this._state.aborted) {
      this._shutdown();
      return;
    }

    if (err) {
      this.emit('error', err);
      setImmediate(callback);
      return;
    }

    /**
     * Fires when the entry's input has been processed and appended to the archive.
     *
     * @event Archiver#entry
     * @type {EntryData}
     */
    this.emit('entry', data);
    this._entries.push(data);

    setImmediate(callback);
  }.bind(this));
};

/**
 * Finalizes the module.
 *
 * @private
 * @return void
 */
Archiver.prototype._moduleFinalize = function() {
  if (typeof this._module.finalize === 'function') {
    this._module.finalize();
  } else if (typeof this._module.end === 'function') {
    this._module.end();
  } else {
    this.emit('error', new Error('module: no suitable finalize/end method found'));
    return;
  }
};

/**
 * Pipes the module to our internal stream with error bubbling.
 *
 * @private
 * @return void
 */
Archiver.prototype._modulePipe = function() {
  this._module.on('error', this._onModuleError.bind(this));
  this._module.pipe(this);
  this._state.modulePiped = true;
};

/**
 * Determines if the current module supports a defined feature.
 *
 * @private
 * @param  {String} key
 * @return {Boolean}
 */
Archiver.prototype._moduleSupports = function(key) {
  if (!this._module.supports || !this._module.supports[key]) {
    return false;
  }

  return this._module.supports[key];
};

/**
 * Unpipes the module from our internal stream.
 *
 * @private
 * @return void
 */
Archiver.prototype._moduleUnpipe = function() {
  this._module.unpipe(this);
  this._state.modulePiped = false;
};

/**
 * Normalizes entry data with fallbacks for key properties.
 *
 * @private
 * @param  {Object} data
 * @param  {fs.Stats} stats
 * @return {Object}
 */
Archiver.prototype._normalizeEntryData = function(data, stats) {
  data = util.defaults(data, {
    type: 'file',
    name: null,
    date: null,
    mode: null,
    prefix: null,
    sourcePath: null,
    stats: false
  });

  if (stats && data.stats === false) {
    data.stats = stats;
  }

  var isDir = data.type === 'directory';

  if (data.name) {
    if (typeof data.prefix === 'string' && '' !== data.prefix) {
      data.name = data.prefix + '/' + data.name;
      data.prefix = null;
    }

    data.name = util.sanitizePath(data.name);

    if (data.name.slice(-1) === '/') {
      isDir = true;
      data.type = 'directory';
    } else if (isDir) {
      data.name += '/';
    }
  }

  // 511 === 0777; 493 === 0755; 438 === 0666; 420 === 0644
  if (typeof data.mode === 'number') {
    if (win32) {
      data.mode &= 511;
    } else {
      data.mode &= 4095
    }
  } else if (data.stats && data.mode === null) {
    if (win32) {
      data.mode = data.stats.mode & 511;
    } else {
      data.mode = data.stats.mode & 4095;
    }

    // stat isn't reliable on windows; force 0755 for dir
    if (win32 && isDir) {
      data.mode = 493;
    }
  } else if (data.mode === null) {
    data.mode = isDir ? 493 : 420;
  }

  if (data.stats && data.date === null) {
    data.date = data.stats.mtime;
  } else {
    data.date = util.dateify(data.date);
  }

  return data;
};

/**
 * Error listener that re-emits error on to our internal stream.
 *
 * @private
 * @param  {Error} err
 * @return void
 */
Archiver.prototype._onModuleError = function(err) {
  this.emit('error', err);
};

/**
 * Checks the various state variables after queue has drained to determine if
 * we need to `finalize`.
 *
 * @private
 * @return void
 */
Archiver.prototype._onQueueDrain = function() {
  if (this._state.finalizing || this._state.finalized || this._state.aborted) {
    return;
  }

  if (this._state.finalize && this._pending === 0 && this._queue.idle() && this._statQueue.idle()) {
    this._finalize();
    return;
  }
};

/**
 * Appends each queue task to the module.
 *
 * @private
 * @param  {Object} task
 * @param  {Function} callback
 * @return void
 */
Archiver.prototype._onQueueTask = function(task, callback) {
  if (this._state.finalizing || this._state.finalized || this._state.aborted) {
    callback();
    return;
  }

  this._task = task;
  this._moduleAppend(task.source, task.data, callback);
};

/**
 * Performs a file stat and reinjects the task back into the queue.
 *
 * @private
 * @param  {Object} task
 * @param  {Function} callback
 * @return void
 */
Archiver.prototype._onStatQueueTask = function(task, callback) {
  if (this._state.finalizing || this._state.finalized || this._state.aborted) {
    callback();
    return;
  }

  fs.stat(task.filepath, function(err, stats) {
    if (this._state.aborted) {
      setImmediate(callback);
      return;
    }

    if (err) {
      this.emit('error', err);
      setImmediate(callback);
      return;
    }

    task = this._updateQueueTaskWithStats(task, stats);

    if (task.source !== null) {
      this._queue.push(task);
      setImmediate(callback);
    } else {
      this.emit('error', new Error('unsupported entry: ' + task.filepath));
      setImmediate(callback);
      return;
    }
  }.bind(this));
};

/**
 * Unpipes the module and ends our internal stream.
 *
 * @private
 * @return void
 */
Archiver.prototype._shutdown = function() {
  this._moduleUnpipe();
  this.end();
};

/**
 * Tracks the bytes emitted by our internal stream.
 *
 * @private
 * @param  {Buffer} chunk
 * @param  {String} encoding
 * @param  {Function} callback
 * @return void
 */
Archiver.prototype._transform = function(chunk, encoding, callback) {
  if (chunk) {
    this._pointer += chunk.length;
  }

  callback(null, chunk);
};

/**
 * Updates and normalizes a queue task using stats data.
 *
 * @private
 * @param  {Object} task
 * @param  {fs.Stats} stats
 * @return {Object}
 */
Archiver.prototype._updateQueueTaskWithStats = function(task, stats) {
  if (stats.isFile()) {
    task.data.type = 'file';
    task.data.sourceType = 'stream';
    task.source = util.lazyReadStream(task.filepath);
  } else if (stats.isDirectory() && this._moduleSupports('directory')) {
    task.data.name = util.trailingSlashIt(task.data.name);
    task.data.type = 'directory';
    task.data.sourcePath = util.trailingSlashIt(task.filepath);
    task.data.sourceType = 'buffer';
    task.source = new Buffer(0);
  } else {
    return task;
  }

  task.data = this._normalizeEntryData(task.data, stats);
  return task;
};

/**
 * Aborts the archiving process, taking a best-effort approach, by:
 *
 * - removing any pending queue tasks
 * - allowing any active queue workers to finish
 * - detaching internal module pipes
 * - ending both sides of the Transform stream
 *
 * It will NOT drain any remaining sources.
 *
 * @return {this}
 */
Archiver.prototype.abort = function() {
  if (this._state.aborted || this._state.finalized) {
    return this;
  }

  this._abort();

  return this;
};

/**
 * Appends an input source (text string, buffer, or stream) to the instance.
 *
 * When the instance has received, processed, and emitted the input, the `entry`
 * event is fired.
 *
 * @fires  Archiver#entry
 * @param  {(Buffer|Stream|String)} source The input source.
 * @param  {EntryData} data See also {@link ZipEntryData} and {@link TarEntryData}.
 * @return {this}
 */
Archiver.prototype.append = function(source, data) {
  if (this._state.finalize || this._state.aborted) {
    this.emit('error', new Error('append: queue closed'));
    return this;
  }

  data = this._normalizeEntryData(data);

  if (typeof data.name !== 'string' || data.name.length === 0) {
    this.emit('error', new Error('append: entry name must be a non-empty string value'));
    return this;
  }

  if (data.type === 'directory' && !this._moduleSupports('directory')) {
    this.emit('error', new Error('append: entries of "directory" type not currently supported by this module'));
    return this;
  }

  source = util.normalizeInputSource(source);

  if (Buffer.isBuffer(source)) {
    data.sourceType = 'buffer';
  } else if (util.isStream(source)) {
    data.sourceType = 'stream';
  } else {
    this.emit('error', new Error('append: input source must be valid Stream or Buffer instance'));
    return this;
  }

  this._queue.push({
    data: data,
    source: source
  });

  return this;
};

/**
 * Appends multiple entries from passed array of src-dest mappings.
 *
 * A [lazystream]{@link https://github.com/jpommerening/node-lazystream} wrapper is
 * used to prevent issues with open file limits.
 *
 * @deprecated 0.21.0
 * @param  {Object[]} mappings
 * @param  {(EntryData|Function)} mappings[].data See also {@link ZipEntryData}
 * and {@link TarEntryData}.
 * @param  {(String|Array)} mappings[].src Pattern(s) to match, relative to the `cwd`.
 * @param  {String} mappings[].dest Destination path prefix.
 * @param  {Boolean} mappings[].expand Process a dynamic src-dest file mapping.
 * @param  {String} mappings[].cwd All `src` matches are relative to (but don't include)
 * this path. requires `expand`.
 * @param  {String} mappings[].ext Replace any existing extension with this value in
 * generated `dest` paths. requires `expand`.
 * @param  {String} mappings[].extDot Used to indicate where the period indicating
 * the extension is located. requires `expand`.
 * @param  {String} mappings[].flatten Remove all path parts from generated `dest`
 * paths. requires `expand`.
 * @param  {*} mappings[].* See [node-glob]{@link https://github.com/isaacs/node-glob#properties}
 * and [minimatch]{@link https://github.com/isaacs/minimatch#properties} documentation
 * for additional properties.
 * @return {this}
 */
Archiver.prototype.bulk = function(mappings) {
  if (process._loggedBulkDeprecation === false) {
    process._loggedBulkDeprecation = true;
    var warning = 'Archiver.bulk() deprecated since 0.21.0';
    if (typeof process !== 'undefined' && typeof process.emitWarning !== 'undefined') {
      process.emitWarning(warning, 'DeprecationWarning');
    } else {
      console.warn(warning);
    }
  }

  if (this._state.finalize || this._state.aborted) {
    this.emit('error', new Error('bulk: queue closed'));
    return this;
  }

  if (!Array.isArray(mappings)) {
    mappings = [mappings];
  }

  var self = this;
  var files = util.file.normalizeFilesArray(mappings);

  files.forEach(function(file){
    var isExpandedPair = file.orig.expand || false;
    var data = {};
    var dataFunction = false;

    if (typeof file.data === 'function') {
      dataFunction = file.data;
    } else if (typeof file.data === 'object') {
      data = file.data;
    }

    file.src.forEach(function(filepath) {
      var entryData = _.extend({}, data);
      var entryName = isExpandedPair ? file.dest : (file.dest || '') + '/' + filepath;
      entryData.name = util.sanitizePath(entryName);

      if (entryData.name === '.') {
        return;
      }

      try {
        if (dataFunction) {
          entryData = dataFunction(entryData);

          if (typeof entryData !== 'object') {
            throw new Error('bulk: invalid data returned from custom function');
          }
        }
      } catch(e) {
        self.emit('error', e);
        return;
      }

      self._append(filepath, entryData);
    });
  });

  return this;
};

/**
 * Appends a directory and its files, recursively, given its dirpath.
 *
 * @param  {String} dirpath The source directory path.
 * @param  {String} destpath The destination path within the archive.
 * @param  {(EntryData|Function)} data See also [ZipEntryData]{@link ZipEntryData} and
 * [TarEntryData]{@link TarEntryData}.
 * @return {this}
 */
Archiver.prototype.directory = function(dirpath, destpath, data) {
  if (this._state.finalize || this._state.aborted) {
    this.emit('error', new Error('directory: queue closed'));
    return this;
  }

  if (typeof dirpath !== 'string' || dirpath.length === 0) {
    this.emit('error', new Error('directory: dirpath must be a non-empty string value'));
    return this;
  }

  this._pending++;

  if (destpath === false) {
    destpath = '';
  } else if (typeof destpath !== 'string'){
    destpath = dirpath;
  }

  var dataFunction = false;
  if (typeof data === 'function') {
    dataFunction = data;
    data = {};
  } else if (typeof data !== 'object') {
    data = {};
  }

  function onWalkPath(filepath, stats){
    var entryData = _.extend({}, data);
    entryData.name = path.relative(dirpath, filepath).replace(/\\/g, '/');
    entryData.prefix = destpath;
    entryData.stats = stats;

    try {
      if (dataFunction) {
        entryData = dataFunction(entryData);

        if (typeof entryData !== 'object') {
          throw new Error('directory: invalid data returned from custom function');
        }
      }
    } catch(e) {
      this.emit('error', e);
      return;
    }

    this._append(filepath, entryData);
  }

  function onWalkEnd() {
    this._pending--;
    this._maybeFinalize();
  }

  function onWalkError(err) {
    this.emit('error', 'directory: ' + err);
  }

  var walker = walkdir(dirpath);

  walker.on('error', onWalkError.bind(this));
  walker.on('directory', onWalkPath.bind(this));
  walker.on('file', onWalkPath.bind(this));
  walker.on('end', onWalkEnd.bind(this));

  return this;
};

/**
 * Appends a file given its filepath using a
 * [lazystream]{@link https://github.com/jpommerening/node-lazystream} wrapper to
 * prevent issues with open file limits.
 *
 * When the instance has received, processed, and emitted the file, the `entry`
 * event is fired.
 *
 * @param  {String} filepath The source filepath.
 * @param  {EntryData} data See also [ZipEntryData]{@link ZipEntryData} and
 * [TarEntryData]{@link TarEntryData}.
 * @return {this}
 */
Archiver.prototype.file = function(filepath, data) {
  if (this._state.finalize || this._state.aborted) {
    this.emit('error', new Error('file: queue closed'));
    return this;
  }

  if (typeof filepath !== 'string' || filepath.length === 0) {
    this.emit('error', new Error('file: filepath must be a non-empty string value'));
    return this;
  }

  this._append(filepath, data);

  return this;
};

/**
 * Appends multiple files that match a glob pattern.
 *
 * @param  {String} pattern The [glob pattern]{@link https://github.com/isaacs/node-glob#glob-primer} to match.
 * @param  {Object} options See [node-glob]{@link https://github.com/isaacs/node-glob#options}.
 * @param  {EntryData} data See also [ZipEntryData]{@link ZipEntryData} and
 * [TarEntryData]{@link TarEntryData}.
 * @return {this}
 */
Archiver.prototype.glob = function(pattern, options, data) {
  this._pending++;

  options = util.defaults(options, {
    stat: false
  });

  function onGlobEnd() {
    this._pending--;
    this._maybeFinalize();
  }

  function onGlobError(err) {
    this.emit('error', 'glob: ' + err);
  }

  function onGlobMatch(match){
    entryData = _.extend({}, data);

    if (options.cwd) {
      entryData.name = match;
      match = globber._makeAbs(match);
    }

    this._append(match, entryData);
  }

  var globber = glob(pattern, options);
  globber.on('error', onGlobError.bind(this));
  globber.on('match', onGlobMatch.bind(this));
  globber.on('end', onGlobEnd.bind(this));

  return this;
};

/**
 * Finalizes the instance and prevents further appending to the archive
 * structure (queue will continue til drained).
 *
 * The `end`, `close` or `finish` events on the destination stream may fire
 * right after calling this method so you should set listeners beforehand to
 * properly detect stream completion.
 *
 * @return {this}
 */
Archiver.prototype.finalize = function() {
  if (this._state.aborted) {
    this.emit('error', new Error('finalize: archive was aborted'));
    return this;
  }

  if (this._state.finalize) {
    this.emit('error', new Error('finalize: archive already finalizing'));
    return this;
  }

  this._state.finalize = true;

  if (this._pending === 0 && this._queue.idle() && this._statQueue.idle()) {
    this._finalize();
  }

  return this;
};

/**
 * Sets the module format name used for archiving.
 *
 * @param {String} format The name of the format.
 * @return {this}
 */
Archiver.prototype.setFormat = function(format) {
  if (this._format) {
    this.emit('error', new Error('format: archive format already set'));
    return this;
  }

  this._format = format;

  return this;
};

/**
 * Sets the module used for archiving.
 *
 * @param {Function} module The function for archiver to interact with.
 * @return {this}
 */
Archiver.prototype.setModule = function(module) {
  if (this._state.aborted) {
    this.emit('error', new Error('module: archive was aborted'));
    return this;
  }

  if (this._state.module) {
    this.emit('error', new Error('module: module already set'));
    return this;
  }

  this._module = module;
  this._modulePipe();

  return this;
};

/**
 * Returns the current length (in bytes) that has been emitted.
 *
 * @return {Number}
 */
Archiver.prototype.pointer = function() {
  return this._pointer;
};

/**
 * Middleware-like helper that has yet to be fully implemented.
 *
 * @private
 * @param  {Function} plugin
 * @return {this}
 */
Archiver.prototype.use = function(plugin) {
  this._streams.push(plugin);
  return this;
};

module.exports = Archiver;

/**
 * @typedef {Object} CoreOptions
 * @global
 * @property {Number} [statConcurrency=4] Sets the number of workers used to
 * process the internal fs stat queue.
 */

/**
 * @typedef {Object} TransformOptions
 * @property {Boolean} [allowHalfOpen=true] If set to false, then the stream
 * will automatically end the readable side when the writable side ends and vice
 * versa.
 * @property {Boolean} [readableObjectMode=false] Sets objectMode for readable
 * side of the stream. Has no effect if objectMode is true.
 * @property {Boolean} [writableObjectMode=false] Sets objectMode for writable
 * side of the stream. Has no effect if objectMode is true.
 * @property {Boolean} [decodeStrings=true] Whether or not to decode strings
 * into Buffers before passing them to _write(). `Writable`
 * @property {String} [encoding=NULL] If specified, then buffers will be decoded
 * to strings using the specified encoding. `Readable`
 * @property {Number} [highWaterMark=16kb] The maximum number of bytes to store
 * in the internal buffer before ceasing to read from the underlying resource.
 * `Readable` `Writable`
 * @property {Boolean} [objectMode=false] Whether this stream should behave as a
 * stream of objects. Meaning that stream.read(n) returns a single value instead
 * of a Buffer of size n. `Readable` `Writable`
 */

/**
 * @typedef {Object} EntryData
 * @property {String} name Sets the entry name including internal path.
 * @property {(String|Date)} [date=NOW()] Sets the entry date.
 * @property {Number} [mode=D:0755/F:0644] Sets the entry permissions.
 * @property {String} [prefix] Sets a path prefix for the entry name. Useful
 * when working with methods like `directory` or `glob`.
 * @property {fs.Stats} [stats] Sets the fs stat data for this entry allowing
 * for reduction of fs stat calls when stat data is already known.
 */

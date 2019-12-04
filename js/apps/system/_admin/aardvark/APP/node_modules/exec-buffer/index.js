'use strict';

var execFile = require('child_process').execFile;
var fs = require('fs');
var rm = require('rimraf');
var tempfile = require('tempfile');

/**
 * Run a buffer through a child process
 *
 * @api public
 */

function ExecBuffer() {
	if (!(this instanceof ExecBuffer)) {
		return new ExecBuffer();
	}

	this.src(tempfile());
	this.dest(tempfile());
}

/**
 * Add a child process to use
 *
 * @param {String} bin
 * @param {Array} args
 * @api public
 */

ExecBuffer.prototype.use = function (bin, args) {
	if (!arguments.length) {
		return this._use;
	}

	this._use = {
		args: args,
		bin: bin
	};

	return this;
};

/**
 * Get or set the temp source path
 *
 * @param {String} path
 * @api public
 */

ExecBuffer.prototype.src = function (path) {
	if (!arguments.length) {
		return this._src;
	}

	this._src = path;
	return this;
};

/**
 * Get or set the temp destination path
 *
 * @param {String} path
 * @api public
 */

ExecBuffer.prototype.dest = function (path) {
	if (!arguments.length) {
		return this._dest;
	}

	this._dest = path;
	return this;
};

/**
 * Run buffer through the child process
 *
 * @param {Buffer} buf
 * @param {Function} cb
 * @api public
 */

ExecBuffer.prototype.run = function (buf, cb) {
	fs.writeFile(this.src(), buf, function (err) {
		if (err) {
			cb(err);
			return;
		}

		execFile(this.use().bin, this.use().args, function (err, stdout, stderr) {
			if (err) {
				cb(err);
				return;
			}

			fs.readFile(this.dest(), function (err, data) {
				if (err) {
					cb(err);
					return;
				}

				this.cleanup(function (err) {
					if (err) {
						cb(err);
						return;
					}

					cb(null, data, stderr);
				});
			}.bind(this));
		}.bind(this));
	}.bind(this));
};

/**
 * Cleanup temporary files
 *
 * @param {Function} cb
 * @api private
 */

ExecBuffer.prototype.cleanup = function (cb) {
	rm(this.src(), function (err) {
		if (err) {
			cb(err);
			return;
		}

		rm(this.dest(), function (err) {
			if (err) {
				cb(err);
				return;
			}

			cb();
		});
	}.bind(this));
};

/**
 * Module exports
 */

module.exports = ExecBuffer;

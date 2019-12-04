/*
 * node-htmlprocessor
 * https://github.com/dciccale/node-htmlprocessor
 *
 * Copyright (c) 2013-2016 Denis Ciccale (@dciccale)
 * Licensed under the MIT license.
 * https://github.com/dciccale/node-htmlprocessor/blob/master/LICENSE-MIT
 */

'use strict';

var os = require('os');
var path = require('path');
var Parser = require('./parser');
var utils = require('./utils');
var blockTypes = require('./blocktypes');

// The processor
function HTMLProcessor(options) {
  var defaults = {
    data: {},
    environment: '',
    templateSettings: null,
    includeBase: null,
    commentMarker: 'build',
    strip: false,
    recursive: false,
    customBlockTypes: []
  };

  this.options = utils._.extend({}, defaults, options);
  this.data = utils._.extend({}, this.options.data, {environment: this.options.environment});
  this.parser = new Parser(this.options);

  // Register custom block types
  if (this.options.customBlockTypes.length) {
    this._registerCustomBlockTypes();
  }
}

HTMLProcessor.prototype._registerCustomBlockTypes = function () {
  this.options.customBlockTypes.forEach(function (processor) {
    require(path.resolve(processor)).call(this, this);
  }, this);
};

HTMLProcessor.prototype._blockTypes = blockTypes;

HTMLProcessor.prototype.registerBlockType = function (name, fn) {
  this._blockTypes[name] = fn;
};

// Returns a single line of the current block comment
HTMLProcessor.prototype._getBlockLine = function (block) {
  return block.raw.join(this.linefeed);
};

// Returns the block content (not including the build comments)
HTMLProcessor.prototype._getBlockContent = function (block) {
  return block.raw.slice(1, -1).join(this.linefeed);
};

// Replace passed block with the processed content
HTMLProcessor.prototype._replace = function (block, content, filePath) {
  var blockLine = this._getBlockLine(block);
  var blockContent = this._getBlockContent(block);
  var args = [content, block, blockLine, blockContent, filePath];
  var result = this._blockTypes[block.type].apply(this, args);

  return result;
};

// Strips blocks not matched for the current environment
HTMLProcessor.prototype._strip = function (block, content) {
  var blockLine = this._getBlockLine(block);
  var blockContent = this._getBlockContent(block);
  var result = content.split(blockLine).join(blockContent);

  return result;
};

HTMLProcessor.prototype._replaceBlocks = function (blocks, filePath) {
  var result = this.content;

  // Replace found blocks
  blocks.forEach(function (block) {

    // Parse through correct block type checking the build environment
    if (this._blockTypes[block.type] && (!block.targets ||
        utils._.indexOf(block.targets, this.options.environment) >= 0) || (!this.options.strip && !this.options.environment)) {
      result = this._replace(block, result, filePath);
    } else if (this.options.strip) {
      result = this._strip(block, result, filePath);
    }
  }, this);

  return result;
};

// Process the file content
HTMLProcessor.prototype.process = function (filePath) {
  return this.processContent(utils.read(filePath), filePath);
};

// Process the input content
HTMLProcessor.prototype.processContent = function (content, filePath) {
  this.content = content;
  this.linefeed = os.EOL;
  this.content = this.content.replace(/\r\n|\r|\n/g, this.linefeed);

  // Parse the file content to look for build comment blocks
  var blocks = this.parser.getBlocks(this.content, filePath);

  // Replace found blocks
  content = this._replaceBlocks(blocks, filePath);

  return content;
};

HTMLProcessor.prototype.template = utils._.template;

// Export the processor
module.exports = HTMLProcessor;

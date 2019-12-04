/*
 * grunt-contrib-watch
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

module.exports = function(grunt) {

  // Create a TaskRun on a target
  function TaskRun(target) {
    this.name = target.name || 0;
    this.files = target.files || [];
    this._getConfig = target._getConfig;
    this.options = target.options;
    this.startedAt = false;
    this.spawned = null;
    this.changedFiles = Object.create(null);
    this.spawnTaskFailure = false;
    this.livereloadOnError = true;
    if (typeof this.options.livereloadOnError !== 'undefined') {
      this.livereloadOnError = this.options.livereloadOnError;
    }
  }

  var getErrorCount = function() {
    if (typeof grunt.fail.forever_warncount !== 'undefined') {
      return grunt.fail.forever_warncount + grunt.fail.forever_errorcount;
    } else {
      return grunt.fail.warncount + grunt.fail.errorcount;
    }
  };

  // Run it
  TaskRun.prototype.run = function(done) {
    var self = this;

    // Dont run if already running
    if (self.startedAt !== false) {
      return;
    }

    // Start this task run
    self.startedAt = Date.now();

    // reset before each run
    self.spawnTaskFailure = false;
    self.errorsAndWarningsCount = getErrorCount();

    // pull the tasks here in case they were changed by a watch event listener
    self.tasks = self._getConfig('tasks') || [];
    if (typeof self.tasks === 'string') {
      self.tasks = [self.tasks];
    }

    // If no tasks just call done to trigger potential livereload
    if (self.tasks.length < 1) {
      return done();
    }

    if (self.options.spawn === false || self.options.nospawn === true) {
      grunt.task.run(self.tasks);
      done();
    } else {
      self.spawned = grunt.util.spawn({
        // Spawn with the grunt bin
        grunt: true,
        // Run from current working dir and inherit stdio from process
        opts: {
          cwd: self.options.cwd.spawn,
          stdio: 'inherit'
        },
        // Run grunt this process uses, append the task to be run and any cli options
        args: self.tasks.concat(self.options.cliArgs || [])
      }, function(err, res, code) {
        self.spawnTaskFailure = (code !== 0);
        if (self.options.interrupt !== true || (code !== 130 && code !== 1)) {
          // Spawn is done
          self.spawned = null;
          done();
        }
      });
    }
  };

  // When the task run has completed
  TaskRun.prototype.complete = function() {
    var time = Date.now() - this.startedAt;
    this.startedAt = false;
    if (this.spawned) {
      this.spawned.kill('SIGINT');
      this.spawned = null;
    }

    var taskFailed = this.spawnTaskFailure || (getErrorCount() > this.errorsAndWarningsCount);
    this.errorsAndWarningsCount = getErrorCount();

    // Trigger livereload if necessary
    if (this.livereload && (this.livereloadOnError || !taskFailed)) {
      this.livereload.trigger(Object.keys(this.changedFiles));
      this.changedFiles = Object.create(null);
    }
    return time;
  };

  return TaskRun;
};

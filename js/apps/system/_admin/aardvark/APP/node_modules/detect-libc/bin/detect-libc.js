#!/usr/bin/env node

'use strict';

const spawnSync = require('child_process').spawnSync;
const libc = require('../');

const spawnOptions = {
  env: process.env,
  shell: true,
  stdio: 'inherit'
};

if (libc.isNonGlibcLinux) {
  spawnOptions.env.LIBC = process.env.LIBC || libc.family;
}

process.exit(spawnSync(process.argv[2], process.argv.slice(3), spawnOptions).status);

'use strict';
const semver = require('semver');

function deprecated (version, graceReleases, message) {
  if (typeof version === 'number') {
    version = String(version);
  }
  if (typeof version === 'string' && !semver.valid(version)) {
    if (semver.valid(version + '.0.0')) {
      version += '.0.0';
    } else if (semver.valid(version + '.0')) {
      version += '.0';
    }
  }
  const arangoVersion = require('internal').version;
  const arangoMajor = semver.major(arangoVersion);
  const arangoMinor = semver.minor(arangoVersion);
  const deprecateMajor = semver.major(version);
  const deprecateMinor = semver.minor(version);
  if (!message && typeof graceReleases === 'string') {
    message = graceReleases;
    graceReleases = 1;
  }
  if (!message) {
    message = 'This feature is deprecated.';
  }
  if (arangoMajor >= deprecateMajor) {
    const error = new Error(`DEPRECATED: ${message}`);
    if (arangoMajor > deprecateMajor || arangoMinor >= deprecateMinor) {
      throw error;
    }
    if (arangoMinor >= (deprecateMinor - graceReleases)) {
      console.warn(error.stack);
    }
  }
}

module.exports = deprecated;

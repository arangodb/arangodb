var internal = require('internal');

exports.isIP = internal.isIP;

exports.isIPv4 = function (input) {
  return exports.isIP(input) === 4;
};

exports.isIPv6 = function (input) {
  return exports.isIP(input) === 6;
};
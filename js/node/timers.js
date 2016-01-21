exports.setImmediate =
exports.setTimeout =
exports.setInterval = function (fn) {
  fn();
  return {unref() {
    // noop
  }};
};

exports.ref =
exports.clearImmediate =
exports.clearTimeout =
exports.clearInterval = function () {
  // noop
};


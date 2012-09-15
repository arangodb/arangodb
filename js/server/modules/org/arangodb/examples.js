var console = require("console");

exports.logRequest = function (req, res, next, options) {
  console.log("received request: %s", JSON.stringify(req));
  next();
  console.log("produced response: %s", JSON.stringify(res));
}

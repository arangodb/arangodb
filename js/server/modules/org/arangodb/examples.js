var console = require("console");
var actions = require("org/arangodb/actions");

exports.logRequest = function (req, res, next, options) {
  console.log("received request: %s", JSON.stringify(req));
  console.log("error: ", next);
  next();
  console.log("produced response: %s", JSON.stringify(res));
};

exports.echoRequest = function (req, res) {
  res.responseCode = actions.HTTP_OK;
  res.contentType = "application/json";
  res.body = JSON.stringify(req);
};
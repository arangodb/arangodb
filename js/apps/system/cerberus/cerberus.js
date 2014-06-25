/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, vars: true */
/*global module, require, exports, applicationContext */
(function () {
  "use strict";

  var Foxx = require("org/arangodb/foxx"),
    users = require("org/arangodb/users"),
    controller = new Foxx.Controller(applicationContext),
    url = require("url");

  controller.get("/initpwd/:token", function (req, res) {
    var token = req.params("token"),
      username;

    //check token
    username = users.userByToken(token);

    if (username) {
      var path = url.parse(req.url).pathname.split("/");
      path = path.slice(0, path.length - 2).join("/") + "/changePassword.html";

      res.status(307);
      res.set("Location", path + "?n=" + username + "&t=" + token);
    } else {
      res.set("Content-Type", "text/plain");
      res.body = 'The token was not valid. Plaese ensure, that the url you entered was valid (no linebreaks etc.)';
    }

  });

  controller.post("/checkpwd", function (req, res) {
    var params = req.rawBody().split("&");
    var password = params[0].split("=")[1];
    var confirmPassword = params[1].split("=")[1];
    var token = params[2].split("=")[1];

    //check, if passwords are equal
    if (password !== confirmPassword) {
      var path = url.parse(req.url).pathname.split("/");
      path = path.slice(0, path.length - 2).join("/") + "/changePassword.html";

      res.status(307);
      res.set("Location", path + "?n=" + name + "&t=" + token);
      return;
    }

    if (users.changePassword(token, password)) {
      res.set("Content-Type", "text/html");
      res.body = 'Password sucessfully changed. Press <a href="/">here</a> to proceed.';
    } else {
      res.set("Content-Type", "text/plain");
      res.body = 'The token was not valid. Plaese ensure, that the url you entered was valid (no linebreaks etc.)';
    }
  });
}());
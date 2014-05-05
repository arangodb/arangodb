(function() {
  "use strict";

  var Foxx = require("org/arangodb/foxx"),
    users = require("org/arangodb/users"),
    controller = new Foxx.Controller(applicationContext),
    url = require("url");

  controller.get("/initpwd/:token", function(req, res) {
    var token = req.params("token");
    var username = users.userByToken(token);
    var path = url.parse(req.url).pathname.split("/");
    path = path.slice(0, path.length - 2).join("/");

    if (username) {
      res.set("Location", path + "/changePassword.html" + "?n=" + username + "&t=" + token);
    } else {
      res.set("Location", path + "/invalid.html");
    }
    
    res.status(303);
  });

  controller.post("/checkpwd", function(req, res) {
    var params = req.rawBody().split("&");
    var password = params[0].split("=")[1];
    var token = params[2].split("=")[1];
    var path = url.parse(req.url).pathname.split("/");
    path = path.slice(0, path.length - 1).join("/");
    
    if (users.changePassword(token, password)) {
      res.set("Location", path + "/confirmed.html");
    } else {
      res.set("Location", path + "/invalid.html");
    }
    
    res.status(303);
  });
}());

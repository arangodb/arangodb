(function() {
  "use strict";

  var Foxx = require("org/arangodb/foxx"),
    users = require("org/arangodb/users"),
    controller = new Foxx.Controller(applicationContext)

  controller.get("/initpwd/:token", function(req, res) {
    var token = req.params("token"),
      username;

    //check token
    username = users.userByToken(token);

//    token = users.setPasswordToken(username);

    if (username) {
      res.status(307);
      res.set("Location", "/system/cerberus/changePassword.html?n="+username+"&t="+token);
    } else {
      res.set("Content-Type", "text/plain");
      res.body = 'The token was not valid. Plaese ensure, that the url you entered was valid (no linebreaks etc.)';
    }

  });

  controller.post("/checkpwd", function(req, res) {
    var params = req.rawBody().split("&");
    var password = params[0].split("=")[1];
    var confirmPassword = params[1].split("=")[1];
    var token = params[2].split("=")[1];
    //check, if passwords are equal
    if(password !== confirmPassword) {
      res.status(307);
      res.set("Location", "/system/cerberus/changePassword.html?n="+name+"&t="+token);
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
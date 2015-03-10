/*global require, applicationContext */
(function () {
    "use strict";

    var Foxx = require("org/arangodb/foxx"),
        users = require("org/arangodb/users"),
        controller = new Foxx.Controller(applicationContext),
        url = require("url"),
        token,
        path,
        password;

    controller.get("/initpwd/:token", function (req, res) {
        token = req.params("token");
        var username = users.userByToken(token);
        path = url.parse(req.url).pathname.split("/");
        path = path.slice(0, path.length - 2).join("/");

        if (username) {
            res.set("Location", path + "/changePassword.html" + "?n=" + encodeURIComponent(username.user) + "&t=" + token);
        } else {
            res.set("Location", path + "/invalid.html");
        }

        res.status(303);
    });

    controller.post("/checkpwd", function (req, res) {
        var params = req.rawBody().split("&");
        password = params[0].split("=")[1];
        token = params[2].split("=")[1];
        path = url.parse(req.url).pathname.split("/");
        path = path.slice(0, path.length - 1).join("/");

        if (users.changePassword(decodeURIComponent(token), decodeURIComponent(password))) {
            res.set("Location", path + "/confirmed.html");
        } else {
            res.set("Location", path + "/invalid.html");
        }

        res.status(303);
    });
}());

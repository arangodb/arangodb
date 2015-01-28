/*jshint strict: false, -W083 */
/*global require */

// Discover a new server and give it a role

var download = require("internal").download;
var print = require("internal").print;
var wait = require("internal").wait;
var highestDBServer = 0;
var highestCoordinator = 0;

var result;
var body;
var nodes;
var newservers;
var oldservers;

while (true) {
  result = download("http://localhost:4001/v2/keys/arango/Current/NewServers","",{"method":"GET"});
  body = JSON.parse(result.body);
  nodes = body.node.nodes;
  if (nodes === undefined) {
    newservers = [];
  }
  else {
    newservers = nodes.map(function(x) { return x.key.substr(27); });
  }
  print("New:", newservers);
  result = download("http://localhost:4001/v2/keys/arango/Target/MapLocalToID","",{"method":"GET"});
  body = JSON.parse(result.body);
  nodes = body.node.nodes;
  if (nodes === undefined) {
    oldservers = [];
  }
  else {
    oldservers = nodes.map(function(x) { return x.key.substr(28); });
  }
  print("Old:", oldservers);
  
  var i;
  for (i = 0; i < newservers.length; i++) {
    if (oldservers.indexOf(newservers[i]) === -1) {
      print("Configuring new server", newservers[i]);
      var server = newservers[i];
      var isDB = false;
      if (server.substr(0, 9) === "dbserver:") {
        isDB = true;
        server = server.substr(9);
        highestDBServer += 1;
      }
      else if (server.substr(0, 12) === "coordinator:") {
        isDB = false;
        server = server.substr(12);
        highestCoordinator += 1;
      }
      else {
        continue;
      }
      var entity = isDB ? "DBServers" : "Coordinators";
      var serverName = isDB ? "DBServer"+highestDBServer
                            : "Coordinator"+highestCoordinator;
      var url = "http://localhost:4001/v2/keys/arango/Plan/" + entity + "/" +
                serverName;
      var headers = { "Content-Type" : "application/x-www-form-urlencoded" };
      var res = download(url, "value="+encodeURIComponent('"none"'), 
                         { "method": "PUT", "headers": headers,
                           "followRedirects": true });
      print("Result of first PUT: ", JSON.stringify(res));
      url = "http://localhost:4001/v2/keys/arango/Target/MapLocalToID/"+newservers[i];
      body = "value=" + encodeURIComponent(JSON.stringify( 
                            {"endpoint":"tcp://"+server, "ID":serverName}));
      print(url);
      print(body);
      var res = download(url, body, { "method": "PUT", "headers" : headers,
                                      "followRedirects": true });
      print("Result of second PUT: ", JSON.stringify(res));
    }
  }
      
  wait(3);
}

/*jshint globalstrict:false, strict:false */
var agencyData = {
  "arango" : {
     "Sync" : {
        "LatestID" : "\"1\"",
        "Problems" : {},
        "UserVersion" : "\"1\"",
        "ServerStates" : {},
        "HeartbeatIntervalMs" : "5000",
        "Commands" : {}
     },
     "Current" : {
        "Collections" : {
           "_system" : {}
        },
        "Version" : "\"1\"",
        "ShardsCopied" : {},
        "NewServers" : {},
        "Coordinators" : {},
        "Lock" : "\"UNLOCKED\"",
        "DBservers" : {},
        "ServersRegistered" : {
           "Version" : "\"1\""
        },
        "Databases" : {
           "_system" : {
              "id" : "\"1\"",
              "name" : "\"name\""
           }
        }
     },
     "Plan" : {
        "Coordinators" : {
        },
        "Databases" : {
           "_system" : "{\"name\":\"_system\", \"id\":\"1\"}"
        },
        "DBServers" : {
        },
        "Version" : "\"1\"",
        "Collections" : {
           "_system" : {
           }
        },
        "Lock" : "\"UNLOCKED\""
     },
     "Launchers" : {
     },
     "Target" : {
        "Coordinators" : {
        },
        "MapIDToEndpoint" : {
        },
        "Collections" : {
           "_system" : {}
        },
        "Version" : "\"1\"",
        "MapLocalToID" : {},
        "Databases" : {
           "_system" : "{\"name\":\"_system\", \"id\":\"1\"}"
        },
        "DBServers" : {
        },
        "Lock" : "\"UNLOCKED\""
     }
  }
};

var download = require("internal").download;
var print = require("internal").print;
var wait = require("internal").wait;

function encode (st) {
  var st2 = "";
  var i;
  for (i = 0; i < st.length; i++) {
    if (st[i] === "_") {
      st2 += "@U";
    }
    else if (st[i] === "@") {
      st2 += "@@";
    }
    else {
      st2 += st[i];
    }
  }
  return encodeURIComponent(st2);
}

function sendToAgency (agencyURL, path, obj) {
  var res;
  var body;

  print("Sending",path," to agency...");
  if (typeof obj === "string") {
    var count = 0;
    while (count++ <= 2) {
      body = "value="+encodeURIComponent(obj);
      print("Body:", body);
      print("URL:", agencyURL+path);
      res = download(agencyURL+path,body,
          {"method":"PUT", "followRedirects": true,
           "headers": { "Content-Type": "application/x-www-form-urlencoded"}});
      if (res.code === 201 || res.code === 200) {
        return true;
      }
      wait(3);  // wait 3 seconds before trying again
    }
    return res;
  }
  if (typeof obj !== "object") {
    return "Strange object found: not a string or object";
  }
  var keys = Object.keys(obj);
  var i;
  if (keys.length !== 0) {
    for (i = 0; i < keys.length; i++) {
      res = sendToAgency (agencyURL, path+encode(keys[i])+"/", obj[keys[i]]);
      if (res !== true) {
        return res;
      }
    }
    return true;
  }
  else {
    body = "dir=true";
    res = download(agencyURL+path, body,
          {"method": "PUT", "followRedirects": true,
           "headers": { "Content-Type": "application/x-www-form-urlencoded"}});
    if (res.code !== 201 && res.code !== 200) {
      return res;
    }
    return true;
  }
}

print("Starting to send data to Agency...");
var res = sendToAgency("http://localhost:4001/v2/keys", "/", agencyData);
print("Result:",res);

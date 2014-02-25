/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, plannerTemplateEngine */
(function() {

    "use strict";

    window.PlanScenarioSelectorView = Backbone.View.extend({
        el: '#content',

        template: templateEngine.createTemplate("planScenarioSelector.ejs", "planner"),


        events: {
            "click #multiServerSymmetrical": "multiServerSymmetrical",
            "click #multiServerAsymmetrical": "multiServerAsymmetrical",
            "click #singleServer": "singleServer",
            "click #importRunInfo": "togglePlanImport",
            "click #confirmRunInfoImport": "uploadPlan",
            "click #importConnectInfo": "toggleConnectImport",
            "click #confirmConnectImport": "uploadConnect"
        },

        render: function() {
          $(this.el).html(this.template.render({}));
        },
        multiServerSymmetrical: function() {
          window.App.navigate(
            "planSymmetrical", {trigger: true}
          );
        },
        multiServerAsymmetrical: function() {
          window.App.navigate(
            "planAsymmetrical", {trigger: true}
          );
        },
        singleServer: function() {
          window.App.navigate(
            "planTest", {trigger: true}
          );
        },

        togglePlanImport: function() {
          this.hideConnectImportModal();
          $('#runinfoDropdownImport').slideToggle(200);
        },

        hidePlanImportModal: function() {
          $('#runinfoDropdownImport').hide();
        },

        toggleConnectImport: function() {
          this.hidePlanImportModal();
          $('#connectDropdownImport').slideToggle(200);
        },

        hideConnectImportModal: function() {
          $('#connectDropdownImport').hide();
        },

        uploadConnect: function() {
          alert("Sorry not yet implemented");
          return;
          var info = {"config":{"dispatchers":{"d1":{"endpoint":"tcp://localhost:8529"}},"numberOfDBservers":2,"numberOfCoordinators":1},"plan":{"dispatchers":{"d1":{"endpoint":"tcp://localhost:8529","id":"d1","avoidPorts":{"4004":true,"4005":true,"4006":true,"7004":true,"7005":true,"7006":true,"8531":true,"8631":true,"8632":true},"arangodExtraArgs":[],"allowCoordinators":true,"allowDBservers":true}},"commands":[{"action":"startAgent","dispatcher":"d1","extPort":4004,"intPort":7004,"peers":[],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"startAgent","dispatcher":"d1","extPort":4005,"intPort":7005,"peers":["tcp://localhost:7004"],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"startAgent","dispatcher":"d1","extPort":4006,"intPort":7006,"peers":["tcp://localhost:7004","tcp://localhost:7005"],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"sendConfiguration","agency":{"agencyPrefix":"meier","endpoints":["tcp://localhost:4004","tcp://localhost:4005","tcp://localhost:4006"]},"data":{"meier":{"Target":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBServers":{"Pavel":"\"none\"","Perry":"\"none\""},"MapLocalToEndpoint":{},"MapIDToEndpoint":{"Pavel":"\"tcp://localhost:8631\"","Perry":"\"tcp://localhost:8632\"","Claus":"\"tcp://localhost:8531\""},"Coordinators":{"Claus":"\"none\""},"Databases":{"_system":"{\"name\":\"_system\"}"},"Collections":{"_system":{}}},"Plan":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBServers":{"Pavel":"\"none\"","Perry":"\"none\""},"MapLocalToEndpoint":{},"Coordinators":{"Claus":"\"none\""},"Databases":{"_system":"{\"name\":\"_system\"}"},"Collections":{"_system":{}}},"Current":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBservers":{},"Coordinators":{},"Databases":{"_system":{}},"Collections":{"_system":{}},"ServersRegistered":{"Version":"\"1\""},"ShardsCopied":{}},"Sync":{"ServerStates":{},"Problems":{},"LatestID":"\"0\"","Commands":{},"HeartbeatIntervalMs":"1000"},"Launchers":{"d1":"{\"DBservers\":[\"Pavel\",\"Perry\"],\"Coordinators\":[\"Claus\"]}"}}}},{"action":"startServers","dispatcher":"d1","DBservers":["Pavel","Perry"],"Coordinators":["Claus"],"name":"d1","dataPath":"","logPath":"","arangodPath":"","onlyLocalhost":true,"agency":{"agencyPrefix":"meier","endpoints":["tcp://localhost:4004","tcp://localhost:4005","tcp://localhost:4006"]}},{"action":"createSystemColls","url":"http://localhost:8531"},{"action":"initializeFoxx","url":"http://localhost:8531"}]},"runInfo":{"error":false,"errorMessage":"none","runInfo":[{"error":false,"isStartAgent":true,"pid":{"pid":36758},"endpoint":"tcp://localhost:4004"},{"error":false,"isStartAgent":true,"pid":{"pid":36759},"endpoint":"tcp://localhost:4005"},{"error":false,"isStartAgent":true,"pid":{"pid":36760},"endpoint":"tcp://localhost:4006"},{"error":false,"isSendConfiguration":true},{"error":false,"isStartServers":true,"pids":[{"pid":36764},{"pid":36765},{"pid":36766}],"endpoints":["tcp://localhost:8631","tcp://localhost:8632","tcp://localhost:8531"],"roles":["DBserver","DBserver","Coordinator"]},{"code":200,"message":"OK","headers":{"connection":"Keep-Alive","content-length":"26","content-type":"application/json; charset=utf-8","http/1.1":"200 OK","server":"ArangoDB"},"body":"{\"error\":false,\"code\":200}"},{"error":false}]}};
          console.log(info);
          $.ajax("cluster/shutdown", {
            type: "POST",
            contentType: "application/json",
            data: JSON.stringify(info)
          });
          this.hideConnectImportModal(); 
        },

        uploadPlan: function() {
          alert("Sorry not yet implemented");
          return;
          var info = {"config":{"dispatchers":{"d1":{"endpoint":"tcp://localhost:8529"}},"numberOfDBservers":2,"numberOfCoordinators":1},"plan":{"dispatchers":{"d1":{"endpoint":"tcp://localhost:8529","id":"d1","avoidPorts":{"4004":true,"4005":true,"4006":true,"7004":true,"7005":true,"7006":true,"8531":true,"8631":true,"8632":true},"arangodExtraArgs":[],"allowCoordinators":true,"allowDBservers":true}},"commands":[{"action":"startAgent","dispatcher":"d1","extPort":4004,"intPort":7004,"peers":[],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"startAgent","dispatcher":"d1","extPort":4005,"intPort":7005,"peers":["tcp://localhost:7004"],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"startAgent","dispatcher":"d1","extPort":4006,"intPort":7006,"peers":["tcp://localhost:7004","tcp://localhost:7005"],"agencyPrefix":"meier","dataPath":"","logPath":"","agentPath":"","onlyLocalhost":true},{"action":"sendConfiguration","agency":{"agencyPrefix":"meier","endpoints":["tcp://localhost:4004","tcp://localhost:4005","tcp://localhost:4006"]},"data":{"meier":{"Target":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBServers":{"Pavel":"\"none\"","Perry":"\"none\""},"MapLocalToEndpoint":{},"MapIDToEndpoint":{"Pavel":"\"tcp://localhost:8631\"","Perry":"\"tcp://localhost:8632\"","Claus":"\"tcp://localhost:8531\""},"Coordinators":{"Claus":"\"none\""},"Databases":{"_system":"{\"name\":\"_system\"}"},"Collections":{"_system":{}}},"Plan":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBServers":{"Pavel":"\"none\"","Perry":"\"none\""},"MapLocalToEndpoint":{},"Coordinators":{"Claus":"\"none\""},"Databases":{"_system":"{\"name\":\"_system\"}"},"Collections":{"_system":{}}},"Current":{"Lock":"\"UNLOCKED\"","Version":"\"1\"","DBservers":{},"Coordinators":{},"Databases":{"_system":{}},"Collections":{"_system":{}},"ServersRegistered":{"Version":"\"1\""},"ShardsCopied":{}},"Sync":{"ServerStates":{},"Problems":{},"LatestID":"\"0\"","Commands":{},"HeartbeatIntervalMs":"1000"},"Launchers":{"d1":"{\"DBservers\":[\"Pavel\",\"Perry\"],\"Coordinators\":[\"Claus\"]}"}}}},{"action":"startServers","dispatcher":"d1","DBservers":["Pavel","Perry"],"Coordinators":["Claus"],"name":"d1","dataPath":"","logPath":"","arangodPath":"","onlyLocalhost":true,"agency":{"agencyPrefix":"meier","endpoints":["tcp://localhost:4004","tcp://localhost:4005","tcp://localhost:4006"]}},{"action":"createSystemColls","url":"http://localhost:8531"},{"action":"initializeFoxx","url":"http://localhost:8531"}]},"runInfo":{"error":false,"errorMessage":"none","runInfo":[{"error":false,"isStartAgent":true,"pid":{"pid":36758},"endpoint":"tcp://localhost:4004"},{"error":false,"isStartAgent":true,"pid":{"pid":36759},"endpoint":"tcp://localhost:4005"},{"error":false,"isStartAgent":true,"pid":{"pid":36760},"endpoint":"tcp://localhost:4006"},{"error":false,"isSendConfiguration":true},{"error":false,"isStartServers":true,"pids":[{"pid":36764},{"pid":36765},{"pid":36766}],"endpoints":["tcp://localhost:8631","tcp://localhost:8632","tcp://localhost:8531"],"roles":["DBserver","DBserver","Coordinator"]},{"code":200,"message":"OK","headers":{"connection":"Keep-Alive","content-length":"26","content-type":"application/json; charset=utf-8","http/1.1":"200 OK","server":"ArangoDB"},"body":"{\"error\":false,\"code\":200}"},{"error":false}]}};
          $.ajax("cluster/shutdown", {
            type: "POST",
            contentType: "application/json",
            data: JSON.stringify(info)
          });
          this.hidePlanImportModal(); 
        }
    });
}());

/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoCollection*/
(function() {
  "use strict";

  describe("Plan Test View", function() {

    var myView, ip, port, coords, dbs;

    beforeEach(function() {
      $('body').append('<div id="content" class="removeMe"></div>');

      ip = "192.168.0.1";
      port = 8529;
      coords = 4;
      dbs = 5;


      myView = new window.PlanTestView();
      myView.render();

      $("#host").val(ip);
      $("#port").val(port);
      $("#coordinators").val(coords);
      $("#dbs").val(dbs);
      spyOn($, "ajax");
      spyOn(window, "alert");
    });

    afterEach(function() {
      $('.removeMe').remove();
    });

    it("should start a cluster", function() {
      $("#startPlan").click();
      expect($.ajax).toHaveBeenCalledWith("cluster/plan", {
        type: "POST",
        data: {
          dispatcher: ip + ":" + port,
          numberCoordinators: coords,
          numberDBServers: dbs,
          type: "testSetup"
        }
      });
    });

    it("should not start a cluster if dispatcher port is missing", function() {
      $("#host").val("");
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please define a Host");
    });

    it("should not start a cluster if dispatcher host is missing", function() {
      $("#port").val("");
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please define a Port");
    });

    it("should not start a cluster if coordinator number is missing", function() {
      $("#coordinators").val("");
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please define a number of Coordinators");
    });

    it("should not start a cluster if db number is missing", function() {
      $("#dbs").val("");
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please define a number of DBServers");
    });

  });
}());

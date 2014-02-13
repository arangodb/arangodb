/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoCollection*/
(function() {
  "use strict";

  describe("Plan Symmetric/Asymmetric View", function() {
    var myView, ip_base;

    beforeEach(function() {
      $('body').append('<div id="content" class="removeMe"></div>');

      myView = new window.PlanSymmetricView();
      myView.render(true);

      ip_base = "123.456.78";

      spyOn($, "ajax");
      spyOn(window, "alert");
    });

    afterEach(function() {
      $('.removeMe').remove();
    });

    it("should start a symmetrical cluster", function() {
      myView.render(true);
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
          $(".host", d).val(ip_base + i);
          $(".port", d).val(i);
      });
      $("#startPlan").click();
      expect($.ajax).toHaveBeenCalledWith("cluster/plan", {
        type: "POST",
        data: JSON.stringify({
          dbServer: [ip_base + "0:0", ip_base + "1:1", ip_base + "2:2"],
          coordinator: [ip_base + "0:0", ip_base + "1:1", ip_base + "2:2"],
          type: "symmetricalSetup"
        })
      });
    });
    it("should start a asymmetrical cluster", function() {
      myView.render(false);
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
          $(".host", d).val(ip_base + i);
          $(".port", d).val(i);
          if (i === 0) {
            $(".isDBServer", d).val(true);
            $(".isCoordinator", d).val(false)
          } else if (i === 1) {
              $(".isDBServer", d).val(false);
            $(".isCoordinator", d).val(true);
          } else {
            $(".isDBServer", d).val(true);
            $(".isCoordinator", d).val(true);
          }
      });

      $("#startPlan").click();
      expect($.ajax).toHaveBeenCalledWith("cluster/plan", {
          type: "POST",
          data: JSON.stringify({
              dbServer: [ip_base + "0:0",  ip_base + "2:2"],
              coordinator: [ ip_base + "1:1", ip_base + "2:2"],
              type: "asymmetricalSetup"
          })
      });
    });

    it("should not start a symmetrical cluster because no complete host ist provided", function() {
      myView.render(true);
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
          if (i === 0) {
            $(".host", d).val(ip_base + i);
          } else  {
            $(".port", d).val(i);
          }
      });
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please provide at least one dispatcher");
    });

    it("should not start an asymmetrical cluster because no dbserver and coordinator ist provided", function() {
      myView.render(false);
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
        $(".host", d).val(ip_base + i);
        $(".port", d).val(i);
        if (i === 0) {
            $(".isDBServer", d).val(false);
            $(".isCoordinator", d).val(false)
        } else if (i === 1) {
            $(".isDBServer", d).val(false);
            $(".isCoordinator", d).val(false);
        } else {
            $(".isDBServer", d).val(false);
            $(".isCoordinator", d).val(false);
        }
      });
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please provide at least one DBServer");
    });
    it("should not start an asymmetrical cluster because no coordinator ist provided", function() {
      myView.render(false);
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
          $(".host", d).val(ip_base + i);
          $(".port", d).val(i);
          if (i === 0) {
              $(".isDBServer", d).val(true);
              $(".isCoordinator", d).val(false)
          } else if (i === 1) {
              $(".isDBServer", d).val(false);
              $(".isCoordinator", d).val(false);
          } else {
              $(".isDBServer", d).val(false);
              $(".isCoordinator", d).val(false);
          }
      });
      $("#startPlan").click();
      expect($.ajax).not.toHaveBeenCalled();
      expect(window.alert).toHaveBeenCalledWith("Please provide at least one Coordinator");
    });


    it("should be able to add  3 and remove 2 input lines for hosts", function() {
      myView.render(false);
      $(".add").click();
      $(".add").click();
      $(".add").click();
      $(".dispatcher").each(function (i, d) {
         if (i >= 2) {
             $(".delete", d).click();
         }
      });
      expect($(".dispatcher").length).toEqual(2);
    });


  });
}());

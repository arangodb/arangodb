/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $, _, uiMatchers*/
(function() {

  "use strict";

  describe("Cluster Servers Collection", function() {

    var col, list, prim1, prim2, prim3, sec1, sec2, sec3, oldRouter;

    beforeEach(function() {
      oldRouter = window.App;
      window.App = {
        registerForUpdate: function(){},
        addAuth: function(){}
      };
      list = [];
      sec1 = {
        role: "secondary",
        address: "tcp://192.168.1.1:1337",
        name: "Sally"
      };
      sec2 = {
        role: "secondary",
        address: "tcp://192.168.1.2:1337",
        name: "Sandy"
      };
      sec3 = {
        role: "secondary",
        address: "tcp://192.168.1.2:1337",
        name: "Sammy"
      };
      prim1 = {
        role: "primary",
        secondary: sec1.name,
        address: "tcp://192.168.0.1:1337",
        name: "Pavel"
      };
      prim2 = {
        role: "primary",
        secondary: sec2.name,
        address: "tcp://192.168.0.2:1337",
        name: "Pancho"
      };
      prim3 = {
        role: "primary",
        secondary: sec3.name,
        address: "tcp://192.168.0.3:1337",
        name: "Pepe"
      };
      col = new window.ClusterServers();
      spyOn(col, "fetch").andCallFake(function() {
        _.each(list, function(s) {
          col.add(s);
        });
      });
    });

    afterEach(function() {
      window.App = oldRouter;
    });

    describe("list overview", function() {

      it("should fetch the result sync", function() {
        col.fetch.reset();
        col.getOverview();
        expect(col.fetch).toHaveBeenCalledWith({
          async: false,
          beforeSend: jasmine.any(Function)
        });
      });
      

      it("should return a status ok if all servers are running", function() {
        prim1.status = "ok";
        prim2.status = "ok";
        prim3.status = "ok";
        sec1.status = "ok";
        sec2.status = "ok";
        sec3.status = "ok";
        list.push(prim1);
        list.push(prim2);
        list.push(prim3);
        list.push(sec1);
        list.push(sec2);
        list.push(sec3);
        var expected = {
          having: 3,
          plan: 3,
          status: "ok"
        };
        expect(col.getOverview()).toEqual(expected);
      });

      it("should return a status ok if all primary servers are running", function() {
        prim1.status = "ok";
        prim2.status = "ok";
        list.push(prim1);
        list.push(prim2);
        var expected = {
          having: 2,
          plan: 2,
          status: "ok"
        };
        expect(col.getOverview()).toEqual(expected);
      });

      it("should return a status warning if there was a failover", function() {
        prim1.status = "critical";
        prim2.status = "ok";
        sec1.status = "ok";
        sec2.status = "ok";
        list.push(prim1);
        list.push(prim2);
        list.push(sec1);
        list.push(sec2);
        var expected = {
          having: 2,
          plan: 2,
          status: "warning"
        };
        expect(col.getOverview()).toEqual(expected);
      });

      it("should return a status critical if one pair is missing", function() {
        prim1.status = "critical";
        prim2.status = "ok";
        sec1.status = "critical";
        sec2.status = "ok";
        list.push(prim1);
        list.push(prim2);
        list.push(sec1);
        list.push(sec2);
        var expected = {
          having: 1,
          plan: 2,
          status: "critical"
        };
        expect(col.getOverview()).toEqual(expected);
      });

    });

    describe("the pair list", function() {

      it("should fetch the result sync", function() {
        col.fetch.reset();
        col.getList();
        expect(col.fetch).toHaveBeenCalledWith({
          async: false,
          beforeSend: jasmine.any(Function)
        });
      });
      
      it("should combine all pairs", function() {
        prim1.status = "ok";
        prim2.status = "warning";
        prim3.status = "critical";
        sec1.status = "critical";
        sec2.status = "ok";
        delete prim3.secondary;
        list.push(prim1);
        list.push(prim2);
        list.push(prim3);
        list.push(sec1);
        list.push(sec2);
        var expected = [];
        expected.push({
          primary: {
            name: prim1.name,
            status: prim1.status,
            address: prim1.address
          },
          secondary: {
            name: sec1.name,
            status: sec1.status,
            address: sec1.address
          }
        });
        expected.push({
          primary: {
            name: prim2.name,
            status: prim2.status,
            address: prim2.address
          },
          secondary: {
            name: sec2.name,
            status: sec2.status,
            address: sec2.address
          }
        });
        expected.push({
          primary: {
            name: prim3.name,
            status: prim3.status,
            address: prim3.address
          }
        });
        expect(col.getList()).toEqual(expected);
      });
    });
  });

}());

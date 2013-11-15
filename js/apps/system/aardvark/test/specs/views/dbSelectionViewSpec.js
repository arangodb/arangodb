/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect, runs, waitsFor*/
/*global _, $, DBSelectionView*/

(function() {
  "use strict";

  describe("DB Selection", function() {
  
    var view,
      list,
      dbCollection,
      fetched,
      current,
      div;

    beforeEach(function() {
      dbCollection = new window.ArangoDatabase();
      list = ["_system", "second", "first"];
      current = new window.CurrentDatabase({
        name: "first",
        isSystem: false
      });
      _.each(list, function(n) {
        dbCollection.add({name: n});
      });
      fetched = false;
      spyOn(dbCollection, "fetch").andCallFake(function(opt) {
        fetched = true;
        opt.success();
      });
      div = document.createElement("div");
      div.id = "dbSelect";
      document.body.appendChild(div);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should display all databases ordered", function() {

      runs(function() {
        view = new DBSelectionView(
          {
            collection: dbCollection,
            current: current
          }
        );
      });

      waitsFor(function() {
        return fetched;
      }, 1000);

      runs(function() {
        var select = $(div).children()[0],
          childs;
        expect(div.childElementCount).toEqual(1);
        childs = $(select).children();
        expect(childs.length).toEqual(3);
        expect(childs[0].id).toEqual(list[0]);
        expect(childs[1].id).toEqual(list[2]);
        expect(childs[2].id).toEqual(list[1]);
      });
    });

    it("should select the current db", function() {
      runs(function() {
        view = new DBSelectionView(
          {
            collection: dbCollection,
            current: current
          }
        );
      });

      waitsFor(function() {
        return fetched;
      }, 1000);

      runs(function() {
        expect($(div).find(":selected").attr("id")).toEqual(current.get("name"));
      });
    });

    it("should trigger fetch on collection", function() {
      runs(function() {
        view = new DBSelectionView(
          {
            collection: dbCollection,
            current: current
          }
        );
      });

      waitsFor(function() {
        return fetched;
      }, 1000);

      runs(function() {
        expect(dbCollection.fetch).toHaveBeenCalled();
      });
    });

    it("should trigger a database switch on click", function() {
      runs(function() {
        view = new DBSelectionView(
          {
            collection: dbCollection,
            current: current
          }
        );
      });
      
      waitsFor(function() {
        return fetched;
      }, 1000);

      runs(function() {
        spyOn(dbCollection, "createDatabaseURL").andReturn("switchURL");
        spyOn(location, "replace");
        $("#dbSelectionList").val("second").trigger("change");
        expect(dbCollection.createDatabaseURL).toHaveBeenCalledWith("second");
        expect(location.replace).toHaveBeenCalledWith("switchURL");
      });
    });

    it("should not render if the list has only one element", function() {
      runs(function() {
        var oneCollection = new window.ArangoDatabase();
        oneCollection.add({name: current});
        spyOn(oneCollection, "fetch").andCallFake(function(opts) {
          fetched = true;
          opts.success();
        });
        view = new DBSelectionView({
          collection: oneCollection,
          current: current
        });
      });

      waitsFor(function() {
        return fetched;
      }, 1000);

      runs(function() {
        expect($(div).text().trim()).toEqual("");
      });
    });
  });
}());

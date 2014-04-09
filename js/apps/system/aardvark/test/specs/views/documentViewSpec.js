/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The document view", function() {

    var view, model;

    beforeEach(function() {
      model = new window.arangoDocument({
        _key: "123",
        _id: "v/123",
        _rev: "123",
        name: "alice"
      });
      view = new window.DocumentView();
    });

    it("assert the basics", function () {
      expect(view.colid).toEqual(0);
      expect(view.colid).toEqual(0);
      expect(view.editor).toEqual(0);
      expect(view.events).toEqual({
        "click #saveDocumentButton" : "saveDocument"
      });
    });

    it("should render", function () {
    });

    it("should copy a model into editor", function () {
    });


/*    it("check document view type check", function() {

      var arangoDocStoreDummy = {
        getEdge: function () {
        },
        getDocument: function () {
        },
        first: function () {
          return model;
        }
      };


      spyOn(window, "arangoDocumentStore").andReturn(arangoDocStoreDummy);
      spyOn(arangoDocStoreDummy, "getEdge").andReturn(true);

      expect(view.typeCheck('edge')).toEqual(true);
    });*/

    it("should remove readonly keys", function() {
      var object = {
        hello: 123,
        wrong: true,
        _key: "123",
        _rev: "adasda",
        _id: "paosdjfp1321"
      },
      shouldObject = {
        hello: 123,
        wrong: true
      },
      result = view.removeReadonlyKeys(object);

      expect(result).toEqual(shouldObject);
    });

    it("should modify the breadcrumb", function () {
      var bar = document.createElement("div"),
        emptyBar = document.createElement("div");
      bar.id = 'transparentHeader';


      view.breadcrumb();
      expect(emptyBar).not.toBe(bar);
    });

    //it("should escape the value string", function () {
    // FUNKTION NICHT MEHR BENUTZT??
    //});


  });

}());

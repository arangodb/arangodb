/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoHelper */

(function() {
  "use strict";
  
  describe("Foxx Edit View", function() {
    var view,
      model,
      div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "modalPlaceholder";
      document.body.appendChild(div);
      model = new window.Foxx({
        _key: "1234",
        _id: "aal/1234",
        title: "TestFoxx",
        version: 1.2,
        mount: "/testfoxx",
        description: "Test application",
        git: "gitpath",
        app: "app:TestFoxx:1.2",
        isSystem: false,
        options: {
          collectionPrefix: "testfoxx"
        },
        development: false
      });
      view = new window.foxxEditView({model: model}); 
      view.render();
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should be able to switch mount point", function() {
      var calledBack,
        newMount;
      
      runs(function() {
        newMount = "/newMount";
        spyOn($, "ajax").andCallFake(function(url, options) {
          calledBack = true;
        });
        $("#change-mount-point").val(newMount);
        $("#change").click();
      });

      waitsFor(function() {
        return calledBack;
      }, 1000);

      runs(function() {
        expect($.ajax).toHaveBeenCalledWith(
          "foxx/move/" + model.get("_key"),{
            dataType: "json",
            data: JSON.stringify({
              mount: newMount,
              app: model.get("app"),
              prefix: model.get("options").collectionPrefix
            }),
            async: false,
            type: "PUT",
            error: jasmine.any(Function)
          });
      });
    });

    it("should not hide the modal view if mountpoint change has failed", function() {
      var calledBack,
        newMount,
        resText;
      
      runs(function() {
        newMount = "/newMount";
        resText = "Mount-Point already in use";
        spyOn($, "ajax").andCallFake(function(url, options) {
          expect(options.error).toEqual(jasmine.any(Function));
          options.error(
            {
              status: 409,
              statusText: "Conflict",
              responseText: resText
            }
          );
          calledBack = true;
        });
        spyOn(view, "hideModal");
        spyOn(arangoHelper, "arangoError");
        $("#change-mount-point").val(newMount);
        $("#change").click();
      });

      waitsFor(function() {
        return calledBack;
      }, 1000);

      runs(function() {
        expect(view.hideModal).not.toHaveBeenCalled();
        expect(arangoHelper.arangoError).toHaveBeenCalledWith(resText);
      });

    });

    it("should not trigger a remount if mountpoint has not been changed", function() {
      spyOn(window, "alert");
      $("#change").click();
      expect(window.alert).not.toHaveBeenCalled();
    });

    it("should prevent an illegal mountpoint", function() {
      spyOn(window, "alert");
      $("#change-mount-point").val("illegal");
      $("#change").click();
      expect(window.alert).toHaveBeenCalled();
    });
  });
}());

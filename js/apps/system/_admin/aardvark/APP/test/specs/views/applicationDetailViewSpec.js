/*jshint browser: true */
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoHelper */

(function() {
  "use strict";

  describe("Application Details View", function() {

    var div, view, appDummy, deleteButton, openButton, modalDiv;

    beforeEach(function() {
      spyOn(arangoHelper, "currentDatabase").andReturn("_system");
      modalDiv = document.createElement("div");
      modalDiv.id = "modalPlaceholder";
      document.body.appendChild(modalDiv);
      window.modalView = new window.ModalView();
      div = document.createElement("div");
      div.id = "content";
      document.body.appendChild(div);
      appDummy = new window.Foxx({
        "author": "ArangoDB GmbH",
        "name": "MyApp",
        "mount": "/my/app",
        "version": "1.0.0",
        "description": "Description of my app",
        "license": "Apache 2",
        "contributors": [],
        "git": "",
        "system": false,
        "development": false
      });
      view = new window.ApplicationDetailView({
        model: appDummy
      });
      view.render();
      deleteButton = $("input.delete");
      openButton = $("input.open");
      window.CreateDummyForObject(window, "Router");
      window.App = new window.Router();
      spyOn(window.App, "navigate");
    });

    afterEach(function() {
      document.body.removeChild(div);
      delete window.modalView;
      delete window.App;
      document.body.removeChild(modalDiv);
    });

    it("should be able to delete the app", function() {
      runs(function() {
        deleteButton.click();
      });

      waitsFor(function () {
        return $("#modal-dialog").css("display") === "block";
      }, "The confirmation dialog should be shown", 750);

      runs(function() {
        spyOn($, "ajax").andCallFake(function(opts) {
          expect(opts.url).toEqual("/_admin/aardvark/foxxes?mount=" + appDummy.encodedMount());
          expect(opts.type).toEqual("DELETE");
          expect(opts.success).toEqual(jasmine.any(Function));
          expect(opts.error).toEqual(jasmine.any(Function));
          opts.success({error: false});
        });
        $("#modalButton1").click();
      });

      waitsFor(function () {
        return $("#modal-dialog").css("display") === "none";
      }, "The confirmation dialog should be hidden", 750);

      runs(function() {
        expect(window.App.navigate).toHaveBeenCalledWith(
          "applications", {trigger: true}
        );

      });

    });

    it("should be able to open the main route if exists", function() {
      expect(true).toBeFalsy();
    });

    it("should be disable open if the main route does not exists", function() {
      expect(true).toBeFalsy();
    });

    /*
    describe("edit a foxx", function() {

      var modalButtons, ajaxFunction;

      beforeEach(function() {
        runs(function() {
          // Check if exactly on application is available
          var button = $(".iconSet .icon_arangodb_settings2");
          expect(button.length).toEqual(1);
          button.click();
        });

        waitsFor(function() {
          return $("#modal-dialog").css("display") === "block";
        }, "show the modal dialog");

        runs(function() {
          ajaxFunction = function() {
            return undefined;
          };
          modalButtons = {};
          modalButtons.uninstall = function() {
            return $("#modal-dialog .button-danger");
          };
          modalButtons.cancel = function() {
            return $("#modal-dialog .button-close");
          };
          modalButtons.change = function() {
            return $("#modal-dialog .button-success");
          };
          spyOn($, "ajax").andCallFake(function() {
            ajaxFunction.apply(window, arguments);
          });
        });
      });

       it("should be able to switch mount point", function() {
         var calledBack,
             newMount;

         runs(function() {
           newMount = "/newMount";
           ajaxFunction = function() {
             calledBack = true;
           };
           $("#change-mount-point").val(newMount);
           modalButtons.change().click();
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
             ajaxFunction = function(url, options) {
               expect(options.error).toEqual(jasmine.any(Function));
               options.error(
                 {
                   status: 409,
                   statusText: "Conflict",
                   responseText: resText
                 }
               );
               calledBack = true;
             };
             spyOn(window.modalView, "hide");
             spyOn(arangoHelper, "arangoError");
             $("#change-mount-point").val(newMount);
             modalButtons.change().click();
           });

           waitsFor(function() {
             return calledBack;
           }, 1000);

           runs(function() {
             expect(arangoHelper.arangoError).toHaveBeenCalledWith(resText);
             expect(window.modalView.hide).not.toHaveBeenCalled();
           });

         });

        it("should not trigger a remount if mountpoint has not been changed", function() {
          spyOn(window, "alert");
          modalButtons.change().click();
          expect(window.alert).not.toHaveBeenCalled();
        });

        it("should prevent an illegal mountpoint", function() {
          spyOn(arangoHelper, "arangoError");
          $("#change-mount-point").val("illegal");
          modalButtons.change().click();
          expect(arangoHelper.arangoError).toHaveBeenCalled();
        });
      });
      */

  });

}());

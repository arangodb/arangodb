/*jshint browser: true */
/*jshint unused: false */
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoHelper */

(function() {
  "use strict";

  describe("Applications View", function() {
    var view,
        collection,
        model,
        div,
        specificFetch,
        modalDiv;

    beforeEach(function() {
      specificFetch = undefined;
      modalDiv = document.createElement("div");
      modalDiv.id = "modalPlaceholder";
      document.body.appendChild(modalDiv);
      window.modalView = new window.ModalView();
      div = document.createElement("div");
      div.id = "content";
      document.body.appendChild(div);
      collection = new window.FoxxCollection();
      spyOn(collection, "fetch").andCallFake(function(opts) {
        if (specificFetch) {
          specificFetch(opts);
          return;
        }
        // Fake model already included
        if (collection.length > 0) {
          if (opts.success) {
            opts.success();
          }
          return;
        }
        model = new window.Foxx({
          _key: "1234",
          _id: "aal/1234",
          title: "TestFoxx",
          version: 1.2,
          mount: "/testfoxx",
          description: "Test application",
          manifest: {
            git: "gitpath"
          },
          app: "app:TestFoxx:1.2",
          type: "mount",
          isSystem: false,
          options: {
            collectionPrefix: "testfoxx"
          },
          development: false
        });
        collection.add(model);
        if (opts.success) {
          opts.success();
        }
      });
      view = new window.ApplicationsView({collection: collection});
      view.reload();
    });

    afterEach(function() {
      delete window.modalView;
      document.body.removeChild(div);
      document.body.removeChild(modalDiv);
    });

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
    });
  }());

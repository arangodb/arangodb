/*jshint browser: true */
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global runs, waitsFor, jasmine*/
/*global $, arangoHelper */

(function() {
  "use strict";

  describe("Application Details View", function() {

    var div, view, appDummy, deleteButton, openButton, modalDiv, getDummy, config;

    beforeEach(function() {
      config = {
        data: {}
      };
      getDummy = {
        success: function(callback) {
          callback();
        }
      };
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
      spyOn(appDummy, "getConfiguration").andCallFake(function(callback) {
        callback(config.data);
      });
      view = new window.ApplicationDetailView({
        model: appDummy
      });
      spyOn($, "get").andCallFake(function(url) {
        expect(url).toEqual(view.appUrl());
        return getDummy;
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
      getDummy.success = function(callback) {
        // App url is reachable.
        callback();
      };
      var focusDummy = {
        focus: function() {
          throw new Error();
        }
      };
      view.render();
      expect($("input.open").prop("disabled")).toBeFalsy();
      spyOn(window, "open").andReturn(focusDummy);
      spyOn(focusDummy, "focus");
      $("input.open").click();
      expect(window.open).toHaveBeenCalledWith(
        view.appUrl(),
        appDummy.get('title')
      );
      expect(focusDummy.focus).toHaveBeenCalled();
    });

    it("should be disable open if the main route does not exists", function() {
      getDummy.success = function() {
        // App url is not reachable.
        return;
      };
      view.render();
      expect($("input.open").prop("disabled")).toBeTruthy();
    });

    describe("configure the app", function() {

      var buttonId;

      beforeEach(function() {
        buttonId = "#configure-app";
      });

      describe("if required", function() {

        beforeEach(function() {
          config.data = {
            opt1: {
              type: "string",
              description: "Option description",
              default: "empty",
              current: "empty"
            }
          };
          view.render();
          expect(view._appConfig).toEqual(config.data);
          expect(appDummy.getConfiguration).toHaveBeenCalled();
        });

        it("the button should be enabled", function() {
          expect($(buttonId).hasClass("disabled")).toEqual(false);
        });

        it("should utilize a modal dialog", function() {
          var value;

          runs(function() {
            value = "Option1";
            $(buttonId).click();
          });

          waitsFor(function () {
            return $("#modal-dialog").css("display") === "block";
          }, "The configure App dialog should be shown", 750);

          runs(function() {
            $("#app_config_opt1").val(value);
            spyOn(appDummy, "setConfiguration").andCallFake(function(data, callback) {
              callback({
                error: false
              });
            });
            $("#modalButton1").click();
          });

          waitsFor(function () {
            return $("#modal-dialog").css("display") === "none";
          }, "The configure App dialog should be hidden.", 750);

          runs(function() {
            expect(appDummy.setConfiguration).toHaveBeenCalledWith(
              {
                opt1: value
              },
              jasmine.any(Function)
            );
          });

        });

        describe("with different configuration types", function() {
          var valid1, valid3, valid4, def;

          beforeEach(function() {
            runs(function() {
              valid1 = "Value";
              valid3 = "4.5";
              valid4 = "4";
              def = "default";

              config.data.opt1 = {
                type: "string",
                description: "My String"
              };
              config.data.opt2 = {
                type: "boolean",
                description: "My Bool"
              };
              config.data.opt3 = {
                type: "number",
                description: "My Number"
              };
              config.data.opt4 = {
                type: "integer",
                description: "My Integer"
              };
              config.data.opt5 = {
                type: "string",
                description: "My String",
                default: def
              };
              view.render();
              $(buttonId).click();
            });

            waitsFor(function () {
              return $("#modal-dialog").css("display") === "block";
            }, "The configure App dialog should be shown", 750);
          });

          it("should not allow to configure if the string is empty", function() {
            $("#app_config_opt1").val("");
            $("#app_config_opt3").val(valid3);
            $("#app_config_opt4").val(valid4);
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should not allow to configure if the number is empty", function() {
            $("#app_config_opt1").val(valid1);
            $("#app_config_opt3").val("");
            $("#app_config_opt4").val(valid4);
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should not allow to configure if the number is invalid", function() {
            $("#app_config_opt1").val(valid1);
            $("#app_config_opt3").val("invalid");
            $("#app_config_opt4").val(valid4);
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should not allow to configure if the integer is empty", function() {
            $("#app_config_opt1").val(valid1);
            $("#app_config_opt3").val(valid3);
            $("#app_config_opt4").val("");
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should not allow to configure if the integer is invalid", function() {
            $("#app_config_opt1").val(valid1);
            $("#app_config_opt3").val(valid3);
            $("#app_config_opt4").val("invalid");
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should not allow to configure if the integer is not integer", function() {
            $("#app_config_opt1").val(valid1);
            $("#app_config_opt3").val(valid3);
            $("#app_config_opt4").val("4.5");
            $("#app_config_opt4").keyup();
            expect($("#modalButton1").prop("disabled")).toBeTruthy();
          });

          it("should allow to configure if all values are valid", function() {
            runs(function() {
              $("#app_config_opt1").val(valid1);
              $("#app_config_opt3").val(valid3);
              $("#app_config_opt4").val(valid4);
              $("#app_config_opt4").keyup();
              expect($("#modalButton1").prop("disabled")).toBeFalsy();
              spyOn(appDummy, "setConfiguration").andCallFake(function(data, callback) {
                callback({
                  error: false
                });
              });
              $("#modalButton1").click();
            });

            waitsFor(function () {
              return $("#modal-dialog").css("display") === "none";
            }, "The configure App dialog should be hidden.", 750);

            runs(function() {
              expect(appDummy.setConfiguration).toHaveBeenCalledWith(
                {
                  opt1: valid1,
                  opt2: false,
                  opt3: 4.5,
                  opt4: 4,
                  opt5: def
                },
                jasmine.any(Function)
              );
            });

          });

        });


      });

      describe("if not required", function() {

        beforeEach(function() {
          config.data = {};
          view.render();
          expect(view._appConfig).toEqual(config.data);
          expect(appDummy.getConfiguration).toHaveBeenCalled();
        });

        it("the button should be disabled", function() {
          expect($(buttonId).hasClass("disabled")).toEqual(true);
          spyOn(window.modalView, "show");
          $(buttonId).click();
          expect(window.modalView.show).not.toHaveBeenCalled();
        });

      });
    });

    describe("app modes", function() {
      var switchButton;

      beforeEach(function() {
        switchButton = "#app-switch-mode";
        spyOn(appDummy, "toggleDevelopment").andCallThrough();
        spyOn($, "ajax").andCallFake(function(opts) {
          opts.success();
        });
      });

      it("should switch to production if in development", function() {
        appDummy.set("development", true);
        view.render();
        expect($(switchButton).val()).toEqual("Set Pro");
        $(switchButton).click();
        expect($(switchButton).val()).toEqual("Set Dev");
        expect(appDummy.toggleDevelopment).toHaveBeenCalledWith(false, jasmine.any(Function));
      });

      it("should switch to development if in production", function() {
        appDummy.set("development", false);
        view.render();
        expect($(switchButton).val()).toEqual("Set Dev");
        $(switchButton).click();
        expect($(switchButton).val()).toEqual("Set Pro");
        expect(appDummy.toggleDevelopment).toHaveBeenCalledWith(true, jasmine.any(Function));
      });

    });

    it("should trigger the setup script", function() {
      var button = "#app-setup";
      spyOn(appDummy, "setup").andCallThrough();
      spyOn($, "ajax").andCallFake(function(opts) {
        opts.success();
      });
      view.render();
      expect($(button).text()).toEqual("Setup");
      $(button).click();
      expect(appDummy.setup).toHaveBeenCalledWith(jasmine.any(Function));
    });

    it("should trigger the teardown script", function() {
      var button = "#app-teardown";
      spyOn(appDummy, "teardown").andCallThrough();
      spyOn($, "ajax").andCallFake(function(opts) {
        opts.success();
      });
      view.render();
      expect($(button).text()).toEqual("Teardown");
      $(button).click();
      expect(appDummy.teardown).toHaveBeenCalledWith(jasmine.any(Function));
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

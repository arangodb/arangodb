/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
    "use strict";

    describe("The ClusterDownView", function () {

        var view;

        beforeEach(function () {
            window.App = {
                navigate: function () {
                    throw "This should be a spy";
                },
                planScenario : function () {
                    throw "This should be a spy";
                },
                clusterPlan : {
                    isTestSetup : function () {

                    },
                    destroy :  function () {

                    }
                }
            };
            view = new window.ClusterDownView();
        });

        afterEach(function () {
            delete window.App;
        });

        it("assert the basics", function () {


            expect(view.events).toEqual({
                "click #relaunchCluster"  : "relaunchCluster",
                "click #editPlan"         : "editPlan",
                "click #submitEditPlan"   : "submitEditPlan",
                "click #deletePlan"       : "deletePlan",
                "click #submitDeletePlan" : "submitDeletePlan"
            });
        });

        it("assert render", function () {
            var jquerydummy = {
                html : function () {

                },

                append : function () {

                }

            };
            view.template = {
                render : function () {
                    return "template";
                }
            };
            view.modal = {
                render : function () {
                    return "modal";
                }
            };
            spyOn(window, "$").andReturn(jquerydummy);
            spyOn(jquerydummy, "html");
            spyOn(jquerydummy, "append");
            view.render();

            expect(jquerydummy.html).toHaveBeenCalledWith("template");
            expect(jquerydummy.append).toHaveBeenCalledWith("modal");

        });

        it("assert editPlan", function () {
            var jquerydummy = {
                modal : function () {
                }
            },
            jquerydummy2 = {
                modal : function () {
                }
            };
            spyOn(window, "$").andCallFake(function (a) {
                if (a === '#deletePlanModal') {
                    return jquerydummy;
                }
                if (a === '#editPlanModal') {
                    return jquerydummy2;
                }
            });
            spyOn(jquerydummy, "modal");
            spyOn(jquerydummy2, "modal");
            view.editPlan();

            expect(jquerydummy.modal).toHaveBeenCalledWith("hide");
            expect(jquerydummy2.modal).toHaveBeenCalledWith("show");

        });

        it("assert deletePlan", function () {
            var jquerydummy = {
                modal : function () {
                }
            }, jquerydummy2 = {
                modal : function () {
                }
            };
            spyOn(window, "$").andCallFake(function (a) {
                if (a === '#editPlanModal') {
                    return jquerydummy;
                }
                if (a === '#deletePlanModal') {
                    return jquerydummy2;
                }
            });
            spyOn(jquerydummy, "modal");
            spyOn(jquerydummy2, "modal");
            view.deletePlan();

            expect(jquerydummy.modal).toHaveBeenCalledWith("hide");
            expect(jquerydummy2.modal).toHaveBeenCalledWith("show");

        });

        it("assert relaunchCluster", function () {
            spyOn(window.App, "navigate");
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("cluster/relaunch");
                expect(opt.type).toEqual("GET");
                expect(opt.cache).toEqual(false);
                opt.success();
            });

            view.relaunchCluster();

            expect(window.App.navigate).toHaveBeenCalledWith("showCluster", {trigger: true});

        });


        it("assert submitEditPlan with non testsetup", function () {
            var jquerydummy = {
                modal : function () {
                }
            };
            spyOn(window, "$").andCallFake(function (a) {
                if (a === '#editPlanModal') {
                    return jquerydummy;
                }
            });
            spyOn(window.App, "navigate");
            spyOn(window.App.clusterPlan, "isTestSetup").andReturn(false);
            spyOn(jquerydummy, "modal");
            view.submitEditPlan();

            expect(jquerydummy.modal).toHaveBeenCalledWith("hide");
            expect(window.App.navigate).toHaveBeenCalledWith("planAsymmetrical", {trigger : true});

        });
        it("assert submitEditPlan with testsetup", function () {
            var jquerydummy = {
                modal : function () {
                }
            };
            spyOn(window, "$").andCallFake(function (a) {
                if (a === '#editPlanModal') {
                    return jquerydummy;
                }
            });
            spyOn(window.App, "navigate");
            spyOn(window.App.clusterPlan, "isTestSetup").andReturn(true);
            spyOn(jquerydummy, "modal");
            view.submitEditPlan();

            expect(jquerydummy.modal).toHaveBeenCalledWith("hide");
            expect(window.App.navigate).toHaveBeenCalledWith("planTest", {trigger : true});

        });


        it("assert submitDeletePlan with testsetup", function () {
            var jquerydummy = {
                modal : function () {
                }
            };
            spyOn(window, "$").andCallFake(function (a) {
                if (a === '#deletePlanModal') {
                    return jquerydummy;
                }
            });
            spyOn(window.App, "navigate");
            spyOn(window.App, "planScenario");
            spyOn(window.App.clusterPlan, "destroy");
            spyOn(jquerydummy, "modal");
            view.submitDeletePlan();

            expect(jquerydummy.modal).toHaveBeenCalledWith("hide");
            expect(window.App.planScenario).toHaveBeenCalled();

        });

    });

}());


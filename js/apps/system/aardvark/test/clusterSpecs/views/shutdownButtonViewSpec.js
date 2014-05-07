/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
    "use strict";

    describe("The ServerDashboardView", function () {

        var view;

        beforeEach(function () {
            window.App = {
                navigate: function () {
                    throw "This should be a spy";
                }
            };
            view = new window.ShutdownButtonView();
        });

        afterEach(function () {
            delete window.App;
        });

        it("assert clusterShutdown", function () {
            expect(view.events).toEqual({
                "click #clusterShutdown"  : "clusterShutdown"
            });
            var overviewDummy = {
                stopUpdating : function () {

                }
            };
            spyOn(window.App, "navigate");
            spyOn(overviewDummy, "stopUpdating");

            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("cluster/shutdown");
                expect(opt.cache).toEqual(false);
                expect(opt.type).toEqual("GET");
                opt.success();
            });
            view.overview =overviewDummy;
            view.clusterShutdown();


            expect(overviewDummy.stopUpdating).toHaveBeenCalled();
            expect(window.App.navigate).toHaveBeenCalledWith(
                "handleClusterDown", {trigger: true}
            );
        });


        it("assert render", function () {
            expect(view.render()).toEqual(view);
            expect(view.unrender()).toEqual(undefined);

        });




/*

                render: function () {
                    $(this.el).html(this.template.render({}));
                    return this;
                },

                unrender: function() {
                    $(this.el).html("");
                }
            });
        }());

*/


    });

}());


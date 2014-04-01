/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global describe, beforeEach, afterEach, it, spyOn, expect,
 require, jasmine,  exports, window */
(function () {

    "use strict";

    describe("ClusterStatisticsCollection", function () {

        var col, oldrouter;

        beforeEach(function () {

            oldrouter = window.App;

            window.App = {
                registerForUpdate: function(o) {
                    o.updateUrl();
                },
                getNewRoute: function(last) {
                    if (last === "statistics") {
                        return this.clusterPlan.getCoordinator()
                            + "/_admin/"
                            + last;
                    }
                    return this.clusterPlan.getCoordinator()
                        + "/_admin/aardvark/cluster/"
                        + last;
                },
                clusterPlan : {getCoordinator : function() {return "fritz";}},
                addAuth : {bind: function() {return function() {};}}
            };
            col = new window.ClusterStatisticsCollection();
        });

        it("inititalize", function () {
            expect(col.model).toEqual(window.Statistics);
            expect(col.url).toEqual("fritz/_admin/statistics");
            spyOn(col, "updateUrl").andCallThrough();
            col.initialize();
            expect(col.updateUrl).toHaveBeenCalled();
        });


        it("fetch", function () {
            var m1 = new window.Statistics(),m2 = new window.Statistics();
            spyOn(m1, "fetch");
            spyOn(m2, "fetch");
            col.add(m1);
            col.add(m2);
            col.fetch();
            expect(m1.fetch).toHaveBeenCalledWith({
                async: false,
                beforeSend: jasmine.any(Function)
            });
            expect(m2.fetch).toHaveBeenCalledWith({
                async: false,
                beforeSend: jasmine.any(Function)
            });
        });
        afterEach(function() {
           window.App = oldrouter;
        });
    });
}());

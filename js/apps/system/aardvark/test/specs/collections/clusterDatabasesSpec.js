/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, jasmine, */
/*global spyOn, expect*/
/*global _*/
(function () {

    "use strict";

    describe("Cluster Databases Collection", function () {

        var col, list, db1, db2, db3, oldRouter;

        beforeEach(function () {
            oldRouter = window.App;
            window.App = {
                registerForUpdate: function () {
                },
                addAuth: function () {

                },
                getNewRoute : function(a) {
                    return a;
                }
            };
            list = [];
            db1 = {name: "_system"};
            db2 = {name: "otherDB"};
            db3 = {name: "yaDB"};
            col = new window.ClusterDatabases();
            spyOn(col, "fetch").andCallFake(function () {
                _.each(list, function (s) {
                    col.add(s);
                });
            });
        });

        afterEach(function () {
            window.App = oldRouter;
        });

        describe("list", function () {

            it("should fetch the result sync", function () {
                col.fetch.reset();
                col.getList();
                expect(col.fetch).toHaveBeenCalledWith({
                    async: false,
                    beforeSend: jasmine.any(Function)
                });
            });


            it("should return a list with default ok status", function () {
                list.push(db1);
                list.push(db2);
                list.push(db3);
                var expected = [];
                expected.push({
                    name: db1.name,
                    status: "ok"
                });
                expected.push({
                    name: db2.name,
                    status: "ok"
                });
                expected.push({
                    name: db3.name,
                    status: "ok"
                });
                expect(col.getList()).toEqual(expected);
            });


            it("updateUrl", function () {
                col.updateUrl();
                expect(col.url).toEqual("Databases");
            });
        });

    });

}());

/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, jasmine, */
/*global spyOn, expect*/
/*global _*/
(function () {

    "use strict";

    describe("Cluster Collections Collection", function () {

        var col, list, c1, c2, c3, oldRouter;

        beforeEach(function () {
            oldRouter = window.App;
            window.App = {
                registerForUpdate: function () {
                },
                addAuth: function () {
                },
                getNewRoute: function () {
                }
            };
            list = [];
            c1 = {name: "Documents", id: "123"};
            c2 = {name: "Edges", id: "321"};
            c3 = {name: "_graphs", id: "222"};
            col = new window.ClusterCollections();
            spyOn(col, "fetch").andCallFake(function () {
                _.each(list, function (s) {
                    col.add(s);
                });
            });
        });

        describe("list overview", function () {

            it("should fetch the result sync", function () {
                col.fetch.reset();
                col.getList();
                expect(col.fetch).toHaveBeenCalledWith({
                    async: false,
                    beforeSend: jasmine.any(Function)
                });
            });


            it("url", function () {
                col.dbname = "db";
                expect(col.url()).toEqual("/_admin/aardvark/cluster/"
                    + "db/"
                    + "Collections");
            });


            it("stopUpdating", function () {
                col.timer = 4;
                spyOn(window, "clearTimeout");
                col.stopUpdating();
                expect(col.isUpdating).toEqual(false);
                expect(window.clearTimeout).toHaveBeenCalledWith(4);
            });

            it("startUpdating while already updating", function () {
                col.isUpdating = true;
                spyOn(window, "setInterval");
                col.startUpdating();
                expect(window.setInterval).not.toHaveBeenCalled();
            });

            it("startUpdating while", function () {
                col.isUpdating = false;
                spyOn(window, "setInterval");
                col.startUpdating();
                expect(window.setInterval).toHaveBeenCalled();
            });


            it("should return a list with default ok status", function () {
                list.push(c1);
                list.push(c2);
                list.push(c3);
                var expected = [];
                expected.push({
                    name: c1.name,
                    status: "ok"
                });
                expected.push({
                    name: c2.name,
                    status: "ok"
                });
                expected.push({
                    name: c3.name,
                    status: "ok"
                });
                expect(col.getList()).toEqual(expected);
            });

        });

    });

}());


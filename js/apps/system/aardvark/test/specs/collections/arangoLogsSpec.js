/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
(function () {

    "use strict";

    describe("ArangoLogs", function () {

        var col;

        beforeEach(function () {
            col = new window.ArangoLogs();
            window.arangoLogsStore = new window.ArangoLogs();
            window.logsView = jasmine.createSpyObj(window.LogsView, ['drawTable']);

        });

        it("should parse", function () {
            var result = col.parse({
                level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
            });
            expect(col.url).toEqual('/_admin/log?upto=4&size=10&offset=0');
            expect(col.tables).toEqual(["logTableID", "warnTableID", "infoTableID", "debugTableID", "critTableID"]);
            expect(result).toEqual([
                {level: 3, lid: 1, text: "bla", timestamp: 10, totalAmount: 3},
                {level: 2, lid: 2, text: "blub", timestamp: 11, totalAmount: 3},
                {level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3}
            ]);
        });
        it("should clearLocalStorage", function () {
            spyOn(window.arangoLogsStore, "reset");
            col.clearLocalStorage();
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
        });
        it("should returnElements", function () {
            col.returnElements();
        });
        it("should fillLocalStorage", function () {
            spyOn(window.arangoLogsStore, "reset");
            spyOn(window.arangoLogsStore, "add");

            spyOn($, "getJSON").andCallFake(function (url, callback) {
                expect(url).toEqual("/_admin/log?upto=4&size=10&offset=0");
                callback({
                    level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
                })
            });
            col.fillLocalStorage();
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
            expect(window.arangoLogsStore.add).toHaveBeenCalledWith({level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3});
            expect(window.logsView.drawTable).toHaveBeenCalled();
        });

        it("should fillLocalStorage for critical table", function () {
            spyOn(window.arangoLogsStore, "reset");
            spyOn(window.arangoLogsStore, "add");

            spyOn($, "getJSON").andCallFake(function (url, callback) {
                expect(url).toEqual("/_admin/log?level=1&size=10&offset=0");
                callback({
                    level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
                })
            });
            col.fillLocalStorage("critTableID");
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
            expect(window.arangoLogsStore.add).toHaveBeenCalledWith({level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3});
            expect(window.logsView.drawTable).toHaveBeenCalled();
        });

        it("should fillLocalStorage for warnTable table", function () {
            spyOn(window.arangoLogsStore, "reset");
            spyOn(window.arangoLogsStore, "add");

            spyOn($, "getJSON").andCallFake(function (url, callback) {
                expect(url).toEqual("/_admin/log?level=2&size=10&offset=0");
                callback({
                    level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
                })
            });
            col.fillLocalStorage("warnTableID");
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
            expect(window.arangoLogsStore.add).toHaveBeenCalledWith({level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3});
            expect(window.logsView.drawTable).toHaveBeenCalled();
        });

        it("should fillLocalStorage for infoTable table", function () {
            spyOn(window.arangoLogsStore, "reset");
            spyOn(window.arangoLogsStore, "add");

            spyOn($, "getJSON").andCallFake(function (url, callback) {
                expect(url).toEqual("/_admin/log?level=3&size=10&offset=0");
                callback({
                    level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
                })
            });
            col.fillLocalStorage("infoTableID");
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
            expect(window.arangoLogsStore.add).toHaveBeenCalledWith({level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3});
            expect(window.logsView.drawTable).toHaveBeenCalled();
        });

        it("should fillLocalStorage for debugTable table", function () {
            spyOn(window.arangoLogsStore, "reset");
            spyOn(window.arangoLogsStore, "add");

            spyOn($, "getJSON").andCallFake(function (url, callback) {
                expect(url).toEqual("/_admin/log?level=4&size=10&offset=0");
                callback({
                    level: [3, 2, 3], text: ["bla", "blub", "blaf"], lid: [1, 2, 3], timestamp: [10, 11, 12], totalAmount: 3
                })
            });
            col.fillLocalStorage("debugTableID");
            expect(window.arangoLogsStore.reset).toHaveBeenCalled();
            expect(window.arangoLogsStore.add).toHaveBeenCalledWith({level: 3, lid: 3, text: "blaf", timestamp: 12, totalAmount: 3});
            expect(window.logsView.drawTable).toHaveBeenCalled();
        });
    })
}());

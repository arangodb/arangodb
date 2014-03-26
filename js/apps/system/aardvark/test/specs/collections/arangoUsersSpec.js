/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, window, $, arangoLog */
(function () {

    "use strict";

    describe("ArangoUsers", function () {

        var col;

        beforeEach(function () {
            col = new window.ArangoUsers();
            window.App = jasmine.createSpyObj(window.Router, ["navigate"]);
        });

        it("comparator", function () {
            expect(col.model).toEqual(window.Users);
            expect(col.activeUser).toEqual('');
            expect(col.activeUserSettings).toEqual({
                "query": {},
                "shell": {},
                "testing": true
            });
            expect(col.url).toEqual("/_api/user");
            expect(col.comparator({get: function (val) {
                return "Herbert"
            }})).toEqual("herbert");
        });

        it("login", function () {
            col.login("user", "pw")
            expect(col.activeUser).toEqual("user");
        });
        it("logout", function () {
            spyOn(col, "reset");
            spyOn(window.location, "reload");
            spyOn($, "ajax").andCallFake(function (state, opt) {
                expect(state).toEqual("unauthorized");
                expect(opt).toEqual({async: false});
                return {
                    error: function (callback) {
                        callback();
                    }
                }
            });
            col.logout()
            expect(col.activeUser).toEqual(undefined);
            expect(col.reset).toHaveBeenCalled();
            expect(window.App.navigate).toHaveBeenCalledWith("");
            expect(window.location.reload).toHaveBeenCalled();
        });

        it("setUserSettings", function () {
            col.setUserSettings("bad", true);
            expect(col.activeUserSettings["identifier"]).toEqual(true);
        });

        it("loadUserSettings with success", function () {
            col.activeUserSettings = {
                "query": {},
                "shell": {},
                "testing": true
            };
            col.activeUser = "heinz";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/user/" + encodeURIComponent(col.activeUser));
                expect(opt.type).toEqual("GET");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success({extra: {
                    Heinz: "herbert",
                    Heinz2: "herbert2"
                }});
            });
            col.loadUserSettings();
            expect(col.activeUserSettings).toEqual({
                Heinz: "herbert",
                Heinz2: "herbert2"
            });
        });

        it("loadUserSettings with error", function () {
            col.activeUserSettings = {
                "query": {},
                "shell": {},
                "testing": true
            };
            col.activeUser = "heinz";
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/user/" + encodeURIComponent(col.activeUser));
                expect(opt.type).toEqual("GET");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error({extra: {
                    Heinz: "herbert",
                    Heinz2: "herbert2"
                }});
            });
            col.loadUserSettings();
            expect(col.activeUserSettings).toEqual({
                "query": {},
                "shell": {},
                "testing": true
            });
        });
        it("saveUserSettings with success", function () {
            col.activeUser = "heinz";
            col.activeUserSettings = {a: "B"};
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/user/" + encodeURIComponent(col.activeUser));
                expect(opt.type).toEqual("PUT");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify({ extra: col.activeUserSettings }));
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.success();
            });
            col.saveUserSettings();
            expect($.ajax).toHaveBeenCalled();
        });

        it("saveUserSettings with error", function () {
            col.activeUser = "heinz";
            col.activeUserSettings = {a: "B"};
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual("/_api/user/" + encodeURIComponent(col.activeUser));
                expect(opt.type).toEqual("PUT");
                expect(opt.contentType).toEqual("application/json");
                expect(opt.cache).toEqual(false);
                expect(opt.data).toEqual(JSON.stringify({ extra: col.activeUserSettings }));
                expect(opt.processData).toEqual(false);
                expect(opt.async).toEqual(false);
                opt.error();
            });
            col.saveUserSettings();
            expect($.ajax).toHaveBeenCalled();
        });

        it("parse", function () {
            expect(col.parse({
                result: ["a", "b", "c"]
            })).toEqual(["a", "b", "c"]);
        });

        it("whoAmI without activeUser", function () {
            col.activeUserSettings = {a: "B"};
            spyOn($, "ajax").andCallFake(function (state, opt) {
                expect(state).toEqual("whoAmI");
                expect(opt).toEqual({async: false});
                return {
                    done: function (callback) {
                        callback({name: "heinz"});
                    }
                }
            });
            expect(col.whoAmI()).toEqual("heinz");
        });

        it("whoAmI with activeUser", function () {
            col.activeUser = "heinz";
            expect(col.whoAmI()).toEqual("heinz");
        });


    })
}());


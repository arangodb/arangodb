if (typeof module === "object" && typeof require === "function") {
    var buster = {
        assertions: require("buster-assertions"),
        format: require("../lib/buster-format"),
        util: require("buster-util")
    };
}

(function () {
    function F() {}

    var create = Object.create || function (object) {
        F.prototype = object;
        return new F();
    };

    var assert = buster.assertions.assert;
    var refute = buster.assertions.refute;

    buster.util.testCase("AsciiFormatTest", {
        "should format strings with quotes": function () {
            assert.equals('"A string"', buster.format.ascii("A string"));
        },

        "should format booleans without quotes": function () {
            assert.equals("true", buster.format.ascii(true));
            assert.equals("false", buster.format.ascii(false));
        },

        "should format null and undefined without quotes": function () {
            assert.equals("null", buster.format.ascii(null));
            assert.equals("undefined", buster.format.ascii(undefined));
        },

        "should format numbers without quotes": function () {
            assert.equals("3", buster.format.ascii(3));
            assert.equals("3987.56", buster.format.ascii(3987.56));
            assert.equals("-980", buster.format.ascii(-980.0));
            assert.equals("NaN", buster.format.ascii(NaN));
            assert.equals("Infinity", buster.format.ascii(Infinity));
            assert.equals("-Infinity", buster.format.ascii(-Infinity));
        },

        "should format regexp using toString": function () {
            assert.equals("/[a-zA-Z0-9]+\\.?/", buster.format.ascii(/[a-zA-Z0-9]+\.?/));
        },

        "should format functions with name": function () {
            assert.equals("function doIt() {}", buster.format.ascii(function doIt() {}));
        },

        "should format functions without name": function () {
            assert.equals("function () {}", buster.format.ascii(function () {}));
        },

        "should format functions with display name": function () {
            function doIt() {}
            doIt.displayName = "ohHai";

            assert.equals("function ohHai() {}", buster.format.ascii(doIt));
        },

        "should shorten functions with long bodies": function () {
            function doIt() {
                var i;
                function hey() {}
                for (; i < 10; i++) {
                }
            }

            assert.equals("function doIt() {}", buster.format.ascii(doIt));
        },

        "should format functions with no name or display name": function () {
            function doIt() {}
            doIt.name = "";

            assert.equals("function doIt() {}", buster.format.ascii(doIt));
        },

        "should format arrays": function () {
            function ohNo() { return "Oh yes!"; }

            var array = ["String", 123, /a-z/, null];

            var str = buster.format.ascii(array);
            assert.equals('["String", 123, /a-z/, null]', str);

            str = buster.format.ascii([ohNo, array]);
            assert.equals('[function ohNo() {}, ["String", 123, /a-z/, null]]', str);
        },

        "should not trip on circular arrays": function () {
            var array = ["String", 123, /a-z/];
            array.push(array);

            var str = buster.format.ascii(array);
            assert.equals('["String", 123, /a-z/, [Circular]]', str);
        },

        "should format object": function () {
            var object = {
                id: 42,
                hello: function () {},
                prop: "Some",
                more: "properties",
                please: "Gimme some more",
                "oh hi": 42,
                seriously: "many properties"
            };

            var expected = "{\n  hello: function () {},\n  id: 42,\n  " +
                "more: \"properties\",\n  \"oh hi\": 42,\n  please: " +
                "\"Gimme some more\",\n  prop: \"Some\"," +
                "\n  seriously: \"many properties\"\n}";

            assert.equals(expected, buster.format.ascii(object));
        },

        "should format short object on one line": function () {
            var object = {
                id: 42,
                hello: function () {},
                prop: "Some"
            };

            var expected = "{ hello: function () {}, id: 42, prop: \"Some\" }";
            assert.equals(expected, buster.format.ascii(object));
        },

        "should format object with a non-function toString": function () {
            var object = { toString: 42 };
            assert.equals("{ toString: 42 }", buster.format.ascii(object));
        },

        "should format nested object": function () {
            var object = {
                id: 42,
                hello: function () {},
                prop: "Some",
                obj: {
                    num: 23,
                    string: "Here you go you little mister"
                }
            };

            var expected = "{\n  hello: function () {},\n  id: 42,\n  obj" +
                ": { num: 23, string: \"Here you go you little mister\"" +
                " },\n  prop: \"Some\"\n}";

            assert.equals(expected, buster.format.ascii(object));
        },

        "should include constructor if known and not Object": function () {
            function Person(name) {
                this.name = name;
            }

            var person = new Person("Christian");

            assert.equals("[Person] { name: \"Christian\" }", buster.format.ascii(person));
        },

        "should not include one letter constructors": function () {
            function F(name) {
                this.name = name;
            }

            var person = new F("Christian");

            assert.equals("{ name: \"Christian\" }", buster.format.ascii(person));
        },

        "should include one letter constructors when configured to do so": function () {
            function C(name) {
                this.name = name;
            }

            var person = new C("Christian");
            var formatter = create(buster.format);
            formatter.excludeConstructors = [];

            assert.equals("[C] { name: \"Christian\" }", formatter.ascii(person));
        },

        "should exclude constructors when configured to do so": function () {
            function Person(name) {
                this.name = name;
            }

            var person = new Person("Christian");
            var formatter = create(buster.format);
            formatter.excludeConstructors = ["Person"];

            assert.equals("{ name: \"Christian\" }", formatter.ascii(person));
        },

        "should exclude constructors by pattern when configured to do so": function () {
            function Person(name) {
                this.name = name;
            }

            function Ninja(name) {
                this.name = name;
            }

            function Pervert(name) {
                this.name = name;
            }

            var person = new Person("Christian");
            var ninja = new Ninja("Haruhachi");
            var pervert = new Pervert("Mr. Garrison");
            var formatter = create(buster.format);
            formatter.excludeConstructors = [/^Per/];

            assert.equals("{ name: \"Christian\" }", formatter.ascii(person));
            assert.equals("[Ninja] { name: \"Haruhachi\" }", formatter.ascii(ninja));
            assert.equals("{ name: \"Mr. Garrison\" }", formatter.ascii(pervert));
        },

        "should exclude constructors when run on other objects": function () {
            function Person(name) {
                this.name = name;
            }

            var person = new Person("Christian");
            var formatter = { ascii: buster.format.ascii };
            formatter.excludeConstructors = ["Person"];

            assert.equals("{ name: \"Christian\" }", formatter.ascii(person));
        },

        "should exclude default constructors when run on other objects": function () {
            var person = { name: "Christian" };
            var formatter = { ascii: buster.format.ascii };

            assert.equals("{ name: \"Christian\" }", formatter.ascii(person));
        },

        "should not trip on circular formatting": function () {
            var object = {};
            object.foo = object;

            assert.equals("{ foo: [Circular] }", buster.format.ascii(object));
        },

        "should not trip on indirect circular formatting": function () {
            var object = { someProp: {} };
            object.someProp.foo = object;

            assert.equals("{ someProp: { foo: [Circular] } }", buster.format.ascii(object));
        },

        "should format nested array nicely": function () {
            var object = { people: ["Chris", "August"] };

            assert.equals("{ people: [\"Chris\", \"August\"] }",
                         buster.format.ascii(object));
        },

        "should not rely on object's hasOwnProperty": function () {
            // Create object with no "own" properties to get past
            //  Object.keys test and no .hasOwnProperty() function
            var Obj = function () {};
            Obj.prototype = { hasOwnProperty: undefined };
            var object = new Obj();

            assert.equals("{  }", buster.format.ascii(object));
        },

        "handles cyclic structures": function () {
            var obj = {};
            obj.list1 = [obj];
            obj.list2 = [obj];
            obj.list3 = [{ prop: obj }];

            refute.exception(function () {
                buster.format.ascii(obj);
            });
        }
    });

    buster.util.testCase("UnquotedStringsTest", {
        setUp: function () {
            this.formatter = create(buster.format);
            this.formatter.quoteStrings = false;
        },

        "should not quote strings": function () {
            assert.equals("Hey there", this.formatter.ascii("Hey there"));
        },

        "should quote string properties": function () {
            var obj = { hey: "Mister" };
            assert.equals("{ hey: \"Mister\" }", this.formatter.ascii(obj));
        }
    });

    if (typeof document != "undefined") {
        buster.util.testCase("AsciiFormatDOMElementTest", {
            "should format dom element": function () {
                var element = document.createElement("div");

                assert.equals("<div></div>", buster.format.ascii(element));
            },

            "should format dom element with attributes": function () {
                var element = document.createElement("div");
                element.className = "hey there";
                element.id = "ohyeah";
                var str = buster.format.ascii(element);

                assert(str, /<div (.*)><\/div>/);
                assert(str, /class="hey there"/);
                assert(str, /id="ohyeah"/);
            },

            "should format dom element with content": function () {
                var element = document.createElement("div");
                element.innerHTML = "Oh hi!";

                assert.equals("<div>Oh hi!</div>",
                                     buster.format.ascii(element));
            },

            "should truncate dom element content": function () {
                var element = document.createElement("div");
                element.innerHTML = "Oh hi! I'm Christian, and this is a lot of content";

                assert.equals("<div>Oh hi! I'm Christian[...]</div>",
                             buster.format.ascii(element));
            },

            "should include attributes and truncated content": function () {
                var element = document.createElement("div");
                element.id = "anid";
                element.lang = "en"
                element.innerHTML = "Oh hi! I'm Christian, and this is a lot of content";
                var str = buster.format.ascii(element);

                assert(str, /<div (.*)>Oh hi! I'm Christian\[\.\.\.\]<\/div>/);
                assert(str, /lang="en"/);
                assert(str, /id="anid"/);
            },

            "should format document object as toString": function () {
                var str;
                buster.assertions.refute.exception(function () {
                    str = buster.format.ascii(document);
                });

                assert.equals("[object HTMLDocument]", str);
            },

            "should format window object as toString": function () {
                var str;
                buster.assertions.refute.exception(function () {
                    str = buster.format.ascii(window);
                });

                assert.equals("[object Window]", str);
            }
        });
    }

    if (typeof global != "undefined") {
        buster.util.testCase("AsciiFormatGlobalTest", {
            "should format global object as toString": function () {
                var str;
                buster.assertions.refute.exception(function () {
                    str = buster.format.ascii(global);
                });

                assert.equals("[object global]", str);
            }
        });
    }
}());


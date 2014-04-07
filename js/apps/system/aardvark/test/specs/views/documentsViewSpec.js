/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global $, arangoHelper, jasmine, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
    "use strict";

    describe("The documents view", function () {

        var view, jQueryDummy;

        beforeEach(function () {
            view = new window.DocumentsView();
        });

        it("assert the basics", function () {
            expect(view.collectionID).toEqual(0);
            expect(view.currentPage).toEqual(1);
            expect(view.documentsPerPage).toEqual(10);
            expect(view.totalPages).toEqual(1);
            expect(view.filters).toEqual({ "0": true });
            expect(view.filterId).toEqual(0);
            expect(view.addDocumentSwitch).toEqual(true);
            expect(view.allowUpload).toEqual(false);
            expect(view.collectionContext).toEqual({
                prev: null,
                next: null
            });
            expect(view.alreadyClicked).toEqual(false);
            expect(view.table).toEqual('#documentsTableID');
            expect(view.events).toEqual({
                "click #collectionPrev": "prevCollection",
                "click #collectionNext": "nextCollection",
                "click #filterCollection": "filterCollection",
                "click #indexCollection": "indexCollection",
                "click #importCollection": "importCollection",
                "click #filterSend": "sendFilter",
                "click #addFilterItem": "addFilterItem",
                "click .removeFilterItem": "removeFilterItem",
                "click #confirmCreateEdge": "addEdge",
                "click #documentsTableID tr": "clicked",
                "click #deleteDoc": "remove",
                "click #addDocumentButton": "addDocument",
                "click #documents_first": "firstDocuments",
                "click #documents_last": "lastDocuments",
                "click #documents_prev": "prevDocuments",
                "click #documents_next": "nextDocuments",
                "click #confirmDeleteBtn": "confirmDelete",
                "keyup #createEdge": "listenKey",
                "click .key": "nop",
                "keyup": "returnPressedHandler",
                "keydown .filterValue": "filterValueKeydown",
                "click #importModal": "showImportModal",
                "click #resetView": "resetView",
                "click #confirmDocImport": "startUpload",
                "change #newIndexType": "selectIndexType",
                "click #createIndex": "createIndex",
                "click .deleteIndex": "prepDeleteIndex",
                "click #confirmDeleteIndexBtn": "deleteIndex",
                "click #documentsToolbar ul": "resetIndexForms",
                "click #indexHeader #addIndex": "toggleNewIndexView",
                "click #indexHeader #cancelIndex": "toggleNewIndexView"
            });
        });


        it("showSpinner", function () {
            jQueryDummy = {
                show: function () {
                }
            };
            spyOn(jQueryDummy, "show");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.showSpinner();
            expect(window.$).toHaveBeenCalledWith("#uploadIndicator");
            expect(jQueryDummy.show).toHaveBeenCalled();
        });

        it("hideSpinner", function () {
            jQueryDummy = {
                hide: function () {
                }
            };
            spyOn(jQueryDummy, "hide");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.hideSpinner();
            expect(window.$).toHaveBeenCalledWith("#uploadIndicator");
            expect(jQueryDummy.hide).toHaveBeenCalled();
        });


        it("showImportModal", function () {
            jQueryDummy = {
                modal: function () {
                }
            };
            spyOn(jQueryDummy, "modal");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.showImportModal();
            expect(window.$).toHaveBeenCalledWith("#docImportModal");
            expect(jQueryDummy.modal).toHaveBeenCalledWith("show");
        });


        it("hideImportModal", function () {
            jQueryDummy = {
                modal: function () {
                }
            };
            spyOn(jQueryDummy, "modal");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.hideImportModal();
            expect(window.$).toHaveBeenCalledWith("#docImportModal");
            expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
        });


        it("returnPressedHandler call confirmDelete if keyCode == 13", function () {
            jQueryDummy = {
                attr: function () {
                    return false;
                }
            };
            spyOn(jQueryDummy, "attr").andCallThrough();
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            spyOn(view, "confirmDelete").andCallFake(function () {
            });
            view.returnPressedHandler({keyCode: 13});
            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled");
            expect(view.confirmDelete).toHaveBeenCalled();
        });

        it("returnPressedHandler does not call confirmDelete if keyCode !== 13", function () {
            jQueryDummy = {
                attr: function () {
                    return false;
                }
            };
            spyOn(jQueryDummy, "attr");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            spyOn(view, "confirmDelete").andCallFake(function () {
            });
            view.returnPressedHandler({keyCode: 11});
            expect(jQueryDummy.attr).not.toHaveBeenCalled();
            expect(view.confirmDelete).not.toHaveBeenCalled();
        });

        it("returnPressedHandler does not call confirmDelete if keyCode == 13 but" +
            " confirmButton is disabled", function () {
            jQueryDummy = {
                attr: function () {
                    return true;
                }
            };
            spyOn(jQueryDummy, "attr");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            spyOn(view, "confirmDelete").andCallFake(function () {
            });
            view.returnPressedHandler({keyCode: 13});
            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled");
            expect(view.confirmDelete).not.toHaveBeenCalled();
        });

        it("toggleNewIndexView", function () {
            jQueryDummy = {
                toggle: function () {
                }
            };
            spyOn(jQueryDummy, "toggle");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            spyOn(view, "resetIndexForms").andCallFake(function () {
            });
            view.toggleNewIndexView();
            expect(window.$).toHaveBeenCalledWith("#indexEditView");
            expect(window.$).toHaveBeenCalledWith("#newIndexView");
            expect(jQueryDummy.toggle).toHaveBeenCalledWith("fast");
            expect(view.resetIndexForms).toHaveBeenCalled();
        });


        it("nop", function () {
            var event = {stopPropagation: function () {
            }};
            spyOn(event, "stopPropagation");
            view.nop(event);
            expect(event.stopPropagation).toHaveBeenCalled();
        });

        it("listenKey with keyCode 13", function () {
            spyOn(view, "addEdge");
            view.listenKey({keyCode: 13});
            expect(view.addEdge).toHaveBeenCalled();
        });

        it("listenKey with keyCode != 13", function () {
            spyOn(view, "addEdge");
            view.listenKey({keyCode: 12});
            expect(view.addEdge).not.toHaveBeenCalled();
        });


        it("resetView", function () {
            jQueryDummy = {
                val: function () {
                },
                css: function () {
                }
            };
            spyOn(jQueryDummy, "val");
            spyOn(jQueryDummy, "css");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            spyOn(view, "removeAllFilterItems").andCallFake(function () {
            });
            spyOn(view, "clearTable").andCallFake(function () {
            });
            var arangoDocStoreDummy = {
                getDocuments: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "getDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            view.resetView();
            expect(window.$).toHaveBeenCalledWith("input");
            expect(window.$).toHaveBeenCalledWith("select");
            expect(window.$).toHaveBeenCalledWith("#documents_last");
            expect(window.$).toHaveBeenCalledWith("#documents_first");
            expect(jQueryDummy.val).toHaveBeenCalledWith('');
            expect(jQueryDummy.val).toHaveBeenCalledWith('==');
            expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "visible");
            expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "visible");
            expect(arangoDocStoreDummy.getDocuments).toHaveBeenCalledWith(view.collectionID, 1);
            expect(view.removeAllFilterItems).toHaveBeenCalled();
            expect(view.clearTable).toHaveBeenCalled();
            expect(view.addDocumentSwitch).toEqual(true);
        });


        it("start succesful Upload mit XHR ready state = 4, " +
            "XHR status = 201 and parseable JSON", function () {
            spyOn(view, "showSpinner");
            spyOn(view, "hideSpinner");
            spyOn(view, "hideImportModal");
            spyOn(view, "resetView");
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual('/_api/import?type=auto&collection=' +
                    encodeURIComponent(view.colid) +
                    '&createCollection=false');
                expect(opt.dataType).toEqual("json");
                expect(opt.contentType).toEqual("json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(view.file);
                expect(opt.async).toEqual(false);
                expect(opt.type).toEqual("POST");
                opt.complete({readyState: 4, status: 201, responseText: '{"a" : 1}'});
            });

            view.allowUpload = true;

            view.startUpload();
            expect(view.showSpinner).toHaveBeenCalled();
            expect(view.hideSpinner).toHaveBeenCalled();
            expect(view.hideImportModal).toHaveBeenCalled();
            expect(view.resetView).toHaveBeenCalled();
        });

        it("start succesful Upload mit XHR ready state != 4", function () {
            spyOn(view, "showSpinner");
            spyOn(view, "hideSpinner");
            spyOn(view, "hideImportModal");
            spyOn(view, "resetView");
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual('/_api/import?type=auto&collection=' +
                    encodeURIComponent(view.colid) +
                    '&createCollection=false');
                expect(opt.dataType).toEqual("json");
                expect(opt.contentType).toEqual("json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(view.file);
                expect(opt.async).toEqual(false);
                expect(opt.type).toEqual("POST");
                opt.complete({readyState: 3, status: 201, responseText: '{"a" : 1}'});
            });

            view.allowUpload = true;
            spyOn(arangoHelper, "arangoError");
            view.startUpload();
            expect(view.showSpinner).toHaveBeenCalled();
            expect(view.hideSpinner).toHaveBeenCalled();
            expect(view.hideImportModal).not.toHaveBeenCalled();
            expect(view.resetView).not.toHaveBeenCalled();
            expect(arangoHelper.arangoError).toHaveBeenCalledWith("Upload error");
        });

        it("start succesful Upload mit XHR ready state = 4, " +
            "XHR status = 201 and not parseable JSON", function () {
            spyOn(view, "showSpinner");
            spyOn(view, "hideSpinner");
            spyOn(view, "hideImportModal");
            spyOn(view, "resetView");
            spyOn($, "ajax").andCallFake(function (opt) {
                expect(opt.url).toEqual('/_api/import?type=auto&collection=' +
                    encodeURIComponent(view.colid) +
                    '&createCollection=false');
                expect(opt.dataType).toEqual("json");
                expect(opt.contentType).toEqual("json");
                expect(opt.processData).toEqual(false);
                expect(opt.data).toEqual(view.file);
                expect(opt.async).toEqual(false);
                expect(opt.type).toEqual("POST");
                opt.complete({readyState: 4, status: 201, responseText: "blub"});
            });

            view.allowUpload = true;
            spyOn(arangoHelper, "arangoError");
            view.startUpload();
            expect(view.showSpinner).toHaveBeenCalled();
            expect(view.hideSpinner).toHaveBeenCalled();
            expect(view.hideImportModal).toHaveBeenCalled();
            expect(view.resetView).toHaveBeenCalled();
            expect(arangoHelper.arangoError).toHaveBeenCalledWith(
                'Error: SyntaxError: Unable to parse JSON string'
            );
        });


        it("uploadSetup", function () {
            jQueryDummy = {
                change: function (e) {
                    e({target: {files: ["BLUB"]}});
                }
            };
            spyOn(jQueryDummy, "change").andCallThrough();
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.uploadSetup();
            expect(window.$).toHaveBeenCalledWith("#importDocuments");
            expect(jQueryDummy.change).toHaveBeenCalledWith(jasmine.any(Function));
            expect(view.files).toEqual(["BLUB"]);
            expect(view.file).toEqual("BLUB");
            expect(view.allowUpload).toEqual(true);
        });

        it("buildCollectionLink", function () {
            expect(view.buildCollectionLink({get: function () {
                return "blub";
            }})).toEqual(
                    "collection/" + encodeURIComponent("blub") + '/documents/1'
                );
        });


        it("prevCollection with no collectionContext.prev", function () {
            jQueryDummy = {
                parent: function (e) {
                    return jQueryDummy;
                },
                addClass: function () {

                }

            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "addClass");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.collectionContext = {prev: null};
            view.prevCollection();
            expect(window.$).toHaveBeenCalledWith("#collectionPrev");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(jQueryDummy.addClass).toHaveBeenCalledWith("disabledPag");
        });

        it("prevCollection with collectionContext.prev", function () {
            jQueryDummy = {
                parent: function (e) {
                    return jQueryDummy;
                },
                removeClass: function () {

                }

            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "removeClass");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.collectionContext = {prev: 1};
            spyOn(window.App, "navigate");
            spyOn(view, "buildCollectionLink").andReturn(1);
            view.prevCollection();
            expect(window.$).toHaveBeenCalledWith("#collectionPrev");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
            expect(window.App.navigate).toHaveBeenCalledWith(1, {
                trigger: true
            });
            expect(jQueryDummy.removeClass).toHaveBeenCalledWith("disabledPag");
        });


        it("nextCollection with no collectionContext.next", function () {
            jQueryDummy = {
                parent: function (e) {
                    return jQueryDummy;
                },
                addClass: function () {

                }

            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "addClass");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.collectionContext = {next: null};
            view.nextCollection();
            expect(window.$).toHaveBeenCalledWith("#collectionNext");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(jQueryDummy.addClass).toHaveBeenCalledWith("disabledPag");
        });

        it("nextCollection with collectionContext.next", function () {
            jQueryDummy = {
                parent: function (e) {
                    return jQueryDummy;
                },
                removeClass: function () {

                }

            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "removeClass");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.collectionContext = {next: 1};
            spyOn(window.App, "navigate");
            spyOn(view, "buildCollectionLink").andReturn(1);
            view.nextCollection();
            expect(window.$).toHaveBeenCalledWith("#collectionNext");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
            expect(window.App.navigate).toHaveBeenCalledWith(1, {
                trigger: true
            });
            expect(jQueryDummy.removeClass).toHaveBeenCalledWith("disabledPag");
        });


        it("filterCollection", function () {
            jQueryDummy = {
                removeClass: function () {
                },
                toggleClass: function () {
                },
                slideToggle: function () {
                },
                hide: function () {
                },
                focus: function () {
                }

            };
            spyOn(jQueryDummy, "removeClass");
            spyOn(jQueryDummy, "toggleClass");
            spyOn(jQueryDummy, "slideToggle");
            spyOn(jQueryDummy, "hide");
            spyOn(jQueryDummy, "focus");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.filters = [
                {0: "bla"},
                {1: "blub"}
            ];
            view.filterCollection();
            expect(window.$).toHaveBeenCalledWith("#indexCollection");
            expect(window.$).toHaveBeenCalledWith("#importCollection");
            expect(window.$).toHaveBeenCalledWith("#filterCollection");
            expect(window.$).toHaveBeenCalledWith("#filterHeader");
            expect(window.$).toHaveBeenCalledWith("#importHeader");
            expect(window.$).toHaveBeenCalledWith("#indexHeader");
            expect(window.$).toHaveBeenCalledWith("#attribute_name0");
            // next assertion seems strange but follows the strange implementation
            expect(window.$).not.toHaveBeenCalledWith("#attribute_name1");
            expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
            expect(jQueryDummy.hide).toHaveBeenCalled();
            expect(jQueryDummy.focus).toHaveBeenCalled();
        });

        it("importCollection", function () {
            jQueryDummy = {
                removeClass: function () {
                },
                toggleClass: function () {
                },
                slideToggle: function () {
                },
                hide: function () {
                }

            };
            spyOn(jQueryDummy, "removeClass");
            spyOn(jQueryDummy, "toggleClass");
            spyOn(jQueryDummy, "slideToggle");
            spyOn(jQueryDummy, "hide");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.importCollection();

            expect(window.$).toHaveBeenCalledWith("#filterCollection");
            expect(window.$).toHaveBeenCalledWith("#indexCollection");
            expect(window.$).toHaveBeenCalledWith("#importCollection");
            expect(window.$).toHaveBeenCalledWith("#filterHeader");
            expect(window.$).toHaveBeenCalledWith("#importHeader");
            expect(window.$).toHaveBeenCalledWith("#indexHeader");
            expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
            expect(jQueryDummy.hide).toHaveBeenCalled();
        });

        it("indexCollection", function () {
            jQueryDummy = {
                removeClass: function () {
                },
                toggleClass: function () {
                },
                slideToggle: function () {
                },
                hide: function () {
                },
                show: function () {
                }

            };
            spyOn(jQueryDummy, "removeClass");
            spyOn(jQueryDummy, "toggleClass");
            spyOn(jQueryDummy, "slideToggle");
            spyOn(jQueryDummy, "hide");
            spyOn(jQueryDummy, "show");
            spyOn(window, "$").andReturn(
                jQueryDummy
            );
            view.indexCollection();
            expect(window.$).toHaveBeenCalledWith("#filterCollection");
            expect(window.$).toHaveBeenCalledWith("#importCollection");
            expect(window.$).toHaveBeenCalledWith("#indexCollection");
            expect(window.$).toHaveBeenCalledWith("#newIndexView");
            expect(window.$).toHaveBeenCalledWith("#indexEditView");
            expect(window.$).toHaveBeenCalledWith("#indexHeader");
            expect(window.$).toHaveBeenCalledWith("#importHeader");
            expect(window.$).toHaveBeenCalledWith("#filterHeader");
            expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
            expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
            expect(jQueryDummy.hide).toHaveBeenCalled();
            expect(jQueryDummy.show).toHaveBeenCalled();
        });


        it("getFilterContent", function () {

            jQueryDummy = {
                e: undefined,
                val: function () {
                    var e = jQueryDummy.e;
                    if (e === '#attribute_value0') {
                        return '{"jsonval" : 1}';
                    }
                    if (e === '#attribute_value1') {
                        return "stringval";
                    }
                    if (e === '#attribute_name0') {
                        return 'name0';
                    }
                    if (e === '#attribute_name1') {
                        return 'name1';
                    }
                    if (e === '#operator0') {
                        return 'operator0';
                    }
                    if (e === '#operator1') {
                        return 'operator1';
                    }
                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(window, "$").andCallFake(
                function (e) {
                    jQueryDummy.e = e;
                    return jQueryDummy;
                }
            );
            view.filters = [
                {0: "bla"},
                {1: "blub"}
            ];
            expect(view.getFilterContent()).toEqual([
                [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                { param0: {jsonval: 1}, param1: "stringval"}
            ]);
            expect(window.$).toHaveBeenCalledWith("#attribute_value0");
            expect(window.$).toHaveBeenCalledWith("#attribute_value1");
            expect(window.$).toHaveBeenCalledWith("#attribute_name0");
            expect(window.$).toHaveBeenCalledWith("#attribute_name1");
            expect(window.$).toHaveBeenCalledWith("#operator0");
            expect(window.$).toHaveBeenCalledWith("#operator1");
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
        });


        it("sendFilter", function () {

            jQueryDummy = {
                css: function () {
                }

            };
            spyOn(jQueryDummy, "css");
            spyOn(window, "$").andReturn(jQueryDummy);
            spyOn(view, "getFilterContent").andReturn(
                [
                    [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                    { param0: {jsonval: 1}, param1: "stringval"}
                ]

            );

            var arangoDocStoreDummy = {
                    getFilteredDocuments: function () {
                    }
                },
                documentsViewDummy = {
                    clearTable: function () {
                    }
                };
            spyOn(arangoDocStoreDummy, "getFilteredDocuments");
            spyOn(documentsViewDummy, "clearTable");
            spyOn(window, "arangoDocuments").andReturn(arangoDocStoreDummy);
            spyOn(window, "DocumentsView").andReturn(documentsViewDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            window.documentsView = new window.DocumentsView();


            view.sendFilter();

            expect(view.addDocumentSwitch).toEqual(false);
            expect(documentsViewDummy.clearTable).toHaveBeenCalled();
            expect(arangoDocStoreDummy.getFilteredDocuments).toHaveBeenCalledWith(
                view.colid, 1,
                [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                { param0: {jsonval: 1}, param1: "stringval"}
            );


            expect(window.$).toHaveBeenCalledWith("#documents_last");
            expect(window.$).toHaveBeenCalledWith("#documents_first");
            expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "hidden");
            expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "hidden");
        });

        it("addFilterItem", function () {

            jQueryDummy = {
                append: function () {
                }

            };
            spyOn(jQueryDummy, "append");
            spyOn(window, "$").andReturn(jQueryDummy);

            view.filterId = 1;
            var num = 2;
            view.addFilterItem();
            expect(window.$).toHaveBeenCalledWith("#filterHeader");
            expect(jQueryDummy.append).toHaveBeenCalledWith(
                ' <div class="queryline querylineAdd">' +
                '<input id="attribute_name' + num +
                '" type="text" placeholder="Attribute name">' +
                '<select name="operator" id="operator' +
                num + '" class="filterSelect">' +
                '    <option value="==">==</option>' +
                '    <option value="!=">!=</option>' +
                '    <option value="&lt;">&lt;</option>' +
                '    <option value="&lt;=">&lt;=</option>' +
                '    <option value="&gt;=">&gt;=</option>' +
                '    <option value="&gt;">&gt;</option>' +
                '</select>' +
                '<input id="attribute_value' + num +
                '" type="text" placeholder="Attribute value" ' +
                'class="filterValue">' +
                ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
                '<i class="icon icon-minus arangoicon"></i></a></div>');
            expect(view.filters[num]).toEqual(true);

        });


        it("filterValueKeydown with keyCode == 13", function () {
            spyOn(view, "sendFilter");
            view.filterValueKeydown({keyCode: 13});
            expect(view.sendFilter).toHaveBeenCalled();
        });

        it("filterValueKeydown with keyCode !== 13", function () {
            spyOn(view, "sendFilter");
            view.filterValueKeydown({keyCode: 11});
            expect(view.sendFilter).not.toHaveBeenCalled();
        });


        it("removeFilterItem with keyCode !== 13", function () {
            jQueryDummy = {
                remove: function () {
                }

            };
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andReturn(jQueryDummy);
            view.filters[1] = "bla";

            view.removeFilterItem({currentTarget: {id: "removeFilter1", parentElement: "#blub"}});
            expect(window.$).toHaveBeenCalledWith("#blub");
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(view.filters[1]).toEqual(undefined);
        });

        it("removeAllFilterItems", function () {
            jQueryDummy = {
                children: function () {
                },
                parent: function () {
                }

            };
            spyOn(jQueryDummy, "children").andReturn([1, 2, 3]);
            spyOn(jQueryDummy, "parent").andReturn({remove: function () {
            }});
            spyOn(window, "$").andReturn(jQueryDummy);

            view.removeAllFilterItems();
            expect(window.$).toHaveBeenCalledWith("#filterHeader");
            expect(window.$).toHaveBeenCalledWith("#removeFilter1");
            expect(window.$).toHaveBeenCalledWith("#removeFilter2");
            expect(window.$).toHaveBeenCalledWith("#removeFilter3");
            expect(jQueryDummy.children).toHaveBeenCalled();
            expect(jQueryDummy.parent).toHaveBeenCalled();

            expect(view.filters).toEqual({ "0": true });
            expect(view.filterId).toEqual(0);
        });

        it("addDocument without an error", function () {
            var arangoDocStoreDummy = {
                createTypeDocument: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn("newDoc");
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoHelper, "collectionApiType").andReturn("document");
            window.location.hash = "1/2";
            view.addDocument();

            expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
            expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith("2");
            expect(window.location.hash).toEqual("#collection/" + "newDoc");

        });

        it("addDocument with an edge", function () {
            var arangoDocStoreDummy = {
                createTypeDocument: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn("newDoc");
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            jQueryDummy = {
                modal: function () {
                }

            };
            spyOn(jQueryDummy, "modal");
            spyOn(arangoHelper, "fixTooltips");
            spyOn(window, "$").andReturn(jQueryDummy);

            spyOn(arangoHelper, "collectionApiType").andReturn("edge");
            window.location.hash = "1/2";
            view.addDocument();

            expect(window.$).toHaveBeenCalledWith("#edgeCreateModal");
            expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
            expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(".modalTooltips", "left");
            expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
            expect(arangoDocStoreDummy.createTypeDocument).not.toHaveBeenCalled();
            expect(window.location.hash).toEqual("#1/2");

        });

        it("addDocument with an error", function () {
            var arangoDocStoreDummy = {
                createTypeDocument: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeDocument").andReturn(false);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoHelper, "collectionApiType").andReturn("document");
            spyOn(arangoHelper, "arangoError");
            window.location.hash = "1/2";
            view.addDocument();

            expect(arangoHelper.collectionApiType).toHaveBeenCalledWith("2", true);
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating document failed');
            expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith("2");
            expect(window.location.hash).toEqual("#1/2");

        });


        it("addEdge with missing to ", function () {
            jQueryDummy = {
                e: undefined,
                val: function () {
                    if (jQueryDummy.e === "#new-document-from") {
                        return "";
                    }
                    if (jQueryDummy.e === "#new-document-to") {
                        return "bla";
                    }

                }

            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;

            });

            var arangoDocStoreDummy = {
                createTypeEdge: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeEdge");
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            window.location.hash = "1/2";
            view.addEdge();
            expect(window.$).toHaveBeenCalledWith("#new-document-from");
            expect(window.$).toHaveBeenCalledWith("#new-document-to");
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();

        });


        it("addEdge with missing to ", function () {
            jQueryDummy = {
                e: undefined,
                val: function () {
                    if (jQueryDummy.e === "#new-document-from") {
                        return "bla";
                    }
                    if (jQueryDummy.e === "#new-document-to") {
                        return "";
                    }

                }

            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;

            });

            var arangoDocStoreDummy = {
                createTypeEdge: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeEdge");
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            window.location.hash = "1/2";
            view.addEdge();
            expect(window.$).toHaveBeenCalledWith("#new-document-from");
            expect(window.$).toHaveBeenCalledWith("#new-document-to");
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();

        });


        it("addEdge with success", function () {
            jQueryDummy = {
                e: undefined,
                val: function () {
                    if (jQueryDummy.e === "#new-document-from") {
                        return "bla";
                    }
                    if (jQueryDummy.e === "#new-document-to") {
                        return "blub";
                    }
                },
                modal: function () {

                }


            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "modal");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;

            });

            var arangoDocStoreDummy = {
                createTypeEdge: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeEdge").andReturn("newEdge");
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            window.location.hash = "1/2";
            view.addEdge();
            expect(window.$).toHaveBeenCalledWith("#new-document-from");
            expect(window.$).toHaveBeenCalledWith("#new-document-to");
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
            expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith("2", "bla", "blub");
            expect(window.location.hash).toEqual("#collection/newEdge");

        });

        it("addEdge with error", function () {
            jQueryDummy = {
                e: undefined,
                val: function () {
                    if (jQueryDummy.e === "#new-document-from") {
                        return "bla";
                    }
                    if (jQueryDummy.e === "#new-document-to") {
                        return "blub";
                    }
                },
                modal: function () {

                }


            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "modal");
            spyOn(arangoHelper, "arangoError");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;

            });

            var arangoDocStoreDummy = {
                createTypeEdge: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "createTypeEdge").andReturn(false);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            window.location.hash = "1/2";
            view.addEdge();
            expect(window.$).toHaveBeenCalledWith("#new-document-from");
            expect(window.$).toHaveBeenCalledWith("#new-document-to");
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating edge failed');
            expect(jQueryDummy.modal).not.toHaveBeenCalled();
            expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith("2", "bla", "blub");
            expect(window.location.hash).toEqual("#1/2");

        });

        it("first-last-next-prev document", function () {
            var arangoDocumentsStoreDummy = {
                getFirstDocuments: function () {
                },
                getLastDocuments: function () {
                },
                getPrevDocuments: function () {
                },
                getNextDocuments: function () {
                }
            };
            spyOn(arangoDocumentsStoreDummy, "getFirstDocuments");
            spyOn(arangoDocumentsStoreDummy, "getLastDocuments");
            spyOn(arangoDocumentsStoreDummy, "getPrevDocuments");
            spyOn(arangoDocumentsStoreDummy, "getNextDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocumentsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();

            view.firstDocuments();
            expect(arangoDocumentsStoreDummy.getFirstDocuments).toHaveBeenCalled();
            view.lastDocuments();
            expect(arangoDocumentsStoreDummy.getLastDocuments).toHaveBeenCalled();
            view.prevDocuments();
            expect(arangoDocumentsStoreDummy.getPrevDocuments).toHaveBeenCalled();
            view.nextDocuments();
            expect(arangoDocumentsStoreDummy.getNextDocuments).toHaveBeenCalled();


        });


        it("remove", function () {
            jQueryDummy = {
                e: undefined,
                prev: function () {
                    return jQueryDummy;
                },
                attr: function () {

                },
                modal: function () {

                }


            };
            spyOn(jQueryDummy, "prev").andCallThrough();
            spyOn(jQueryDummy, "attr");
            spyOn(jQueryDummy, "modal");
            spyOn(window, "$").andReturn(jQueryDummy);

            view.remove({currentTarget: {parentElement: "#thiselement"}});

            expect(window.$).toHaveBeenCalledWith("#thiselement");
            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(window.$).toHaveBeenCalledWith("#docDeleteModal");
            expect(jQueryDummy.prev).toHaveBeenCalled();
            expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", false);
            expect(jQueryDummy.modal).toHaveBeenCalledWith("show");
            expect(view.alreadyClicked).toEqual(true);
            expect(view.idelement).toEqual(jQueryDummy);

        });


        it("confirmDelete with check != source", function () {
            jQueryDummy = {
                attr: function () {

                }
            };
            spyOn(jQueryDummy, "attr");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/4";
            spyOn(view, "reallyDelete");
            view.confirmDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", true);
            expect(view.reallyDelete).toHaveBeenCalled();
        });


        it("confirmDelete with check = source", function () {
            jQueryDummy = {
                attr: function () {

                }
            };
            spyOn(jQueryDummy, "attr");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/source";
            spyOn(view, "reallyDelete");
            view.confirmDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(jQueryDummy.attr).toHaveBeenCalledWith("disabled", true);
            expect(view.reallyDelete).not.toHaveBeenCalled();
        });


        it("reallyDelete a document with error", function () {
            jQueryDummy = {
                closest: function () {
                    return jQueryDummy;
                },
                get: function () {

                },
                next: function () {
                    return jQueryDummy;
                },
                text: function () {

                }
            };
            spyOn(jQueryDummy, "closest").andCallThrough();
            spyOn(jQueryDummy, "get");
            spyOn(jQueryDummy, "next").andCallThrough();
            spyOn(jQueryDummy, "text").andReturn("4");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/source";

            view.type = "document";
            view.colid = "collection";

            spyOn(arangoHelper, "arangoError");
            var arangoDocStoreDummy = {
                deleteDocument: function () {
                }
            }, arangoDocsStoreDummy = {
                getDocuments: function () {
                }
            };

            spyOn(arangoDocStoreDummy, "deleteDocument").andReturn(false);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoDocsStoreDummy, "getDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            view.target = "#confirmDeleteBtn";

            view.reallyDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Doc error');
            expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith("collection", "4");
        });


        it("reallyDelete a edge with error", function () {
            jQueryDummy = {
                closest: function () {
                    return jQueryDummy;
                },
                get: function () {

                },
                next: function () {
                    return jQueryDummy;
                },
                text: function () {

                }
            };
            spyOn(jQueryDummy, "closest").andCallThrough();
            spyOn(jQueryDummy, "get");
            spyOn(jQueryDummy, "next").andCallThrough();
            spyOn(jQueryDummy, "text").andReturn("4");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/source";

            view.type = "edge";
            view.colid = "collection";

            spyOn(arangoHelper, "arangoError");
            var arangoDocStoreDummy = {
                deleteEdge: function () {
                }
            }, arangoDocsStoreDummy = {
                getDocuments: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "deleteEdge").andReturn(false);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoDocsStoreDummy, "getDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            view.target = "#confirmDeleteBtn";

            view.reallyDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(arangoHelper.arangoError).toHaveBeenCalledWith('Edge error');
            expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
            expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith("collection", "4");
        });


        it("reallyDelete a document with no error", function () {
            jQueryDummy = {
                closest: function () {
                    return jQueryDummy;
                },
                get: function () {

                },
                next: function () {
                    return jQueryDummy;
                },
                text: function () {

                },
                dataTable: function () {
                    return jQueryDummy;
                },
                fnDeleteRow: function () {

                },
                fnGetPosition: function () {

                },
                fnClearTable: function () {

                },
                modal: function () {

                }

            };
            spyOn(jQueryDummy, "closest").andCallThrough();
            spyOn(jQueryDummy, "get").andReturn(1);
            spyOn(jQueryDummy, "fnClearTable");
            spyOn(jQueryDummy, "modal");
            spyOn(jQueryDummy, "fnGetPosition").andReturn(3);
            spyOn(jQueryDummy, "next").andCallThrough();
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnDeleteRow").andCallThrough();
            spyOn(jQueryDummy, "text").andReturn("4");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/source";

            view.type = "document";
            view.colid = "collection";

            spyOn(arangoHelper, "arangoError");
            var arangoDocStoreDummy = {
                deleteDocument: function () {
                }
            }, arangoDocsStoreDummy = {
                getDocuments: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "deleteDocument").andReturn(true);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoDocsStoreDummy, "getDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            view.target = "#confirmDeleteBtn";

            view.reallyDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");
            expect(window.$).toHaveBeenCalledWith("#documentsTableID");
            expect(window.$).toHaveBeenCalledWith("#docDeleteModal");

            expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
            expect(jQueryDummy.dataTable).toHaveBeenCalled();
            expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
            expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
            expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
            expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();


            expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
            expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Doc error');
            expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith("collection", "4");
        });


        it("reallyDelete a edge with no error", function () {
            jQueryDummy = {
                closest: function () {
                    return jQueryDummy;
                },
                get: function () {

                },
                next: function () {
                    return jQueryDummy;
                },
                text: function () {

                },
                dataTable: function () {
                    return jQueryDummy;
                },
                fnDeleteRow: function () {

                },
                fnGetPosition: function () {

                },
                fnClearTable: function () {

                },
                modal: function () {

                }

            };
            spyOn(jQueryDummy, "closest").andCallThrough();
            spyOn(jQueryDummy, "get").andReturn(1);
            spyOn(jQueryDummy, "fnClearTable");
            spyOn(jQueryDummy, "modal");
            spyOn(jQueryDummy, "fnGetPosition").andReturn(3);
            spyOn(jQueryDummy, "next").andCallThrough();
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnDeleteRow").andCallThrough();
            spyOn(jQueryDummy, "text").andReturn("4");
            spyOn(window, "$").andReturn(jQueryDummy);
            window.location.hash = "1/2/3/source";

            view.type = "edge";
            view.colid = "collection";

            spyOn(arangoHelper, "arangoError");
            var arangoDocStoreDummy = {
                deleteEdge: function () {
                }
            }, arangoDocsStoreDummy = {
                getDocuments: function () {
                }
            };
            spyOn(arangoDocStoreDummy, "deleteEdge").andReturn(true);
            spyOn(window, "arangoDocument").andReturn(arangoDocStoreDummy);
            window.arangoDocumentStore = new window.arangoDocument();

            spyOn(arangoDocsStoreDummy, "getDocuments");
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();
            view.target = "#confirmDeleteBtn";

            view.reallyDelete();

            expect(window.$).toHaveBeenCalledWith("#confirmDeleteBtn");


            expect(window.$).toHaveBeenCalledWith("#documentsTableID");
            expect(window.$).toHaveBeenCalledWith("#docDeleteModal");

            expect(jQueryDummy.modal).toHaveBeenCalledWith("hide");
            expect(jQueryDummy.dataTable).toHaveBeenCalled();
            expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
            expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
            expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
            expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();

            expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Edge error');
            expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
            expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith("collection", "4");
        });


        it("clicked when alreadyClicked", function () {
            view.alreadyClicked = true;
            expect(view.clicked("anything")).toEqual(0);
        });

        it("clicked when clicked event has no target", function () {
            jQueryDummy = {
                dataTable: function () {
                    return jQueryDummy;
                },
                fnGetPosition: function () {

                }

            };
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnGetPosition").andReturn(null);
            spyOn(window, "$").andReturn(jQueryDummy);
            view.alreadyClicked = false;
            expect(view.clicked("anything")).toEqual(undefined);
        });

        it("clicked when clicked event has target but no valid checkData", function () {
            jQueryDummy = {
                dataTable: function () {
                    return jQueryDummy;
                },
                fnGetPosition: function () {

                },
                fnGetData: function () {

                }
            };
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnGetPosition").andReturn(1);
            spyOn(jQueryDummy, "fnGetData").andReturn([0, ""]);
            spyOn(window, "$").andReturn(jQueryDummy);
            spyOn(view, "addDocument");
            view.alreadyClicked = false;
            expect(view.clicked("anything")).toEqual(undefined);
            expect(view.addDocument).toHaveBeenCalled();
        });

        it("clicked when clicked event has target and valid checkData", function () {
            jQueryDummy = {
                dataTable: function () {
                    return jQueryDummy;
                },
                fnGetPosition: function () {

                },
                fnGetData: function () {

                },
                next: function () {
                    return jQueryDummy;
                },
                text: function () {

                }

            };
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "next").andCallThrough();
            spyOn(jQueryDummy, "text").andReturn(12);
            spyOn(jQueryDummy, "fnGetPosition").andReturn(1);
            spyOn(jQueryDummy, "fnGetData").andReturn([0, "true"]);
            spyOn(window, "$").andReturn(jQueryDummy);
            spyOn(view, "addDocument");
            view.alreadyClicked = false;
            view.colid = "coll";
            expect(view.clicked({currentTarget: {firstChild: "blub"}})).toEqual(undefined);
            expect(view.addDocument).not.toHaveBeenCalled();
            expect(window.$).toHaveBeenCalledWith("blub");
            expect(window.location.hash).toEqual("#collection/coll/12");
        });

        it("initTable", function () {
            jQueryDummy = {
                dataTable: function () {
                }
            };
            spyOn(jQueryDummy, "dataTable");
            spyOn(window, "$").andReturn(jQueryDummy);
            view.initTable();
            expect(jQueryDummy.dataTable).toHaveBeenCalledWith({
                "bSortClasses": false,
                "bFilter": false,
                "bPaginate": false,
                "bRetrieve": true,
                "bSortable": false,
                "bSort": false,
                "bLengthChange": false,
                "bAutoWidth": false,
                "iDisplayLength": -1,
                "bJQueryUI": false,
                "aoColumns": [
                    { "sClass": "docsFirstCol", "bSortable": false},
                    { "sClass": "docsSecCol", "bSortable": false},
                    { "bSortable": false, "sClass": "docsThirdCol"}
                ],
                "oLanguage": { "sEmptyTable": "Loading..."}
            });
        });

        it("clearTable", function () {
            jQueryDummy = {
                dataTable: function () {
                    return jQueryDummy;
                },
                fnClearTable: function () {
                }
            };
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnClearTable");
            spyOn(window, "$").andReturn(jQueryDummy);
            view.table = "blub";
            view.clearTable();
            expect(window.$).toHaveBeenCalledWith("blub");
            expect(jQueryDummy.fnClearTable).toHaveBeenCalled();
        });


        it("drawTable with empty collection", function () {
            jQueryDummy = {
                text: function () {

                }
            };
            spyOn(jQueryDummy, "text");
            spyOn(window, "$").andReturn(jQueryDummy);

            var arangoDocsStoreDummy = {
                models: [],
                totalPages: 1,
                currentPage: 1,
                documentsCount: 0

            };
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            window.arangoDocumentsStore = new window.arangoDocuments();

            view.drawTable();
            expect(view.totalPages).toEqual(1);
            expect(view.currentPage).toEqual(1);
            expect(view.documentsCount).toEqual(0);
            expect(window.$).toHaveBeenCalledWith(".dataTables_empty");
            expect(jQueryDummy.text).toHaveBeenCalledWith('No documents');
        });


        it("drawTable with not empty collection", function () {
            jQueryDummy = {
                dataTable: function () {
                    return jQueryDummy;
                },
                fnAddData: function () {
                },
                snippet: function () {
                }

            };
            spyOn(jQueryDummy, "dataTable").andCallThrough();
            spyOn(jQueryDummy, "fnAddData");
            spyOn(jQueryDummy, "snippet");
            spyOn(window, "$").andReturn(jQueryDummy);
            $.each = function (list, callback) {
                var callBackWraper = function (a, b) {
                    callback(b, a);
                };
                list.forEach(callBackWraper);
            };

            var arangoDocsStoreDummy = {
                models: [
                    new window.arangoDocumentModel(
                        {content: [
                            {_id: 1},
                            {_rev: 1},
                            {_key: 1} ,
                            {bla: 1}
                        ]}
                    ),
                    new window.arangoDocumentModel(
                        {content: [
                            {_id: 1},
                            {_rev: 1},
                            {_key: 1} ,
                            {bla: 1}
                        ]}
                    ),
                    new window.arangoDocumentModel(
                        {content: [
                            {_id: 1},
                            {_rev: 1},
                            {_key: 1} ,
                            {bla: 1}
                        ]}
                    )
                ],
                totalPages: 1,
                currentPage: 2,
                documentsCount: 3

            };
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            spyOn(arangoHelper, "fixTooltips");
            window.arangoDocumentsStore = new window.arangoDocuments();

            view.drawTable();
            expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
                ".icon_arangodb, .arangoicon", "top"
            );
            expect(view.totalPages).toEqual(1);
            expect(view.currentPage).toEqual(2);
            expect(view.documentsCount).toEqual(3);
            expect(window.$).toHaveBeenCalledWith(".prettify");
            expect(jQueryDummy.dataTable).toHaveBeenCalled();
            expect(jQueryDummy.snippet).toHaveBeenCalledWith("javascript", {
                style: "nedit",
                menu: false,
                startText: false,
                transparent: true,
                showNum: false
            });
            expect(jQueryDummy.fnAddData).toHaveBeenCalled();
        });

        describe("render", function () {
            var jQueryDummy, arangoCollectionsDummy;
            beforeEach(function () {
                jQueryDummy = {
                    parent: function () {
                        return jQueryDummy;
                    },
                    addClass: function () {
                    },
                    tooltip: function () {

                    },
                    html: function () {

                    }
                };
                spyOn(jQueryDummy, "parent").andCallThrough();
                spyOn(jQueryDummy, "addClass");
                spyOn(jQueryDummy, "tooltip");
                spyOn(jQueryDummy, "html");
                spyOn(window, "$").andReturn(jQueryDummy);
                arangoCollectionsDummy = {
                    getPosition: function () {
                    }

                };
                spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
                window.arangoCollectionsStore = new window.arangoCollections();
                spyOn(view, "getIndex");
                spyOn(view, "initTable");
                spyOn(view, "breadcrumb");
                spyOn(view, "uploadSetup");
                spyOn(arangoHelper, "fixTooltips");
                view.el = 1;
                view.colid = "1";
                view.template = {
                    render: function () {
                        return "tmp";
                    }
                };

            });

            it("render with no prev and no next page", function () {
                spyOn(arangoCollectionsDummy, "getPosition").andReturn({prev: null, next: null});

                expect(view.render()).toEqual(view);

                expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith("1");
                expect(view.getIndex).toHaveBeenCalled();
                expect(view.initTable).toHaveBeenCalled();
                expect(view.breadcrumb).toHaveBeenCalled();
                expect(view.uploadSetup).toHaveBeenCalled();
                expect(jQueryDummy.parent).toHaveBeenCalled();
                expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
                expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
                expect(jQueryDummy.tooltip).toHaveBeenCalled();
                expect(jQueryDummy.tooltip).toHaveBeenCalledWith({placement: "left"});
                expect(window.$).toHaveBeenCalledWith("#collectionPrev");
                expect(window.$).toHaveBeenCalledWith("#collectionNext");
                expect(window.$).toHaveBeenCalledWith("[data-toggle=tooltip]");
                expect(window.$).toHaveBeenCalledWith('.modalImportTooltips');
                expect(window.$).toHaveBeenCalledWith(1);

                expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
                    ".icon_arangodb, .arangoicon", "top"
                );
            });

            it("render with no prev but next page", function () {
                spyOn(arangoCollectionsDummy, "getPosition").andReturn({prev: null, next: 1});

                expect(view.render()).toEqual(view);

                expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith("1");
                expect(view.getIndex).toHaveBeenCalled();
                expect(view.initTable).toHaveBeenCalled();
                expect(view.breadcrumb).toHaveBeenCalled();
                expect(view.uploadSetup).toHaveBeenCalled();
                expect(jQueryDummy.parent).toHaveBeenCalled();
                expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
                expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
                expect(jQueryDummy.tooltip).toHaveBeenCalled();
                expect(jQueryDummy.tooltip).toHaveBeenCalledWith({placement: "left"});
                expect(window.$).toHaveBeenCalledWith("#collectionPrev");
                expect(window.$).not.toHaveBeenCalledWith("#collectionNext");
                expect(window.$).toHaveBeenCalledWith("[data-toggle=tooltip]");
                expect(window.$).toHaveBeenCalledWith('.modalImportTooltips');
                expect(window.$).toHaveBeenCalledWith(1);

                expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
                    ".icon_arangodb, .arangoicon", "top"
                );
            });

            it("render with  prev but no next page", function () {
                spyOn(arangoCollectionsDummy, "getPosition").andReturn({prev: 1, next: null});

                expect(view.render()).toEqual(view);

                expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith("1");
                expect(view.getIndex).toHaveBeenCalled();
                expect(view.initTable).toHaveBeenCalled();
                expect(view.breadcrumb).toHaveBeenCalled();
                expect(view.uploadSetup).toHaveBeenCalled();
                expect(jQueryDummy.parent).toHaveBeenCalled();
                expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
                expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
                expect(jQueryDummy.tooltip).toHaveBeenCalled();
                expect(jQueryDummy.tooltip).toHaveBeenCalledWith({placement: "left"});
                expect(window.$).not.toHaveBeenCalledWith("#collectionPrev");
                expect(window.$).toHaveBeenCalledWith("#collectionNext");
                expect(window.$).toHaveBeenCalledWith("[data-toggle=tooltip]");
                expect(window.$).toHaveBeenCalledWith('.modalImportTooltips');
                expect(window.$).toHaveBeenCalledWith(1);

                expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
                    ".icon_arangodb, .arangoicon", "top"
                );
            });

        });

        it("showLoadingState", function () {
            jQueryDummy = {
                text: function () {
                }
            };
            spyOn(jQueryDummy, "text");
            spyOn(window, "$").andReturn(jQueryDummy);
            view.showLoadingState();
            expect(jQueryDummy.text).toHaveBeenCalledWith('Loading...');
            expect(window.$).toHaveBeenCalledWith('.dataTables_empty');

        });

        it("renderPagination with filter check and totalDocs > 0 ", function () {
            jQueryDummy = {
                html: function () {
                },
                css: function () {
                },
                pagination: function (o) {
                    o.click(5);
                },
                prepend: function () {
                },
                append: function () {
                },
                length: 10

            };
            spyOn(jQueryDummy, "html");
            spyOn(jQueryDummy, "css");
            spyOn(jQueryDummy, "prepend");
            spyOn(jQueryDummy, "append");
            spyOn(jQueryDummy, "pagination").andCallThrough();
            spyOn(window, "$").andReturn(jQueryDummy);


            var arangoDocsStoreDummy = {
                currentFilterPage: 2,
                getFilteredDocuments: function () {

                }
            };
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            spyOn(arangoDocsStoreDummy, "getFilteredDocuments");
            spyOn(arangoHelper, "fixTooltips");
            window.arangoDocumentsStore = new window.arangoDocuments();


            spyOn(view, "getFilterContent").andReturn(
                [
                    [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                    { param0: {jsonval: 1}, param1: "stringval"}
                ]

            );
            spyOn(view, "clearTable");
            window.documentsView = view;
            view.colid = 1;
            view.documentsCount = 11;
            view.renderPagination(3, true);


            expect(jQueryDummy.html).toHaveBeenCalledWith('');
            expect(jQueryDummy.html).toHaveBeenCalledWith("Total: 11 documents");
            expect(jQueryDummy.prepend).toHaveBeenCalledWith('<ul class="pre-pagi">' +
                '<li><a id="documents_first" class="pagination-button">' +
                '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>');
            expect(jQueryDummy.append).toHaveBeenCalledWith('<ul class="las-pagi">' +
                '<li><a id="documents_last" class="pagination-button">' +
                '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>');
            expect(jQueryDummy.css).toHaveBeenCalledWith("visibility", "hidden");
            expect(jQueryDummy.pagination).toHaveBeenCalledWith(
                {
                    left: 2,
                    right: 2,
                    page: 5,
                    lastPage: 3,
                    click: jasmine.any(Function)
                }
            );
            expect(arangoDocsStoreDummy.getFilteredDocuments).toHaveBeenCalledWith(
                1, 5, [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                { param0: {jsonval: 1}, param1: "stringval"}
            );
            expect(window.$).toHaveBeenCalledWith('#documentsToolbarF');
            expect(window.$).toHaveBeenCalledWith('#documents_last');
            expect(window.$).toHaveBeenCalledWith('#documents_first');
            expect(window.$).toHaveBeenCalledWith('#totalDocuments');

        });

        it("renderPagination with no filter check and totalDocs = 0 ", function () {
            jQueryDummy = {
                html: function () {
                },
                css: function () {
                },
                pagination: function (o) {
                    o.click(5);
                },
                prepend: function () {
                },
                append: function () {
                },
                length: 0

            };
            spyOn(jQueryDummy, "html");
            spyOn(jQueryDummy, "css");
            spyOn(jQueryDummy, "prepend");
            spyOn(jQueryDummy, "append");
            spyOn(jQueryDummy, "pagination").andCallThrough();
            spyOn(window, "$").andReturn(jQueryDummy);


            var arangoDocsStoreDummy = {
                currentFilterPage: 2,
                getFilteredDocuments: function () {

                }
            };
            spyOn(window, "arangoDocuments").andReturn(arangoDocsStoreDummy);
            spyOn(arangoDocsStoreDummy, "getFilteredDocuments");
            spyOn(arangoHelper, "fixTooltips");
            window.arangoDocumentsStore = new window.arangoDocuments();


            spyOn(view, "getFilterContent").andReturn(
                [
                    [" u.`name0`operator0@param0", " u.`name1`operator1@param1"],
                    { param0: {jsonval: 1}, param1: "stringval"}
                ]

            );
            spyOn(view, "clearTable");
            window.documentsView = view;
            view.colid = 1;
            view.documentsCount = 0;
            view.pageid = "1";
            view.renderPagination(3, false);


            expect(jQueryDummy.html).toHaveBeenCalledWith('');
            expect(view.clearTable).not.toHaveBeenCalled();
            expect(jQueryDummy.html).not.toHaveBeenCalledWith("Total: 0 documents");
            expect(jQueryDummy.prepend).toHaveBeenCalledWith('<ul class="pre-pagi">' +
                '<li><a id="documents_first" class="pagination-button">' +
                '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>');
            expect(jQueryDummy.append).toHaveBeenCalledWith('<ul class="las-pagi">' +
                '<li><a id="documents_last" class="pagination-button">' +
                '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>');
            expect(jQueryDummy.css).not.toHaveBeenCalledWith("visibility", "hidden");
            expect(jQueryDummy.pagination).toHaveBeenCalledWith(
                {
                    left: 2,
                    right: 2,
                    page: 5,
                    lastPage: 3,
                    click: jasmine.any(Function)
                }
            );
            expect(arangoDocsStoreDummy.getFilteredDocuments).not.toHaveBeenCalled();
            expect(window.$).toHaveBeenCalledWith('#documentsToolbarF');
            expect(window.$).not.toHaveBeenCalledWith('#documents_last');
            expect(window.$).not.toHaveBeenCalledWith('#documents_first');
            expect(window.$).toHaveBeenCalledWith('#totalDocuments');
            expect(window.location.hash).toEqual('#collection/1/documents/5');

        });


        it("breadcrumb", function () {
            jQueryDummy = {
                append: function () {
                }
            };
            spyOn(jQueryDummy, "append");
            spyOn(window, "$").andReturn(jQueryDummy);

            window.location.hash = "1/2";
            view.breadcrumb();
            expect(view.collectionName).toEqual("2");
            expect(jQueryDummy.append).toHaveBeenCalledWith('<div class="breadcrumb">' +
                '<a class="activeBread" href="#collections">Collections</a>' +
                '  &gt;  ' +
                '<a class="disabledBread">2</a>' +
                '</div>');
            expect(window.$).toHaveBeenCalledWith('#transparentHeader');
        });

        it("cutByResolution with small string", function () {

            expect(view.cutByResolution("blub")).toEqual(view.escaped("blub"));

        });

        it("cutByResolution with string longer than 1024", function () {
            var string = "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890",

             resultString = "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "12345678901234567890123456789012345678901234567890" +
                "123456789012345678901234...";

            expect(view.cutByResolution(string)).toEqual(view.escaped(resultString));

        });

        it("escaped", function () {
            expect(view.escaped('&<>"\'')).toEqual("&amp;&lt;&gt;&quot;&#39;");
        });


        it("resetIndexForms", function () {
            jQueryDummy = {
                val: function () {
                    return jQueryDummy;
                },
                prop: function () {

                }

            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "prop");
            spyOn(window, "$").andReturn(jQueryDummy);

            spyOn(view, "selectIndexType");
            view.resetIndexForms();
            expect(view.selectIndexType).toHaveBeenCalled();
            expect(jQueryDummy.prop).toHaveBeenCalledWith("checked", false);
            expect(jQueryDummy.prop).toHaveBeenCalledWith('selected', true);
            expect(jQueryDummy.val).toHaveBeenCalledWith('');
            expect(jQueryDummy.val).toHaveBeenCalledWith('Cap');
            expect(window.$).toHaveBeenCalledWith('#indexHeader input');
            expect(window.$).toHaveBeenCalledWith('#newIndexType');

        });

        it("stringToArray", function () {
            expect(view.stringToArray("1,2,3,4,5,")).toEqual(["1", "2", "3", "4", "5"]);
        });


        it("createCap Index", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Cap";
                    }
                    if (jQueryDummy.e === "#newCapSize") {
                        return "1024";
                    }
                    if (jQueryDummy.e === "#newCapByteSize") {
                        return 2048;
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };
            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(true);
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'cap',
                size: 1024,
                byteSize: 2048
            });
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newCapSize');
            expect(window.$).toHaveBeenCalledWith('#newCapByteSize');
            expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).toHaveBeenCalled();
            expect(view.toggleNewIndexView).toHaveBeenCalled();
            expect(view.resetIndexForms).toHaveBeenCalled();

        });

        it("create Geo Index", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Geo";
                    }
                    if (jQueryDummy.e === "#newGeoFields") {
                        return "1,2,3,4,5,6";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };
            spyOn(view, "checkboxToValue").andCallFake(
                function (a) {
                    if (a === "#newGeoJson") {
                        return "newGeoJson";
                    }
                    if (a === "#newGeoConstraint") {
                        return "newGeoConstraint";
                    }
                    if (a === "#newGeoIgnoreNull") {
                        return "newGeoIgnoreNull";
                    }
                }
            );
            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(true);
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'geo',
                fields: ["1", "2", "3", "4", "5", "6"],
                geoJson: "newGeoJson",
                constraint: "newGeoConstraint",
                ignoreNull: "newGeoIgnoreNull"
            });
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newGeoFields');
            expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).toHaveBeenCalled();
            expect(view.toggleNewIndexView).toHaveBeenCalled();
            expect(view.resetIndexForms).toHaveBeenCalled();

        });


        it("create Hash Index", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Hash";
                    }
                    if (jQueryDummy.e === "#newHashFields") {
                        return "1,2,3,4,5,6";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            spyOn(view, "checkboxToValue").andCallFake(
                function (a) {
                    if (a === "#newHashUnique") {
                        return "newHashUnique";
                    }
                }
            );

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(true);
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'hash',
                fields: ["1", "2", "3", "4", "5", "6"],
                unique: "newHashUnique"
            });
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newHashFields');
            expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).toHaveBeenCalled();
            expect(view.toggleNewIndexView).toHaveBeenCalled();
            expect(view.resetIndexForms).toHaveBeenCalled();

        });


        it("create Fulltext Index", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Fulltext";
                    }
                    if (jQueryDummy.e === "#newFulltextFields") {
                        return "1,2,3,4,5,6";
                    }
                    if (jQueryDummy.e === "#newFulltextMinLength") {
                        return "100";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(true);
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'fulltext',
                fields: ["1", "2", "3", "4", "5", "6"],
                minLength: 100
            });
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newFulltextFields');
            expect(window.$).toHaveBeenCalledWith('#newFulltextMinLength');
            expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).toHaveBeenCalled();
            expect(view.toggleNewIndexView).toHaveBeenCalled();
            expect(view.resetIndexForms).toHaveBeenCalled();

        });


        it("create Skiplist Index", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Skiplist";
                    }
                    if (jQueryDummy.e === "#newSkiplistFields") {
                        return "1,2,3,4,5,6";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            spyOn(view, "checkboxToValue").andCallFake(
                function (a) {
                    if (a === "#newSkiplistUnique") {
                        return "newSkiplistUnique";
                    }
                }
            );

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(true);
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'skiplist',
                fields: ["1", "2", "3", "4", "5", "6"],
                unique: "newSkiplistUnique"
            });
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
            expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).toHaveBeenCalled();
            expect(view.toggleNewIndexView).toHaveBeenCalled();
            expect(view.resetIndexForms).toHaveBeenCalled();

        });


        it("create Skiplist Index with error and no response text", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Skiplist";
                    }
                    if (jQueryDummy.e === "#newSkiplistFields") {
                        return "1,2,3,4,5,6";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            spyOn(view, "checkboxToValue").andCallFake(
                function (a) {
                    if (a === "#newSkiplistUnique") {
                        return "newSkiplistUnique";
                    }
                }
            );

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(false);
            spyOn(arangoHelper, "arangoNotification");
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).not.toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'skiplist',
                fields: ["1", "2", "3", "4", "5", "6"],
                unique: "newSkiplistUnique"
            });

            expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
                "Document error", "Could not create index.");
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
            expect(window.$).not.toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).not.toHaveBeenCalled();
            expect(view.toggleNewIndexView).not.toHaveBeenCalled();
            expect(view.resetIndexForms).not.toHaveBeenCalled();

        });


        it("create Skiplist Index with error and response text", function () {
            view.collectionName = "coll";

            spyOn(view, "getIndex");
            spyOn(view, "toggleNewIndexView");
            spyOn(view, "resetIndexForms");


            jQueryDummy = {
                val: function (a) {
                    if (jQueryDummy.e === "#newIndexType") {
                        return "Skiplist";
                    }
                    if (jQueryDummy.e === "#newSkiplistFields") {
                        return "1,2,3,4,5,6";
                    }
                },
                remove: function () {

                }
            };
            spyOn(jQueryDummy, "val").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(window, "$").andCallFake(function (e) {
                jQueryDummy.e = e;
                return jQueryDummy;
            });

            spyOn(view, "checkboxToValue").andCallFake(
                function (a) {
                    if (a === "#newSkiplistUnique") {
                        return "newSkiplistUnique";
                    }
                }
            );

            var arangoCollectionsDummy = {
                createIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "createIndex").andReturn(
                {responseText: '{"errorMessage" : "blub"}'});
            spyOn(arangoHelper, "arangoNotification");
            window.arangoCollectionsStore = new window.arangoCollections();

            view.createIndex();

            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.remove).not.toHaveBeenCalled();
            expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith('coll', {
                type: 'skiplist',
                fields: ["1", "2", "3", "4", "5", "6"],
                unique: "newSkiplistUnique"
            });

            expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
                "Document error", 'blub');
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
            expect(window.$).not.toHaveBeenCalledWith('#collectionEditIndexTable tr');

            expect(view.getIndex).not.toHaveBeenCalled();
            expect(view.toggleNewIndexView).not.toHaveBeenCalled();
            expect(view.resetIndexForms).not.toHaveBeenCalled();

        });


        it("prepDeleteIndex", function () {
            jQueryDummy = {
                parent: function () {
                    return jQueryDummy;
                },
                first: function () {
                    return jQueryDummy;
                },
                children: function () {
                    return jQueryDummy;
                },
                text: function () {
                },
                modal: function () {

                }
            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "first").andCallThrough();
            spyOn(jQueryDummy, "children").andCallThrough();
            spyOn(jQueryDummy, "text");
            spyOn(jQueryDummy, "modal");

            spyOn(window, "$").andReturn(jQueryDummy);

            view.prepDeleteIndex({currentTarget: "blub"});

            expect(window.$).toHaveBeenCalledWith("blub");
            expect(window.$).toHaveBeenCalledWith("#indexDeleteModal");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(jQueryDummy.first).toHaveBeenCalled();
            expect(jQueryDummy.children).toHaveBeenCalled();
            expect(jQueryDummy.text).toHaveBeenCalled();
            expect(jQueryDummy.modal).toHaveBeenCalledWith('show');

        });


        it("deleteIndex", function () {
            jQueryDummy = {
                parent: function () {
                    return jQueryDummy;
                },
                remove: function () {
                },
                modal: function () {

                }
            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(jQueryDummy, "modal");

            spyOn(window, "$").andReturn(jQueryDummy);

            var arangoCollectionsDummy = {
                deleteIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "deleteIndex").andReturn(true);
            spyOn(arangoHelper, "arangoError");
            window.arangoCollectionsStore = new window.arangoCollections();

            view.lastTarget = {currentTarget: "blub"};
            view.collectionName = "coll";
            view.lastId = "lastId";
            view.deleteIndex();

            expect(window.$).toHaveBeenCalledWith("blub");
            expect(window.$).toHaveBeenCalledWith("#indexDeleteModal");
            expect(arangoCollectionsDummy.deleteIndex).toHaveBeenCalledWith("coll", "lastId");
            expect(jQueryDummy.parent).toHaveBeenCalled();
            expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
            expect(jQueryDummy.remove).toHaveBeenCalled();

        });

        it("deleteIndex with error", function () {
            jQueryDummy = {
                parent: function () {
                    return jQueryDummy;
                },
                remove: function () {
                },
                modal: function () {

                }
            };
            spyOn(jQueryDummy, "parent").andCallThrough();
            spyOn(jQueryDummy, "remove");
            spyOn(jQueryDummy, "modal");

            spyOn(window, "$").andReturn(jQueryDummy);

            var arangoCollectionsDummy = {
                deleteIndex: function () {
                }

            };

            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "deleteIndex").andReturn(false);
            spyOn(arangoHelper, "arangoError");
            window.arangoCollectionsStore = new window.arangoCollections();

            view.lastTarget = {currentTarget: "blub"};
            view.collectionName = "coll";
            view.lastId = "lastId";
            view.deleteIndex();

            expect(window.$).not.toHaveBeenCalledWith("blub");
            expect(window.$).toHaveBeenCalledWith("#indexDeleteModal");
            expect(arangoCollectionsDummy.deleteIndex).toHaveBeenCalledWith("coll", "lastId");
            expect(jQueryDummy.parent).not.toHaveBeenCalled();
            expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
            expect(jQueryDummy.remove).not.toHaveBeenCalled();
            expect(arangoHelper.arangoError).toHaveBeenCalledWith("Could not delete index");

        });


        it("selectIndexType", function () {
            jQueryDummy = {
                hide: function () {
                },
                val: function () {
                },
                show: function () {
                }
            };
            spyOn(jQueryDummy, "hide");
            spyOn(jQueryDummy, "show");
            spyOn(jQueryDummy, "val").andReturn("newType");

            spyOn(window, "$").andReturn(jQueryDummy);

            view.selectIndexType();

            expect(window.$).toHaveBeenCalledWith(".newIndexClass");
            expect(window.$).toHaveBeenCalledWith('#newIndexType');
            expect(window.$).toHaveBeenCalledWith('#newIndexTypenewType');
            expect(jQueryDummy.hide).toHaveBeenCalled();
            expect(jQueryDummy.val).toHaveBeenCalled();
            expect(jQueryDummy.show).toHaveBeenCalled();
        });


        it("checkboxToValue", function () {
            jQueryDummy = {
                prop: function () {
                }
            };
            spyOn(jQueryDummy, "prop");

            spyOn(window, "$").andReturn(jQueryDummy);

            view.checkboxToValue("anId");

            expect(window.$).toHaveBeenCalledWith("anId");
            expect(jQueryDummy.prop).toHaveBeenCalledWith("checked");

        });

        it("getIndex", function () {
            jQueryDummy = {
                append: function () {
                }
            };
            spyOn(jQueryDummy, "append");

            spyOn(window, "$").andReturn(jQueryDummy);

            var arangoCollectionsDummy = {
                getIndex: function () {
                }

            };
            spyOn(arangoHelper, "fixTooltips");
            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "getIndex").andReturn(
                {
                    indexes: [
                        {id: "abc/def1", type: 'primary', name: "index1"},
                        {id: "abc/def2", type: 'edge', name: "index2"},
                        {id: "abc/def3", type: 'dummy', name: "index3", fields: [1, 2, 3]}


                    ]
                }
            );
            window.arangoCollectionsStore = new window.arangoCollections();
            $.each = function (list, callback) {
                var callBackWraper = function (a, b) {
                    callback(b, a);
                };
                list.forEach(callBackWraper);
            };

            view.collectionID = "coll";
            view.getIndex();

            expect(arangoCollectionsDummy.getIndex).toHaveBeenCalledWith("coll", true);
            expect(view.index).toEqual({
                indexes: [
                    {id: "abc/def1", type: 'primary', name: "index1"},
                    {id: "abc/def2", type: 'edge', name: "index2"},
                    {id: "abc/def3", type: 'dummy', name: "index3", fields: [1, 2, 3]}


                ]
            });
            expect(window.$).toHaveBeenCalledWith("#collectionEditIndexTable");
            expect(jQueryDummy.append).toHaveBeenCalledWith(
                '<tr><th class="collectionInfoTh modal-text">def1</th><th class="collectionInfoTh' +
                    ' modal-text">primary</th><th class="collectionInfoTh modal-text">undefined' +
                    '</th><th class="collectionInfoTh modal-text"></th><th class=' +
                    '"collectionInfoTh modal-text">' +
                    '<span class="icon_arangodb_locked" data-original-title="No action">' +
                    '</span></th></tr>');
            expect(jQueryDummy.append).toHaveBeenCalledWith(
                '<tr><th class="collectionInfoTh modal-text">def2</th><th class="collectionInfoTh' +
                    ' modal-text">edge</th><th class="collectionInfoTh modal-text">undefined' +
                    '</th><th class="collectionInfoTh modal-text"></th><th class="' +
                    'collectionInfoTh modal-text">' +
                    '<span class="icon_arangodb_locked" data-original-title="No action">' +
                    '</span></th></tr>');
            expect(jQueryDummy.append).toHaveBeenCalledWith('<tr><th class="collectionInfoTh' +
                ' modal-text">def3</th><th class="collectionInfoTh modal-text">dummy' +
                '</th><th class="collectionInfoTh modal-text">undefined</th><th ' +
                'class="collectionInfoTh' +
                ' modal-text">1, 2, 3</th><th class="collectionInfoTh modal-text">' +
                '<span class="deleteIndex icon_arangodb_roundminus" data-original-title=' +
                '"Delete index" title="Delete index"></span></th></tr>');

            expect(arangoHelper.fixTooltips).toHaveBeenCalledWith("deleteIndex", "left");
        });


        it("getIndex with no result", function () {
            jQueryDummy = {
                append: function () {
                }
            };
            spyOn(jQueryDummy, "append");

            spyOn(window, "$").andReturn(jQueryDummy);

            var arangoCollectionsDummy = {
                getIndex: function () {
                }

            };
            spyOn(arangoHelper, "fixTooltips");
            spyOn(window, "arangoCollections").andReturn(arangoCollectionsDummy);
            spyOn(arangoCollectionsDummy, "getIndex").andReturn(undefined);
            window.arangoCollectionsStore = new window.arangoCollections();
            $.each = function (list, callback) {
                var callBackWraper = function (a, b) {
                    callback(b, a);
                };
                list.forEach(callBackWraper);
            };

            view.collectionID = "coll";
            view.getIndex();

            expect(arangoCollectionsDummy.getIndex).toHaveBeenCalledWith("coll", true);
            expect(view.index).toEqual(undefined);
            expect(window.$).not.toHaveBeenCalled();
            expect(jQueryDummy.append).not.toHaveBeenCalled();
            expect(arangoHelper.fixTooltips).not.toHaveBeenCalledWith();
        });


    });

}());

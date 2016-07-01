/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, jasmine, describe, beforeEach, afterEach, it, spyOn, expect, _*/

(function () {
  'use strict';

  describe('The documents view', function () {
    var view, jQueryDummy, arangoDocStoreDummy, arangoDocsStoreDummy, arangoCollectionsDummy;

    beforeEach(function () {
      arangoDocStoreDummy = {
        createTypeDocument: function () {
          throw 'Should be a spy';
        },
        createTypeEdge: function () {
          throw 'Should be a spy';
        },
        deleteDocument: function () {
          throw 'Should be a spy';
        },
        deleteEdge: function () {
          throw 'Should be a spy';
        }
      };
      arangoDocsStoreDummy = {
        models: [],
        totalPages: 1,
        currentPage: 1,
        documentsCount: 0,
        setToFirst: function () {
          throw 'Should be a spy.';
        },
        addFilter: function () {
          throw 'Should be a spy.';
        },
        resetFilter: function () {
          throw 'Should be a spy.';
        },
        getDocuments: function () {
          throw 'Should be a spy';
        },
        getPageSize: function () {
          return 10;
        },
        loadTotal: function () {
          throw 'Should be a spy';
        },
        updloadDocuments: function () {
          throw 'Should be a spy';
        },
        each: function (cb) {
          this.models.forEach(cb);
        },
        size: function () {
          return this.models.length;
        },
        getLastPageNumber: function () {
          return this.totalPages;
        },
        getPage: function () {
          return this.currentPage;
        },
        getTotal: function () {
          return this.documentsCount;
        },
        collectionID: 'collection'
      };
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };
      view = new window.DocumentsView({
        documentStore: arangoDocStoreDummy,
        collection: arangoDocsStoreDummy
      });
    });

    afterEach(function () {
      delete window.App;
    });

    it('assert the basics', function () {
      expect(view.filters).toEqual({ '0': true });
      expect(view.filterId).toEqual(0);
      expect(view.addDocumentSwitch).toEqual(true);
      expect(view.allowUpload).toEqual(false);
      expect(view.collectionContext).toEqual({
        prev: null,
        next: null
      });
      expect(view.table).toEqual('#documentsTableID');
      var expectedEvents = {
        'click #collectionPrev': 'prevCollection',
        'click #collectionNext': 'nextCollection',
        'click #filterCollection': 'filterCollection',
        'click #markDocuments': 'editDocuments',
        'click #indexCollection': 'indexCollection',
        'click #importCollection': 'importCollection',
        'click #exportCollection': 'exportCollection',
        'click #filterSend': 'sendFilter',
        'click #addFilterItem': 'addFilterItem',
        'click .removeFilterItem': 'removeFilterItem',
        'click #deleteSelected': 'deleteSelectedDocs',
        'click #moveSelected': 'moveSelectedDocs',
        'click #addDocumentButton': 'addDocumentModal',
        'click #documents_first': 'firstDocuments',
        'click #documents_last': 'lastDocuments',
        'click #documents_prev': 'prevDocuments',
        'click #documents_next': 'nextDocuments',
        'click #confirmDeleteBtn': 'confirmDelete',
        'click .key': 'nop',
        'keyup': 'returnPressedHandler',
        'keydown .queryline input': 'filterValueKeydown',
        'click #importModal': 'showImportModal',
        'click #resetView': 'resetView',
        'click #confirmDocImport': 'startUpload',
        'click #exportDocuments': 'startDownload',
        'change #newIndexType': 'selectIndexType',
        'click #createIndex': 'createIndex',
        'click .deleteIndex': 'prepDeleteIndex',
        'click #confirmDeleteIndexBtn': 'deleteIndex',
        'click #documentsToolbar ul': 'resetIndexForms',
        'click #indexHeader #addIndex': 'toggleNewIndexView',
        'click #indexHeader #cancelIndex': 'toggleNewIndexView',
        'change #documentSize': 'setPagesize',
        'change #docsSort': 'setSorting'
      };
      var expectedKeys = Object.keys(expectedEvents);
      var i, key;
      for (i = 0; i < expectedKeys.length; ++i) {
        key = expectedKeys[i];
        expect(view.events[key]).toEqual(expectedEvents[key]);
      }
      expect(
        _.difference(Object.keys(view.events), expectedKeys)
      ).toEqual([]);
    });

    it('showSpinner', function () {
      jQueryDummy = {
        show: function () {}
      };
      spyOn(jQueryDummy, 'show');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.showSpinner();
      expect(window.$).toHaveBeenCalledWith('#uploadIndicator');
      expect(jQueryDummy.show).toHaveBeenCalled();
    });

    it('hideSpinner', function () {
      jQueryDummy = {
        hide: function () {}
      };
      spyOn(jQueryDummy, 'hide');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.hideSpinner();
      expect(window.$).toHaveBeenCalledWith('#uploadIndicator');
      expect(jQueryDummy.hide).toHaveBeenCalled();
    });

    it('showImportModal', function () {
      jQueryDummy = {
        modal: function () {}
      };
      spyOn(jQueryDummy, 'modal');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.showImportModal();
      expect(window.$).toHaveBeenCalledWith('#docImportModal');
      expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
    });

    it('hideImportModal', function () {
      jQueryDummy = {
        modal: function () {}
      };
      spyOn(jQueryDummy, 'modal');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.hideImportModal();
      expect(window.$).toHaveBeenCalledWith('#docImportModal');
      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
    });

    it('returnPressedHandler call confirmDelete if keyCode == 13', function () {
      jQueryDummy = {
        attr: function () {
          return false;
        },
        is: function () {
          return false;
        }
      };
      spyOn(jQueryDummy, 'attr').andCallThrough();
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(view, 'confirmDelete').andCallFake(function () {});
      view.returnPressedHandler({keyCode: 13});
      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(jQueryDummy.attr).toHaveBeenCalledWith('disabled');
      expect(view.confirmDelete).toHaveBeenCalled();
    });

    it('returnPressedHandler does not call confirmDelete if keyCode !== 13', function () {
      jQueryDummy = {
        attr: function () {
          return false;
        },
        is: function () {
          return false;
        }
      };
      spyOn(jQueryDummy, 'attr');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(view, 'confirmDelete').andCallFake(function () {});
      view.returnPressedHandler({keyCode: 11});
      expect(jQueryDummy.attr).not.toHaveBeenCalled();
      expect(view.confirmDelete).not.toHaveBeenCalled();
    });

    it('returnPressedHandler does not call confirmDelete if keyCode == 13 but' +
      ' confirmButton is disabled', function () {
        jQueryDummy = {
          attr: function () {
            return true;
          },
          is: function () {
            return false;
          }
        };
        spyOn(jQueryDummy, 'attr');
        spyOn(window, '$').andReturn(
          jQueryDummy
        );
        spyOn(view, 'confirmDelete').andCallFake(function () {});
        view.returnPressedHandler({keyCode: 13});
        expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
        expect(jQueryDummy.attr).toHaveBeenCalledWith('disabled');
        expect(view.confirmDelete).not.toHaveBeenCalled();
      });

    it('toggleNewIndexView', function () {
      jQueryDummy = {
        toggle: function () {}
      };
      spyOn(jQueryDummy, 'toggle');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(view, 'resetIndexForms').andCallFake(function () {});
      view.toggleNewIndexView();
      expect(window.$).toHaveBeenCalledWith('#indexEditView');
      expect(window.$).toHaveBeenCalledWith('#newIndexView');
      expect(jQueryDummy.toggle).toHaveBeenCalledWith('fast');
      expect(view.resetIndexForms).toHaveBeenCalled();
    });

    it('nop', function () {
      var event = {stopPropagation: function () {}};
      spyOn(event, 'stopPropagation');
      view.nop(event);
      expect(event.stopPropagation).toHaveBeenCalled();
    });

    it('resetView', function () {
      jQueryDummy = {
        attr: function () {
          throw 'Should be a spy';
        },
        val: function () {
          throw 'Should be a spy';
        },
        css: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(jQueryDummy, 'val');
      spyOn(jQueryDummy, 'css');
      spyOn(jQueryDummy, 'attr');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(view, 'removeAllFilterItems');
      spyOn(view, 'drawTable');
      spyOn(view, 'renderPaginationElements');
      spyOn(arangoDocsStoreDummy, 'getDocuments');
      spyOn(arangoDocsStoreDummy, 'loadTotal');
      spyOn(arangoDocsStoreDummy, 'resetFilter');
      view.resetView();
      expect(window.$).toHaveBeenCalledWith('input');
      expect(window.$).toHaveBeenCalledWith('select');
      expect(window.$).toHaveBeenCalledWith('#documents_last');
      expect(window.$).toHaveBeenCalledWith('#documents_first');
      expect(jQueryDummy.val).toHaveBeenCalledWith('');
      expect(jQueryDummy.val).toHaveBeenCalledWith('==');
      expect(jQueryDummy.css).toHaveBeenCalledWith('visibility', 'visible');
      expect(jQueryDummy.css).toHaveBeenCalledWith('visibility', 'visible');
      expect(arangoDocsStoreDummy.resetFilter).toHaveBeenCalled();
      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
      expect(arangoDocsStoreDummy.loadTotal).toHaveBeenCalled();
      expect(view.removeAllFilterItems).toHaveBeenCalled();
      expect(view.drawTable).toHaveBeenCalled();
      expect(view.renderPaginationElements).toHaveBeenCalled();
      expect(view.addDocumentSwitch).toEqual(true);
    });

    it('start succesful Upload with XHR ready state = 4, ' +
      'XHR status = 201 and parseable JSON', function () {
        spyOn(view, 'showSpinner');
        spyOn(view, 'hideSpinner');
        spyOn(view, 'hideImportModal');
        spyOn(view, 'resetView');

        spyOn(arangoDocsStoreDummy, 'updloadDocuments').andReturn(true);

        view.allowUpload = true;

        view.startUpload();
        expect(arangoDocsStoreDummy.updloadDocuments).toHaveBeenCalledWith(view.file);
        expect(view.showSpinner).toHaveBeenCalled();
        expect(view.hideSpinner).toHaveBeenCalled();
        expect(view.hideImportModal).toHaveBeenCalled();
        expect(view.resetView).toHaveBeenCalled();
      });

    it('start succesful Upload with XHR ready state != 4', function () {
      spyOn(view, 'showSpinner');
      spyOn(view, 'hideSpinner');
      spyOn(view, 'hideImportModal');
      spyOn(view, 'resetView');
      spyOn(arangoDocsStoreDummy, 'updloadDocuments').andReturn('Upload error');

      view.allowUpload = true;
      spyOn(arangoHelper, 'arangoError');
      view.startUpload();
      expect(arangoDocsStoreDummy.updloadDocuments).toHaveBeenCalledWith(view.file);
      expect(view.showSpinner).toHaveBeenCalled();
      expect(view.hideSpinner).toHaveBeenCalled();
      expect(view.hideImportModal).not.toHaveBeenCalled();
      expect(view.resetView).not.toHaveBeenCalled();
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Upload error');
    });

    it('start succesful Upload mit XHR ready state = 4, ' +
      'XHR status = 201 and not parseable JSON', function () {
        spyOn(view, 'showSpinner');
        spyOn(view, 'hideSpinner');
        spyOn(view, 'hideImportModal');
        spyOn(view, 'resetView');
        spyOn(arangoDocsStoreDummy, 'updloadDocuments').andReturn(
          'Error: SyntaxError: Unable to parse JSON string'
        );

        view.allowUpload = true;
        spyOn(arangoHelper, 'arangoError');
        view.startUpload();
        expect(arangoDocsStoreDummy.updloadDocuments).toHaveBeenCalledWith(view.file);
        expect(view.showSpinner).toHaveBeenCalled();
        expect(view.hideSpinner).toHaveBeenCalled();
        expect(view.hideImportModal).toHaveBeenCalled();
        expect(view.resetView).toHaveBeenCalled();
        expect(arangoHelper.arangoError).toHaveBeenCalledWith(
          'Error: SyntaxError: Unable to parse JSON string'
        );
      });

    it('uploadSetup', function () {
      jQueryDummy = {
        change: function (e) {
          e({target: {files: ['BLUB']}});
        }
      };
      spyOn(jQueryDummy, 'change').andCallThrough();
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.uploadSetup();
      expect(window.$).toHaveBeenCalledWith('#importDocuments');
      expect(jQueryDummy.change).toHaveBeenCalledWith(jasmine.any(Function));
      expect(view.files).toEqual(['BLUB']);
      expect(view.file).toEqual('BLUB');
      expect(view.allowUpload).toEqual(true);
    });

    it('buildCollectionLink', function () {
      expect(view.buildCollectionLink({get: function () {
          return 'blub';
      }})).toEqual(
        'collection/' + encodeURIComponent('blub') + '/documents/1'
      );
    });

    it('prevCollection with no collectionContext.prev', function () {
      jQueryDummy = {
        parent: function (e) {
          return jQueryDummy;
        },
        addClass: function () {}

      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'addClass');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.collectionContext = {prev: null};
      view.prevCollection();
      expect(window.$).toHaveBeenCalledWith('#collectionPrev');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
    });

    it('prevCollection with collectionContext.prev', function () {
      jQueryDummy = {
        parent: function (e) {
          return jQueryDummy;
        },
        removeClass: function () {}

      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'removeClass');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.collectionContext = {prev: 1};
      spyOn(window.App, 'navigate');
      spyOn(view, 'buildCollectionLink').andReturn(1);
      view.prevCollection();
      expect(window.$).toHaveBeenCalledWith('#collectionPrev');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
      expect(window.App.navigate).toHaveBeenCalledWith(1, {
        trigger: true
      });
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('disabledPag');
    });

    it('nextCollection with no collectionContext.next', function () {
      jQueryDummy = {
        parent: function (e) {
          return jQueryDummy;
        },
        addClass: function () {}

      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'addClass');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.collectionContext = {next: null};
      view.nextCollection();
      expect(window.$).toHaveBeenCalledWith('#collectionNext');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
    });

    it('nextCollection with collectionContext.next', function () {
      jQueryDummy = {
        parent: function (e) {
          return jQueryDummy;
        },
        removeClass: function () {}

      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'removeClass');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.collectionContext = {next: 1};
      spyOn(window.App, 'navigate');
      spyOn(view, 'buildCollectionLink').andReturn(1);
      view.nextCollection();
      expect(window.$).toHaveBeenCalledWith('#collectionNext');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(view.buildCollectionLink).toHaveBeenCalledWith(1);
      expect(window.App.navigate).toHaveBeenCalledWith(1, {
        trigger: true
      });
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('disabledPag');
    });

    it('filterCollection', function () {
      jQueryDummy = {
        removeClass: function () {},
        toggleClass: function () {},
        slideToggle: function () {},
        hide: function () {},
        focus: function () {}

      };
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'toggleClass');
      spyOn(jQueryDummy, 'slideToggle');
      spyOn(jQueryDummy, 'hide');
      spyOn(jQueryDummy, 'focus');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.filters = [
        {0: 'bla'},
        {1: 'blub'}
      ];
      view.filterCollection();
      expect(window.$).toHaveBeenCalledWith('#indexCollection');
      expect(window.$).toHaveBeenCalledWith('#importCollection');
      expect(window.$).toHaveBeenCalledWith('#filterCollection');
      expect(window.$).toHaveBeenCalledWith('#filterHeader');
      expect(window.$).toHaveBeenCalledWith('#importHeader');
      expect(window.$).toHaveBeenCalledWith('#indexHeader');
      expect(window.$).toHaveBeenCalledWith('#attribute_name0');
      // next assertion seems strange but follows the strange implementation
      expect(window.$).not.toHaveBeenCalledWith('#attribute_name1');
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
      expect(jQueryDummy.hide).toHaveBeenCalled();
      expect(jQueryDummy.focus).toHaveBeenCalled();
    });

    it('importCollection', function () {
      jQueryDummy = {
        removeClass: function () {},
        toggleClass: function () {},
        slideToggle: function () {},
        hide: function () {}

      };
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'toggleClass');
      spyOn(jQueryDummy, 'slideToggle');
      spyOn(jQueryDummy, 'hide');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.importCollection();

      expect(window.$).toHaveBeenCalledWith('#filterCollection');
      expect(window.$).toHaveBeenCalledWith('#indexCollection');
      expect(window.$).toHaveBeenCalledWith('#importCollection');
      expect(window.$).toHaveBeenCalledWith('#filterHeader');
      expect(window.$).toHaveBeenCalledWith('#importHeader');
      expect(window.$).toHaveBeenCalledWith('#indexHeader');
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
      expect(jQueryDummy.hide).toHaveBeenCalled();
    });

    it('indexCollection', function () {
      jQueryDummy = {
        removeClass: function () {},
        toggleClass: function () {},
        slideToggle: function () {},
        hide: function () {},
        show: function () {}

      };
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'toggleClass');
      spyOn(jQueryDummy, 'slideToggle');
      spyOn(jQueryDummy, 'hide');
      spyOn(jQueryDummy, 'show');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      view.indexCollection();
      expect(window.$).toHaveBeenCalledWith('#filterCollection');
      expect(window.$).toHaveBeenCalledWith('#importCollection');
      expect(window.$).toHaveBeenCalledWith('#indexCollection');
      expect(window.$).toHaveBeenCalledWith('#newIndexView');
      expect(window.$).toHaveBeenCalledWith('#indexEditView');
      expect(window.$).toHaveBeenCalledWith('#indexHeader');
      expect(window.$).toHaveBeenCalledWith('#importHeader');
      expect(window.$).toHaveBeenCalledWith('#filterHeader');
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.toggleClass).toHaveBeenCalledWith('activated');
      expect(jQueryDummy.slideToggle).toHaveBeenCalledWith(200);
      expect(jQueryDummy.hide).toHaveBeenCalled();
      expect(jQueryDummy.show).toHaveBeenCalled();
    });

    it('getFilterContent', function () {
      jQueryDummy = {
        e: undefined,
        val: function () {
          var e = jQueryDummy.e;
          if (e === '#attribute_value0') {
            return '{"jsonval" : 1}';
          }
          if (e === '#attribute_value1') {
            return 'stringval';
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
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(window, '$').andCallFake(
        function (e) {
          jQueryDummy.e = e;
          return jQueryDummy;
        }
      );
      view.filters = [
        {0: 'bla'},
        {1: 'blub'}
      ];
      expect(view.getFilterContent()).toEqual([
        { attribute: 'name0', operator: 'operator0', value: { jsonval: 1 } },
        { attribute: 'name1', operator: 'operator1', value: 'stringval' }
      ]);
      expect(window.$).toHaveBeenCalledWith('#attribute_value0');
      expect(window.$).toHaveBeenCalledWith('#attribute_value1');
      expect(window.$).toHaveBeenCalledWith('#attribute_name0');
      expect(window.$).toHaveBeenCalledWith('#attribute_name1');
      expect(window.$).toHaveBeenCalledWith('#operator0');
      expect(window.$).toHaveBeenCalledWith('#operator1');
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
    });

    it('sendFilter', function () {
      jQueryDummy = {
        css: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(jQueryDummy, 'css');
      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(view, 'getFilterContent').andReturn(
        [
          { attribute: 'name0', operator: 'operator0', value: { jsonval: 1 } },
          { attribute: 'name1', operator: 'operator1', value: 'stringval' }
        ]

      );

      spyOn(arangoDocStoreDummy, 'setToFirst');
      spyOn(arangoDocStoreDummy, 'addFilter');
      spyOn(arangoDocStoreDummy, 'resetFilter');
      spyOn(arangoDocStoreDummy, 'getDocuments');
      spyOn(view, 'drawTable');
      spyOn(view, 'renderPaginationElements');

      view.sendFilter();

      expect(view.addDocumentSwitch).toEqual(false);
      expect(view.drawTable).toHaveBeenCalled();
      expect(view.renderPaginationElements).toHaveBeenCalled();
      expect(arangoDocStoreDummy.resetFilter).toHaveBeenCalled();
      expect(arangoDocStoreDummy.addFilter).toHaveBeenCalledWith(
        'name0', 'operator0', { jsonval: 1 }
      );
      expect(arangoDocStoreDummy.addFilter).toHaveBeenCalledWith(
        'name1', 'operator1', 'stringval'
      );
      expect(arangoDocStoreDummy.setToFirst).toHaveBeenCalled();
      expect(arangoDocStoreDummy.getDocuments).toHaveBeenCalled();

      expect(window.$).toHaveBeenCalledWith('#documents_last');
      expect(window.$).toHaveBeenCalledWith('#documents_first');
      expect(jQueryDummy.css).toHaveBeenCalledWith('visibility', 'hidden');
      expect(jQueryDummy.css).toHaveBeenCalledWith('visibility', 'hidden');
    });

    it('addFilterItem', function () {
      jQueryDummy = {
        append: function () {}

      };
      spyOn(jQueryDummy, 'append');
      spyOn(window, '$').andReturn(jQueryDummy);

      view.filterId = 1;
      var num = 2;
      view.addFilterItem();
      expect(window.$).toHaveBeenCalledWith('#filterHeader');
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

    it('filterValueKeydown with keyCode == 13', function () {
      spyOn(view, 'sendFilter');
      view.filterValueKeydown({keyCode: 13});
      expect(view.sendFilter).toHaveBeenCalled();
    });

    it('filterValueKeydown with keyCode !== 13', function () {
      spyOn(view, 'sendFilter');
      view.filterValueKeydown({keyCode: 11});
      expect(view.sendFilter).not.toHaveBeenCalled();
    });

    it('removeFilterItem with keyCode !== 13', function () {
      jQueryDummy = {
        remove: function () {}

      };
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andReturn(jQueryDummy);
      view.filters[1] = 'bla';

      view.removeFilterItem({currentTarget: {id: 'removeFilter1', parentElement: '#blub'}});
      expect(window.$).toHaveBeenCalledWith('#blub');
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(view.filters[1]).toEqual(undefined);
    });

    it('removeAllFilterItems', function () {
      jQueryDummy = {
        children: function () {},
        parent: function () {}

      };
      spyOn(jQueryDummy, 'children').andReturn([1, 2, 3]);
      spyOn(jQueryDummy, 'parent').andReturn({remove: function () {}});
      spyOn(window, '$').andReturn(jQueryDummy);

      view.removeAllFilterItems();
      expect(window.$).toHaveBeenCalledWith('#filterHeader');
      expect(window.$).toHaveBeenCalledWith('#removeFilter1');
      expect(window.$).toHaveBeenCalledWith('#removeFilter2');
      expect(window.$).toHaveBeenCalledWith('#removeFilter3');
      expect(jQueryDummy.children).toHaveBeenCalled();
      expect(jQueryDummy.parent).toHaveBeenCalled();

      expect(view.filters).toEqual({ '0': true });
      expect(view.filterId).toEqual(0);
    });

    it('addDocument without an error', function () {
      spyOn(arangoDocStoreDummy, 'createTypeDocument').andReturn('newDoc');

      spyOn(arangoHelper, 'collectionApiType').andReturn('document');
      window.location.hash = '1/2';
      view.addDocument();

      expect(arangoHelper.collectionApiType).toHaveBeenCalledWith('2', true);
      expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith('2');
      expect(window.location.hash).toEqual('#collection/' + 'newDoc');
    });

    it('addDocument with an edge', function () {
      spyOn(arangoDocStoreDummy, 'createTypeDocument').andReturn('newDoc');

      jQueryDummy = {
        modal: function () {}

      };
      spyOn(jQueryDummy, 'modal');
      spyOn(arangoHelper, 'fixTooltips');
      spyOn(window, '$').andReturn(jQueryDummy);

      spyOn(arangoHelper, 'collectionApiType').andReturn('edge');
      window.location.hash = '1/2';
      view.addDocument();

      expect(window.$).toHaveBeenCalledWith('#edgeCreateModal');
      expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
      expect(arangoHelper.fixTooltips).toHaveBeenCalledWith('.modalTooltips', 'left');
      expect(arangoHelper.collectionApiType).toHaveBeenCalledWith('2', true);
      expect(arangoDocStoreDummy.createTypeDocument).not.toHaveBeenCalled();
      expect(window.location.hash).toEqual('#1/2');
    });

    it('addDocument with an error', function () {
      spyOn(arangoDocStoreDummy, 'createTypeDocument').andReturn(false);

      spyOn(arangoHelper, 'collectionApiType').andReturn('document');
      spyOn(arangoHelper, 'arangoError');
      window.location.hash = '1/2';
      view.addDocument();

      expect(arangoHelper.collectionApiType).toHaveBeenCalledWith('2', true);
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating document failed');
      expect(arangoDocStoreDummy.createTypeDocument).toHaveBeenCalledWith('2');
      expect(window.location.hash).toEqual('#1/2');
    });

    it('addEdge with missing to ', function () {
      jQueryDummy = {
        e: undefined,
        val: function () {
          if (jQueryDummy.e === '#new-document-from') {
            return '';
          }
          if (jQueryDummy.e === '#new-document-to') {
            return 'bla';
          }
        }

      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(arangoDocStoreDummy, 'createTypeEdge');

      window.location.hash = '1/2';
      view.addEdge();
      expect(window.$).toHaveBeenCalledWith('#new-document-from');
      expect(window.$).toHaveBeenCalledWith('#new-document-to');
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();
    });

    it('addEdge with missing to ', function () {
      jQueryDummy = {
        e: undefined,
        val: function () {
          if (jQueryDummy.e === '#new-document-from') {
            return 'bla';
          }
          if (jQueryDummy.e === '#new-document-to') {
            return '';
          }
        }

      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(arangoDocStoreDummy, 'createTypeEdge');

      window.location.hash = '1/2';
      view.addEdge();
      expect(window.$).toHaveBeenCalledWith('#new-document-from');
      expect(window.$).toHaveBeenCalledWith('#new-document-to');
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(arangoDocStoreDummy.createTypeEdge).not.toHaveBeenCalled();
    });

    it('addEdge with success', function () {
      jQueryDummy = {
        e: undefined,
        val: function () {
          if (jQueryDummy.e === '#new-document-from') {
            return 'bla';
          }
          if (jQueryDummy.e === '#new-document-to') {
            return 'blub';
          }
        },
        modal: function () {}

      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'modal');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(arangoDocStoreDummy, 'createTypeEdge').andReturn('newEdge');

      window.location.hash = '1/2';
      view.addEdge();
      expect(window.$).toHaveBeenCalledWith('#new-document-from');
      expect(window.$).toHaveBeenCalledWith('#new-document-to');
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
      expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith('2', 'bla', 'blub');
      expect(window.location.hash).toEqual('#collection/newEdge');
    });

    it('addEdge with error', function () {
      jQueryDummy = {
        e: undefined,
        val: function () {
          if (jQueryDummy.e === '#new-document-from') {
            return 'bla';
          }
          if (jQueryDummy.e === '#new-document-to') {
            return 'blub';
          }
        },
        modal: function () {}

      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'modal');
      spyOn(arangoHelper, 'arangoError');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(arangoDocStoreDummy, 'createTypeEdge').andReturn(false);

      window.location.hash = '1/2';
      view.addEdge();
      expect(window.$).toHaveBeenCalledWith('#new-document-from');
      expect(window.$).toHaveBeenCalledWith('#new-document-to');
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Creating edge failed');
      expect(jQueryDummy.modal).not.toHaveBeenCalled();
      expect(arangoDocStoreDummy.createTypeEdge).toHaveBeenCalledWith('2', 'bla', 'blub');
      expect(window.location.hash).toEqual('#1/2');
    });

    it('first-last-next-prev document', function () {
      spyOn(view, 'jumpTo');
      spyOn(arangoDocsStoreDummy, 'getLastPageNumber').andReturn(5);
      spyOn(arangoDocsStoreDummy, 'getPage').andReturn(4);

      view.firstDocuments();
      expect(view.jumpTo).toHaveBeenCalledWith(1);
      view.lastDocuments();
      expect(view.jumpTo).toHaveBeenCalledWith(5);
      view.prevDocuments();
      expect(view.jumpTo).toHaveBeenCalledWith(3);
      view.nextDocuments();
      expect(view.jumpTo).toHaveBeenCalledWith(5);
    });

    it('remove', function () {
      jQueryDummy = {
        e: undefined,
        prev: function () {
          return jQueryDummy;
        },
        attr: function () {},
        modal: function () {}

      };
      spyOn(jQueryDummy, 'prev').andCallThrough();
      spyOn(jQueryDummy, 'attr');
      spyOn(jQueryDummy, 'modal');
      spyOn(window, '$').andReturn(jQueryDummy);

      view.remove({currentTarget: {parentElement: '#thiselement'}});

      expect(window.$).toHaveBeenCalledWith('#thiselement');
      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(window.$).toHaveBeenCalledWith('#docDeleteModal');
      expect(jQueryDummy.prev).toHaveBeenCalled();
      expect(jQueryDummy.attr).toHaveBeenCalledWith('disabled', false);
      expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
      expect(view.alreadyClicked).toEqual(true);
      expect(view.idelement).toEqual(jQueryDummy);
    });

    it('confirmDelete with check != source', function () {
      jQueryDummy = {
        attr: function () {}
      };
      spyOn(jQueryDummy, 'attr');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/4';
      spyOn(view, 'reallyDelete');
      view.confirmDelete();

      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(jQueryDummy.attr).toHaveBeenCalledWith('disabled', true);
      expect(view.reallyDelete).toHaveBeenCalled();
    });

    it('confirmDelete with check = source', function () {
      jQueryDummy = {
        attr: function () {}
      };
      spyOn(jQueryDummy, 'attr');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/source';
      spyOn(view, 'reallyDelete');
      view.confirmDelete();

      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(jQueryDummy.attr).toHaveBeenCalledWith('disabled', true);
      expect(view.reallyDelete).not.toHaveBeenCalled();
    });

    it('reallyDelete a document with error', function () {
      jQueryDummy = {
        closest: function () {
          return jQueryDummy;
        },
        get: function () {},
        next: function () {
          return jQueryDummy;
        },
        text: function () {}
      };
      spyOn(jQueryDummy, 'closest').andCallThrough();
      spyOn(jQueryDummy, 'get');
      spyOn(jQueryDummy, 'next').andCallThrough();
      spyOn(jQueryDummy, 'text').andReturn('4');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/source';

      view.type = 'document';
      view.colid = 'collection';

      spyOn(arangoHelper, 'arangoError');

      spyOn(arangoDocStoreDummy, 'deleteDocument').andReturn(false);
      spyOn(arangoDocsStoreDummy, 'getDocuments');
      view.target = '#confirmDeleteBtn';

      view.reallyDelete();

      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Doc error');
      expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith('collection', '4');
    });

    it('reallyDelete a edge with error', function () {
      jQueryDummy = {
        closest: function () {
          return jQueryDummy;
        },
        get: function () {},
        next: function () {
          return jQueryDummy;
        },
        text: function () {}
      };
      spyOn(jQueryDummy, 'closest').andCallThrough();
      spyOn(jQueryDummy, 'get');
      spyOn(jQueryDummy, 'next').andCallThrough();
      spyOn(jQueryDummy, 'text').andReturn('4');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/source';

      view.type = 'edge';
      view.colid = 'collection';

      spyOn(arangoHelper, 'arangoError');
      spyOn(arangoDocStoreDummy, 'deleteEdge').andReturn(false);
      spyOn(arangoDocsStoreDummy, 'getDocuments');
      view.target = '#confirmDeleteBtn';

      view.reallyDelete();

      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Edge error');
      expect(arangoDocsStoreDummy.getDocuments).not.toHaveBeenCalled();
      expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith('collection', '4');
    });

    it('reallyDelete a document with no error', function () {
      jQueryDummy = {
        closest: function () {
          return jQueryDummy;
        },
        get: function () {},
        next: function () {
          return jQueryDummy;
        },
        text: function () {},
        dataTable: function () {
          return jQueryDummy;
        },
        fnDeleteRow: function () {},
        fnGetPosition: function () {},
        fnClearTable: function () {},
        modal: function () {}

      };
      spyOn(jQueryDummy, 'closest').andCallThrough();
      spyOn(jQueryDummy, 'get').andReturn(1);
      spyOn(jQueryDummy, 'fnClearTable');
      spyOn(jQueryDummy, 'modal');
      spyOn(jQueryDummy, 'fnGetPosition').andReturn(3);
      spyOn(jQueryDummy, 'next').andCallThrough();
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'fnDeleteRow').andCallThrough();
      spyOn(jQueryDummy, 'text').andReturn('4');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/source';

      view.type = 'document';
      view.colid = 'collection';

      spyOn(arangoHelper, 'arangoError');
      spyOn(arangoDocStoreDummy, 'deleteDocument').andReturn(true);
      spyOn(arangoDocsStoreDummy, 'getDocuments');

      spyOn(view, 'drawTable');
      view.target = '#confirmDeleteBtn';
      spyOn(view, 'renderPaginationElements');

      view.reallyDelete();

      expect(view.renderPaginationElements).toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');
      expect(window.$).toHaveBeenCalledWith('#documentsTableID');
      expect(window.$).toHaveBeenCalledWith('#docDeleteModal');

      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
      expect(jQueryDummy.dataTable).toHaveBeenCalled();
      expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
      expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
      expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();

      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
      expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Doc error');
      expect(arangoDocStoreDummy.deleteDocument).toHaveBeenCalledWith('collection', '4');
    });

    it('reallyDelete a edge with no error', function () {
      jQueryDummy = {
        closest: function () {
          return jQueryDummy;
        },
        get: function () {},
        next: function () {
          return jQueryDummy;
        },
        text: function () {},
        dataTable: function () {
          return jQueryDummy;
        },
        fnDeleteRow: function () {},
        fnGetPosition: function () {},
        fnClearTable: function () {},
        modal: function () {}

      };
      spyOn(jQueryDummy, 'closest').andCallThrough();
      spyOn(jQueryDummy, 'get').andReturn(1);
      spyOn(jQueryDummy, 'fnClearTable');
      spyOn(jQueryDummy, 'modal');
      spyOn(jQueryDummy, 'fnGetPosition').andReturn(3);
      spyOn(jQueryDummy, 'next').andCallThrough();
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'fnDeleteRow').andCallThrough();
      spyOn(jQueryDummy, 'text').andReturn('4');
      spyOn(window, '$').andReturn(jQueryDummy);
      window.location.hash = '1/2/3/source';

      view.type = 'edge';
      view.colid = 'collection';

      spyOn(arangoHelper, 'arangoError');
      spyOn(arangoDocStoreDummy, 'deleteEdge').andReturn(true);
      spyOn(arangoDocsStoreDummy, 'getDocuments');

      view.target = '#confirmDeleteBtn';
      spyOn(view, 'drawTable');
      spyOn(view, 'renderPaginationElements');

      view.reallyDelete();

      expect(view.renderPaginationElements).toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('#confirmDeleteBtn');

      expect(window.$).toHaveBeenCalledWith('#documentsTableID');
      expect(window.$).toHaveBeenCalledWith('#docDeleteModal');

      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
      expect(jQueryDummy.dataTable).toHaveBeenCalled();
      expect(jQueryDummy.fnGetPosition).toHaveBeenCalledWith(1);
      expect(jQueryDummy.fnDeleteRow).toHaveBeenCalledWith(3);
      expect(jQueryDummy.fnClearTable).toHaveBeenCalledWith();
      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();

      expect(arangoHelper.arangoError).not.toHaveBeenCalledWith('Edge error');
      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalled();
      expect(arangoDocStoreDummy.deleteEdge).toHaveBeenCalledWith('collection', '4');
    });

    it('clicked when alreadyClicked', function () {
      view.alreadyClicked = true;
      expect(view.clicked('anything')).toEqual(0);
    });

    it('clicked when clicked event has no target', function () {
      jQueryDummy = {
        dataTable: function () {
          return jQueryDummy;
        },
        fnGetPosition: function () {}

      };
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'fnGetPosition').andReturn(null);
      spyOn(window, '$').andReturn(jQueryDummy);
      view.alreadyClicked = false;
      expect(view.clicked('anything')).toEqual(undefined);
    });

    it('clicked when clicked event has target but no valid checkData', function () {
      jQueryDummy = {
        dataTable: function () {
          return jQueryDummy;
        },
        fnGetPosition: function () {},
        fnGetData: function () {}
      };
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'fnGetPosition').andReturn(1);
      spyOn(jQueryDummy, 'fnGetData').andReturn([0, '']);
      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(view, 'addDocument');
      view.alreadyClicked = false;
      expect(view.clicked('anything')).toEqual(undefined);
      expect(view.addDocument).toHaveBeenCalled();
    });

    it('clicked when clicked event has target and valid checkData', function () {
      jQueryDummy = {
        dataTable: function () {
          return jQueryDummy;
        },
        fnGetPosition: function () {},
        fnGetData: function () {},
        next: function () {
          return jQueryDummy;
        },
        text: function () {}

      };
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'next').andCallThrough();
      spyOn(jQueryDummy, 'text').andReturn(12);
      spyOn(jQueryDummy, 'fnGetPosition').andReturn(1);
      spyOn(jQueryDummy, 'fnGetData').andReturn([0, 'true']);
      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(view, 'addDocument');
      view.alreadyClicked = false;
      expect(view.clicked({currentTarget: {firstChild: 'blub'}})).toEqual(undefined);
      expect(view.addDocument).not.toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('blub');
      expect(window.location.hash).toEqual('#collection/' + arangoDocsStoreDummy.collectionID + '/12');
    });

    it('drawTable with empty collection', function () {
      jQueryDummy = {
        text: function () {}

      };
      spyOn(jQueryDummy, 'text');
      spyOn(window, '$').andReturn(jQueryDummy);

      view.drawTable();
      expect(window.$).toHaveBeenCalledWith('.dataTables_empty');
      expect(jQueryDummy.text).toHaveBeenCalledWith('No documents');
    });

    it('drawTable with not empty collection', function () {
      jQueryDummy = {
        dataTable: function () {
          return jQueryDummy;
        },
        fnAddData: function () {},
        snippet: function () {},
        each: function (list, callback) {
          var callBackWraper = function (a, b) {
            callback(b, a);
          };
          list.forEach(callBackWraper);
        }
      };
      spyOn(jQueryDummy, 'dataTable').andCallThrough();
      spyOn(jQueryDummy, 'fnAddData');
      spyOn(jQueryDummy, 'snippet');
      spyOn(window, '$').andReturn(jQueryDummy);

      arangoDocsStoreDummy.models = [
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
      ];
      arangoDocsStoreDummy.totalPages = 1;
      arangoDocsStoreDummy.currentPage = 2;
      arangoDocsStoreDummy.documentsCount = 3;
      spyOn(arangoHelper, 'fixTooltips');

      view.drawTable();
      expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
        '.icon_arangodb, .arangoicon', 'top'
      );
      expect(window.$).toHaveBeenCalledWith('.prettify');
      expect(jQueryDummy.dataTable).toHaveBeenCalled();
      expect(jQueryDummy.snippet).toHaveBeenCalledWith('javascript', {
        style: 'nedit',
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });
      expect(jQueryDummy.fnAddData).toHaveBeenCalled();
    });

    describe('render', function () {
      var jQueryDummy;
      beforeEach(function () {
        jQueryDummy = {
          parent: function () {
            return jQueryDummy;
          },
          addClass: function () {},
          tooltip: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {}
        };
        spyOn(jQueryDummy, 'parent').andCallThrough();
        spyOn(jQueryDummy, 'addClass');
        spyOn(jQueryDummy, 'tooltip');
        spyOn(jQueryDummy, 'html');
        spyOn(jQueryDummy, 'val');
        spyOn(jQueryDummy, 'removeClass');
        spyOn(window, '$').andReturn(jQueryDummy);
        arangoCollectionsDummy = {
          getPosition: function () {}

        };
        spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
        view.collectionsStore = new window.arangoCollections();
        spyOn(view, 'getIndex');
        spyOn(view, 'breadcrumb');
        spyOn(view, 'uploadSetup');
        spyOn(view, 'drawTable');
        spyOn(view, 'renderPaginationElements');
        spyOn(arangoHelper, 'fixTooltips');
        view.el = 1;
        view.template = {
          render: function () {
            return 'tmp';
          }
        };
      });

      it('render with no prev and no next page', function () {
        spyOn(arangoCollectionsDummy, 'getPosition').andReturn({prev: null, next: null});
        expect(view.render()).toEqual(view);

        expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith(arangoDocsStoreDummy.collectionID);
        expect(view.getIndex).toHaveBeenCalled();
        expect(view.renderPaginationElements).toHaveBeenCalled();
        expect(view.breadcrumb).toHaveBeenCalled();
        expect(view.uploadSetup).toHaveBeenCalled();
        expect(jQueryDummy.parent).toHaveBeenCalled();
        expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
        expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
        expect(jQueryDummy.tooltip).toHaveBeenCalled();
        expect(window.$).toHaveBeenCalledWith('#collectionPrev');
        expect(window.$).toHaveBeenCalledWith('#collectionNext');
        expect(window.$).toHaveBeenCalledWith('[data-toggle=tooltip]');
        expect(window.$).toHaveBeenCalledWith(1);

        expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
          '.icon_arangodb, .arangoicon', 'top'
        );
      });

      it('render with no prev but next page', function () {
        spyOn(arangoCollectionsDummy, 'getPosition').andReturn({prev: null, next: 1});
        expect(view.render()).toEqual(view);

        expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith(arangoDocsStoreDummy.collectionID);
        expect(view.getIndex).toHaveBeenCalled();
        expect(view.breadcrumb).toHaveBeenCalled();
        expect(view.renderPaginationElements).toHaveBeenCalled();
        expect(view.uploadSetup).toHaveBeenCalled();
        expect(jQueryDummy.parent).toHaveBeenCalled();
        expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
        expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
        expect(jQueryDummy.tooltip).toHaveBeenCalled();
        expect(window.$).toHaveBeenCalledWith('#collectionPrev');
        expect(window.$).not.toHaveBeenCalledWith('#collectionNext');
        expect(window.$).toHaveBeenCalledWith('[data-toggle=tooltip]');
        expect(window.$).toHaveBeenCalledWith(1);

        expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
          '.icon_arangodb, .arangoicon', 'top'
        );
      });

      it('render with  prev but no next page', function () {
        spyOn(arangoCollectionsDummy, 'getPosition').andReturn({prev: 1, next: null});
        expect(view.render()).toEqual(view);

        expect(arangoCollectionsDummy.getPosition).toHaveBeenCalledWith(arangoDocsStoreDummy.collectionID);
        expect(view.getIndex).toHaveBeenCalled();
        expect(view.breadcrumb).toHaveBeenCalled();
        expect(view.renderPaginationElements).toHaveBeenCalled();
        expect(view.uploadSetup).toHaveBeenCalled();
        expect(jQueryDummy.parent).toHaveBeenCalled();
        expect(jQueryDummy.addClass).toHaveBeenCalledWith('disabledPag');
        expect(jQueryDummy.html).toHaveBeenCalledWith('tmp');
        expect(jQueryDummy.tooltip).toHaveBeenCalled();
        expect(window.$).not.toHaveBeenCalledWith('#collectionPrev');
        expect(window.$).toHaveBeenCalledWith('#collectionNext');
        expect(window.$).toHaveBeenCalledWith('[data-toggle=tooltip]');
        expect(window.$).toHaveBeenCalledWith(1);

        expect(arangoHelper.fixTooltips).toHaveBeenCalledWith(
          '.icon_arangodb, .arangoicon', 'top'
        );
      });
    });

    it('renderPagination with filter check and totalDocs > 0 ', function () {
      jQueryDummy = {
        html: function () {},
        css: function () {},
        pagination: function (o) {
          o.click(5);
        },
        prepend: function () {},
        append: function () {},
        length: 10

      };
      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'css');
      spyOn(jQueryDummy, 'prepend');
      spyOn(jQueryDummy, 'append');
      spyOn(jQueryDummy, 'pagination').andCallThrough();
      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoDocsStoreDummy = {
        currentFilterPage: 2,
        getFilteredDocuments: function () {},
        getTotal: function () {
          return 11;
        },
        getPage: function () {},
        getLastPageNumber: function () {},
        setPage: function () {},
        getDocuments: function () {},
        size: function () {},
        models: [],
        each: function (cb) {
          arangoDocsStoreDummy.models.forEach(cb);
        }
      };
      spyOn(window, 'arangoDocuments').andReturn(arangoDocsStoreDummy);
      spyOn(arangoDocsStoreDummy, 'getFilteredDocuments');
      spyOn(arangoHelper, 'fixTooltips');
      view.collection = new window.arangoDocuments();
      spyOn(view, 'getFilterContent').andReturn(
        [
          [' u.`name0`operator0@param0', ' u.`name1`operator1@param1'],
          { param0: {jsonval: 1}, param1: 'stringval'}
        ]

      );
      spyOn(view, 'rerender');
      view.colid = 1;
      view.documentsCount = 11;
      view.renderPagination(3, true);

      expect(view.rerender).toHaveBeenCalled();
    });

    it('renderPagination with no filter check and totalDocs = 0 ', function () {
      jQueryDummy = {
        html: function () {},
        css: function () {},
        pagination: function (o) {
          o.click(5);
        },
        prepend: function () {},
        append: function () {},
        length: 0

      };
      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'css');
      spyOn(jQueryDummy, 'prepend');
      spyOn(jQueryDummy, 'append');
      spyOn(jQueryDummy, 'pagination').andCallThrough();
      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoDocsStoreDummy = {
        currentFilterPage: 2,
        getFilteredDocuments: function () {},
        getPage: function () {},
        getLastPageNumber: function () {},
        setPage: function () {},
        getDocuments: function () {},
        size: function () {},
        models: [],
        each: function (cb) {
          arangoDocsStoreDummy.models.forEach(cb);
        }
      };
      spyOn(window, 'arangoDocuments').andReturn(arangoDocsStoreDummy);
      spyOn(arangoDocsStoreDummy, 'getFilteredDocuments');
      spyOn(arangoHelper, 'fixTooltips');
      view.collection = new window.arangoDocuments();

      spyOn(view, 'getFilterContent').andReturn(
        [
          [' u.`name0`operator0@param0', ' u.`name1`operator1@param1'],
          { param0: {jsonval: 1}, param1: 'stringval'}
        ]

      );
      spyOn(view, 'rerender');
      view.colid = 1;
      view.documentsCount = 0;
      view.pageid = '1';
      view.renderPagination(3, false);

      expect(view.rerender).toHaveBeenCalled();
    });

    it('breadcrumb', function () {
      jQueryDummy = {
        append: function () {}
      };
      spyOn(jQueryDummy, 'append');
      spyOn(window, '$').andReturn(jQueryDummy);

      window.location.hash = '1/2';
      view.breadcrumb();
      expect(view.collectionName).toEqual('2');
      expect(jQueryDummy.append).toHaveBeenCalledWith('<div class="breadcrumb">' +
        '<a class="activeBread" href="#collections">Collections</a>' +
        '  &gt;  ' +
        '<a class="disabledBread">2</a>' +
        '</div>');
      expect(window.$).toHaveBeenCalledWith('#transparentHeader');
    });

    it('resetIndexForms', function () {
      jQueryDummy = {
        val: function () {
          return jQueryDummy;
        },
        prop: function () {}

      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'prop');
      spyOn(window, '$').andReturn(jQueryDummy);

      spyOn(view, 'selectIndexType');
      view.resetIndexForms();
      expect(view.selectIndexType).toHaveBeenCalled();
      expect(jQueryDummy.prop).toHaveBeenCalledWith('checked', false);
      expect(jQueryDummy.prop).toHaveBeenCalledWith('selected', true);
      expect(jQueryDummy.val).toHaveBeenCalledWith('');
      expect(jQueryDummy.val).toHaveBeenCalledWith('Cap');
      expect(window.$).toHaveBeenCalledWith('#indexHeader input');
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
    });

    it('stringToArray', function () {
      expect(view.stringToArray('1,2,3,4,5,')).toEqual(['1', '2', '3', '4', '5']);
    });

    it('createCap Index', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Cap';
          }
          if (jQueryDummy.e === '#newCapSize') {
            return '1024';
          }
          if (jQueryDummy.e === '#newCapByteSize') {
            return 2048;
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;
      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(true);
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
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

    it('create Geo Index', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Geo';
          }
          if (jQueryDummy.e === '#newGeoFields') {
            return '1,2,3,4,5,6';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;
      spyOn(view, 'checkboxToValue').andCallFake(
        function (a) {
          if (a === '#newGeoJson') {
            return 'newGeoJson';
          }
          if (a === '#newGeoConstraint') {
            return 'newGeoConstraint';
          }
          if (a === '#newGeoIgnoreNull') {
            return 'newGeoIgnoreNull';
          }
        }
      );
      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(true);
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'geo',
        fields: ['1', '2', '3', '4', '5', '6'],
        geoJson: 'newGeoJson',
        constraint: 'newGeoConstraint',
        ignoreNull: 'newGeoIgnoreNull'
      });
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newGeoFields');
      expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

      expect(view.getIndex).toHaveBeenCalled();
      expect(view.toggleNewIndexView).toHaveBeenCalled();
      expect(view.resetIndexForms).toHaveBeenCalled();
    });

    it('create Hash Index', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Hash';
          }
          if (jQueryDummy.e === '#newHashFields') {
            return '1,2,3,4,5,6';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(view, 'checkboxToValue').andCallFake(
        function (a) {
          if (a === '#newHashUnique') {
            return 'newHashUnique';
          }
        }
      );

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };

      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(true);
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'hash',
        fields: ['1', '2', '3', '4', '5', '6'],
        unique: 'newHashUnique'
      });
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newHashFields');
      expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

      expect(view.getIndex).toHaveBeenCalled();
      expect(view.toggleNewIndexView).toHaveBeenCalled();
      expect(view.resetIndexForms).toHaveBeenCalled();
    });

    it('create Fulltext Index', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Fulltext';
          }
          if (jQueryDummy.e === '#newFulltextFields') {
            return '1,2,3,4,5,6';
          }
          if (jQueryDummy.e === '#newFulltextMinLength') {
            return '100';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(true);
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'fulltext',
        fields: ['1', '2', '3', '4', '5', '6'],
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

    it('create Skiplist Index', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Skiplist';
          }
          if (jQueryDummy.e === '#newSkiplistFields') {
            return '1,2,3,4,5,6';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(view, 'checkboxToValue').andCallFake(
        function (a) {
          if (a === '#newSkiplistUnique') {
            return 'newSkiplistUnique';
          }
        }
      );

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(true);
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'skiplist',
        fields: ['1', '2', '3', '4', '5', '6'],
        unique: 'newSkiplistUnique'
      });
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
      expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable tr');

      expect(view.getIndex).toHaveBeenCalled();
      expect(view.toggleNewIndexView).toHaveBeenCalled();
      expect(view.resetIndexForms).toHaveBeenCalled();
    });

    it('create Skiplist Index with error and no response text', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Skiplist';
          }
          if (jQueryDummy.e === '#newSkiplistFields') {
            return '1,2,3,4,5,6';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(view, 'checkboxToValue').andCallFake(
        function (a) {
          if (a === '#newSkiplistUnique') {
            return 'newSkiplistUnique';
          }
        }
      );

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(false);
      spyOn(arangoHelper, 'arangoNotification');
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).not.toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'skiplist',
        fields: ['1', '2', '3', '4', '5', '6'],
        unique: 'newSkiplistUnique'
      });

      expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
        'Document error', 'Could not create index.');
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
      expect(window.$).not.toHaveBeenCalledWith('#collectionEditIndexTable tr');

      expect(view.getIndex).not.toHaveBeenCalled();
      expect(view.toggleNewIndexView).not.toHaveBeenCalled();
      expect(view.resetIndexForms).not.toHaveBeenCalled();
    });

    it('create Skiplist Index with error and response text', function () {
      view.collectionName = 'coll';

      spyOn(view, 'getIndex');
      spyOn(view, 'toggleNewIndexView');
      spyOn(view, 'resetIndexForms');

      jQueryDummy = {
        val: function (a) {
          if (jQueryDummy.e === '#newIndexType') {
            return 'Skiplist';
          }
          if (jQueryDummy.e === '#newSkiplistFields') {
            return '1,2,3,4,5,6';
          }
        },
        remove: function () {}
      };
      spyOn(jQueryDummy, 'val').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(window, '$').andCallFake(function (e) {
        jQueryDummy.e = e;
        return jQueryDummy;
      });

      spyOn(view, 'checkboxToValue').andCallFake(
        function (a) {
          if (a === '#newSkiplistUnique') {
            return 'newSkiplistUnique';
          }
        }
      );

      var arangoCollectionsDummy = {
        createIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'createIndex').andReturn(
        {responseText: '{"errorMessage" : "blub"}'});
      spyOn(arangoHelper, 'arangoNotification');
      view.collectionsStore = new window.arangoCollections();

      view.createIndex();

      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.remove).not.toHaveBeenCalled();
      expect(arangoCollectionsDummy.createIndex).toHaveBeenCalledWith({
        type: 'skiplist',
        fields: ['1', '2', '3', '4', '5', '6'],
        unique: 'newSkiplistUnique'
      });

      expect(arangoHelper.arangoNotification).toHaveBeenCalledWith(
        'Document error', 'blub');
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newSkiplistFields');
      expect(window.$).not.toHaveBeenCalledWith('#collectionEditIndexTable tr');

      expect(view.getIndex).not.toHaveBeenCalled();
      expect(view.toggleNewIndexView).not.toHaveBeenCalled();
      expect(view.resetIndexForms).not.toHaveBeenCalled();
    });

    it('prepDeleteIndex', function () {
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
        text: function () {},
        modal: function () {}
      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'first').andCallThrough();
      spyOn(jQueryDummy, 'children').andCallThrough();
      spyOn(jQueryDummy, 'text');
      spyOn(jQueryDummy, 'modal');

      spyOn(window, '$').andReturn(jQueryDummy);

      view.prepDeleteIndex({currentTarget: 'blub'});

      expect(window.$).toHaveBeenCalledWith('blub');
      expect(window.$).toHaveBeenCalledWith('#indexDeleteModal');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(jQueryDummy.first).toHaveBeenCalled();
      expect(jQueryDummy.children).toHaveBeenCalled();
      expect(jQueryDummy.text).toHaveBeenCalled();
      expect(jQueryDummy.modal).toHaveBeenCalledWith('show');
    });

    it('deleteIndex', function () {
      jQueryDummy = {
        parent: function () {
          return jQueryDummy;
        },
        remove: function () {},
        modal: function () {}
      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(jQueryDummy, 'modal');

      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoCollectionsDummy = {
        deleteIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;
      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'deleteIndex').andReturn(true);
      spyOn(arangoHelper, 'arangoError');
      view.collectionsStore = new window.arangoCollections();

      view.lastTarget = {currentTarget: 'blub'};
      view.collectionName = 'coll';
      view.lastId = 'lastId';
      view.deleteIndex();

      expect(window.$).toHaveBeenCalledWith('blub');
      expect(window.$).toHaveBeenCalledWith('#indexDeleteModal');
      expect(arangoCollectionsDummy.deleteIndex).toHaveBeenCalledWith('lastId');
      expect(jQueryDummy.parent).toHaveBeenCalled();
      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
      expect(jQueryDummy.remove).toHaveBeenCalled();
    });

    it('deleteIndex with error', function () {
      jQueryDummy = {
        parent: function () {
          return jQueryDummy;
        },
        remove: function () {},
        modal: function () {}
      };
      spyOn(jQueryDummy, 'parent').andCallThrough();
      spyOn(jQueryDummy, 'remove');
      spyOn(jQueryDummy, 'modal');

      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoCollectionsDummy = {
        deleteIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;

      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'deleteIndex').andReturn(false);
      spyOn(arangoHelper, 'arangoError');
      view.collectionsStore = new window.arangoCollections();

      view.lastTarget = {currentTarget: 'blub'};
      view.collectionName = 'coll';
      view.lastId = 'lastId';
      view.deleteIndex();

      expect(window.$).not.toHaveBeenCalledWith('blub');
      expect(window.$).toHaveBeenCalledWith('#indexDeleteModal');
      expect(arangoCollectionsDummy.deleteIndex).toHaveBeenCalledWith('lastId');
      expect(jQueryDummy.parent).not.toHaveBeenCalled();
      expect(jQueryDummy.modal).toHaveBeenCalledWith('hide');
      expect(jQueryDummy.remove).not.toHaveBeenCalled();
      expect(arangoHelper.arangoError).toHaveBeenCalledWith('Could not delete index');
    });

    it('selectIndexType', function () {
      jQueryDummy = {
        hide: function () {},
        val: function () {},
        show: function () {}
      };
      spyOn(jQueryDummy, 'hide');
      spyOn(jQueryDummy, 'show');
      spyOn(jQueryDummy, 'val').andReturn('newType');

      spyOn(window, '$').andReturn(jQueryDummy);

      view.selectIndexType();

      expect(window.$).toHaveBeenCalledWith('.newIndexClass');
      expect(window.$).toHaveBeenCalledWith('#newIndexType');
      expect(window.$).toHaveBeenCalledWith('#newIndexTypenewType');
      expect(jQueryDummy.hide).toHaveBeenCalled();
      expect(jQueryDummy.val).toHaveBeenCalled();
      expect(jQueryDummy.show).toHaveBeenCalled();
    });

    it('checkboxToValue', function () {
      jQueryDummy = {
        prop: function () {}
      };
      spyOn(jQueryDummy, 'prop');

      spyOn(window, '$').andReturn(jQueryDummy);

      view.checkboxToValue('anId');

      expect(window.$).toHaveBeenCalledWith('anId');
      expect(jQueryDummy.prop).toHaveBeenCalledWith('checked');
    });

    it('getIndex', function () {
      jQueryDummy = {
        append: function () {},
        each: function (list, callback) {
          var callBackWraper = function (a, b) {
            callback(b, a);
          };
          list.forEach(callBackWraper);
        }
      };
      spyOn(jQueryDummy, 'append');

      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoCollectionsDummy = {
        getIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;
      spyOn(arangoHelper, 'fixTooltips');
      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'getIndex').andReturn(
        {
          indexes: [
            {id: 'abc/def1', type: 'primary', name: 'index1'},
            {id: 'abc/def2', type: 'edge', name: 'index2'},
            {id: 'abc/def3', type: 'dummy', name: 'index3', fields: [1, 2, 3]}

          ]
        }
      );
      view.collectionsStore = new window.arangoCollections();
      view.collection = {
        collectionID: 'coll'
      };
      view.getIndex();

      expect(arangoCollectionsDummy.getIndex).toHaveBeenCalledWith();
      expect(view.index).toEqual({
        indexes: [
          {id: 'abc/def1', type: 'primary', name: 'index1'},
          {id: 'abc/def2', type: 'edge', name: 'index2'},
          {id: 'abc/def3', type: 'dummy', name: 'index3', fields: [1, 2, 3]}

        ]
      });
      expect(window.$).toHaveBeenCalledWith('#collectionEditIndexTable');
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

      expect(arangoHelper.fixTooltips).toHaveBeenCalledWith('deleteIndex', 'left');
    });

    it('getIndex with no result', function () {
      jQueryDummy = {
        append: function () {},
        each: function (list, callback) {
          var callBackWraper = function (a, b) {
            callback(b, a);
          };
          list.forEach(callBackWraper);
        }
      };
      spyOn(jQueryDummy, 'append');

      spyOn(window, '$').andReturn(jQueryDummy);

      var arangoCollectionsDummy = {
        getIndex: function () {}

      };
      view.collectionModel = arangoCollectionsDummy;
      spyOn(arangoHelper, 'fixTooltips');
      spyOn(window, 'arangoCollections').andReturn(arangoCollectionsDummy);
      spyOn(arangoCollectionsDummy, 'getIndex').andReturn(undefined);
      view.collectionsStore = new window.arangoCollections();

      view.collection = {
        collectionID: 'coll'
      };
      view.getIndex();

      expect(arangoCollectionsDummy.getIndex).toHaveBeenCalledWith();
      expect(view.index).toEqual(undefined);
      expect(window.$).not.toHaveBeenCalled();
      expect(jQueryDummy.append).not.toHaveBeenCalled();
      expect(arangoHelper.fixTooltips).not.toHaveBeenCalledWith();
    });

    it('rerender', function () {
      spyOn(arangoDocsStoreDummy, 'getDocuments');
      view.rerender();
      expect(arangoDocsStoreDummy.getDocuments).toHaveBeenCalledWith(view.getDocsCallback.bind(view));
    });

    it('setCollectionId', function () {
      view.collection = {
        setCollection: function () {},
        getDocuments: function () {}
      };

      var arangoCollectionsDummy = {
        get: function () {}

      };
      view.collectionsStore = arangoCollectionsDummy;
      spyOn(arangoCollectionsDummy, 'get');
      spyOn(view.collection, 'setCollection');
      spyOn(view.collection, 'getDocuments');
      spyOn(arangoHelper, 'collectionApiType').andReturn('blub');
      view.setCollectionId(1, 2);
      expect(view.collection.setCollection).toHaveBeenCalledWith(1);
      expect(arangoHelper.collectionApiType).toHaveBeenCalledWith(1);
      expect(arangoCollectionsDummy.get).toHaveBeenCalled();
      expect(view.type).toEqual('blub');
      expect(view.pageid).toEqual(2);
      expect(view.collection.getDocuments).toHaveBeenCalledWith(1, 2);
    });

    it('renderPaginationElements', function () {
      view.collection = {
        getTotal: function () {
          return 5;
        }
      };
      spyOn(view.collection, 'getTotal').andCallThrough();

      jQueryDummy = {
        length: 45,
        html: function () {}
      };
      spyOn(jQueryDummy, 'html');

      spyOn(window, '$').andReturn(jQueryDummy);

      spyOn(view, 'renderPagination');

      view.renderPaginationElements();
      expect(window.$).toHaveBeenCalledWith('#totalDocuments');
      expect(view.renderPagination).toHaveBeenCalled();
      expect(view.collection.getTotal).toHaveBeenCalled();
      expect(jQueryDummy.html).toHaveBeenCalledWith(
        'Total: 5 document(s)'
      );
    });

    it('renderPaginationElements with total = 0', function () {
      view.collection = {
        getTotal: function () {
          return 5;
        }
      };
      spyOn(view.collection, 'getTotal').andCallThrough();

      jQueryDummy = {
        length: 0,
        append: function () {
          throw 'Should be a spy';
        },
        html: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(jQueryDummy, 'append');
      spyOn(jQueryDummy, 'html');

      spyOn(window, '$').andReturn(jQueryDummy);

      spyOn(view, 'renderPagination');

      view.renderPaginationElements();
      expect(window.$).toHaveBeenCalledWith('#totalDocuments');
      expect(window.$).toHaveBeenCalledWith('#documentsToolbarFL');
      expect(view.renderPagination).toHaveBeenCalled();
      expect(view.collection.getTotal).toHaveBeenCalled();
      expect(jQueryDummy.append).toHaveBeenCalledWith(
        '<a id="totalDocuments" class="totalDocuments"></a>'
      );
      expect(jQueryDummy.html).toHaveBeenCalledWith(
        'Total: 5 document(s)'
      );
    });

    it('firstPage and lastPage', function () {
      spyOn(view, 'jumpTo');
      view.collection = {
        getLastPageNumber: function () {
          return 3;
        }
      };
      view.firstPage();
      expect(view.jumpTo).toHaveBeenCalledWith(1);
      view.lastPage();
      expect(view.jumpTo).toHaveBeenCalledWith(3);
    });
  });
}());

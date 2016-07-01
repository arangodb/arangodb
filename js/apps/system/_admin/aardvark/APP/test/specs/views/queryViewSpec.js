/* global describe, beforeEach, afterEach, it, spyOn, expect, _, $*/
/* global ace, window, document, localStorage, Joi, jasmine*/

(function () {
  'use strict';

  describe('The query view', function () {
    var view, div, div2, jQueryDummy, queryCollection,
      collectionDummy;

    beforeEach(function () {
      spyOn($, 'ajax');
      window.App = {
        notificationList: {
          add: function () {
            throw 'Should be a spy';
          }
        }
      };

      var store = {};

      spyOn(localStorage, 'getItem').andCallFake(function (key) {
        return store[key];
      });
      spyOn(localStorage, 'setItem').andCallFake(function (key, value) {
        store[key] = value + '';
        return store[key];
      });
      spyOn(localStorage, 'clear').andCallFake(function () {
        store = {};
      });
      spyOn(localStorage, 'removeItem').andCallFake(function (key) {
        delete store[key];
      });

      var DummyModel = function (vals) {
        this.get = function (attr) {
          return vals[attr];
        };
      };

      collectionDummy = {
        list: [],
        fetch: function () {
          throw 'Should be a spy';
        },
        add: function (item) {
          this.list.push(new DummyModel(item));
        },
        each: function (func) {
          return this.list.forEach(func);
        },
        saveCollectionQueries: function () {
          throw 'Should be a spy';
        },
        findWhere: function () {
          throw 'Should ne a spy';
        }
      };
      spyOn(collectionDummy, 'fetch');
      spyOn(collectionDummy, 'saveCollectionQueries');

      spyOn(window.App.notificationList, 'add');

      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);

      queryCollection = {
        __content: [],
        fetch: function () {},
        findWhere: function (ex) {
          var i, k, found;
          for (i = 0; i < this.__content.length; ++i) {
            found = true;
            for (k in ex) {
              if (!ex.hasOwnProperty(k) || !this.__content[i].hasOwnProperty(k) || ex[k] !== this.__content[i][k]) {
                found = false;
              }
            }
            if (found) {
              return this.__content[i];
            }
          }
        },
        each: function (func) {
          return this.__content.forEach(func);
        },
        some: function (func) {
          var res = false, i;
          for (i = 0; i < this.__content.length; ++i) {
            res = res || func(this.__content[i]);
          }
          return res;
        },
        remove: function (obj) {
          var i;
          for (i = 0; i < this.__content.length; ++i) {
            if (this.__content[i] === obj) {
              this.__content.splice(i, 1);
              return;
            }
          }
        },
        add: function (item) {
          this.__content.push(new DummyModel(item));
        },
        saveCollectionQueries: function () {}
      };

      view = new window.queryView({
        collection: queryCollection,
        collection2: collectionDummy
      });

      window.modalView = new window.ModalView();

      spyOn(view, 'getSystemQueries');

      view.render();
    });

    afterEach(function () {
      delete window.App;
      delete window.modalView;
      document.body.removeChild(div);
    });

    it('assert the basics', function () {
      var events = {
        'click #result-switch': 'switchTab',
        'click #query-switch': 'switchTab',
        'click #customs-switch': 'switchTab',
        'click #submitQueryButton': 'submitQuery',
        'click #commentText': 'commentText',
        'click #uncommentText': 'uncommentText',
        'click #undoText': 'undoText',
        'click #redoText': 'redoText',
        'click #smallOutput': 'smallOutput',
        'click #bigOutput': 'bigOutput',
        'click #clearOutput': 'clearOutput',
        'click #clearInput': 'clearInput',
        'click #clearQueryButton': 'clearInput',
        'click #addAQL': 'addAQL',
        'mouseover #querySelect': jasmine.any(Function),
        'change #querySelect': 'importSelected',
        'keypress #aqlEditor': 'aqlShortcuts',
        'click #arangoQueryTable .table-cell0': 'editCustomQuery',
        'click #arangoQueryTable .table-cell1': 'editCustomQuery',
        'click #arangoQueryTable .table-cell2 a': 'deleteAQL',
        'click #confirmQueryImport': 'importCustomQueries',
        'click #confirmQueryExport': 'exportCustomQueries',
        'click #downloadQueryResult': 'downloadQueryResult',
        'click #importQueriesToggle': 'showImportMenu'
      };
      expect(events).toEqual(view.events);
    });

    it('should execute all functions when view initializes', function () {
      spyOn(view, 'getAQL');
      view.initialize();
      expect(view.tableDescription.rows).toEqual(view.customQueries);
      expect(view.getAQL).toHaveBeenCalled();
    });

    it('should create a custom query modal', function () {
      spyOn(window.modalView, 'createTextEntry');
      spyOn(window.modalView, 'createSuccessButton');
      spyOn(window.modalView, 'show');
      view.createCustomQueryModal();
      expect(window.modalView.createTextEntry).toHaveBeenCalledWith(
        'new-query-name', 'Name', '', undefined, undefined, false,
        [
          {
            rule: Joi.string().required(),
            msg: 'No query name given.'
          }
        ]
      );
      expect(window.modalView.createSuccessButton).toHaveBeenCalled();
      expect(window.modalView.show).toHaveBeenCalled();
    });

    it('should create the modal for adding a new custom query', function () {
      spyOn(view, 'createCustomQueryModal');
      spyOn(view, 'checkSaveName');
      view.addAQL();
      expect(view.createCustomQueryModal).toHaveBeenCalled();
      expect(view.checkSaveName).toHaveBeenCalled();
    });

    it('should bind listen event (return keypress) to save aql button', function () {
      var e = {
        keyCode: 13
      };
      spyOn(view, 'saveAQL');
      spyOn(view, 'checkSaveName');
      view.listenKey(e);
      expect(view.saveAQL).toHaveBeenCalledWith(e);
      expect(view.checkSaveName).toHaveBeenCalled();
    });

    it('should get custom queries from local storage if available', function () {
      var customQueries = [{
        name: '123123123',
        value: 'for var yx do something'
      }];
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      view.getAQL();
      expect(localStorage.getItem).toHaveBeenCalledWith('customQueries');
      expect(view.customQueries).toEqual(customQueries);
    });

    it('should clear the editors output', function () {
      view.outputEditor = ace.edit('queryOutput');
      view.outputEditor.setValue('123123');
      view.clearOutput();
      expect(view.outputEditor.getValue()).toEqual('');
    });

    it('should clear the editors input', function () {
      view.inputEditor = ace.edit('aqlEditor');
      view.inputEditor.setValue('123123');
      view.clearInput();
      expect(view.inputEditor.getValue()).toEqual('');
    });

    it('should fold all output editors values', function () {
      view.smallOutput();
    });

    it('should unfold all input editors values', function () {
      view.bigOutput();
    });

    it('should check if ace editor submit working with ctrl key', function () {
      var e = {
        ctrlKey: true,
        metaKey: false,
        keyCode: 13
      };
      spyOn(view, 'submitQuery');
      view.aqlShortcuts(e);
      expect(view.submitQuery).toHaveBeenCalled();
    });

    it('should check if ace editor submit working with cmd key', function () {
      var e = {
        metaKey: true,
        ctrlKey: false,
        keyCode: 13
      };
      spyOn(view, 'submitQuery');
      view.aqlShortcuts(e);
      expect(view.submitQuery).toHaveBeenCalled();
    });

    it('should delete new query string is the string is the default string', function () {
      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);
      $('#new-query-name').val('Insert Query');
      view.checkSaveName();
      expect('').toBe($('#new-query-name').val());
      document.body.removeChild(div2);
    });

    it('should check if the new query string is valid (query name not taken path)', function () {
      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);

      $('#new-query-name').val('myname');

      jQueryDummy = {
        removeClass: function () {
          throw 'Should be a spy';
        },
        addClass: function () {
          throw 'Should be a spy';
        },
        text: function () {
          throw 'Should be a spy';
        },
        val: function () {
          return 'myname';
        }
      };
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'addClass');
      spyOn(jQueryDummy, 'text');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );

      view.checkSaveName();
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('button-warning');
      expect(jQueryDummy.addClass).toHaveBeenCalledWith('button-success');
      expect(jQueryDummy.text).toHaveBeenCalledWith('Save');
      document.body.removeChild(div2);
    });

    it('should check if the new query string is valid (query name not taken path)', function () {
      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);

      $('#new-query-name').val('myname');

      var customQueries = [{
        name: 'myname',
        value: 'for var yx do something'
      }];
      localStorage.setItem('customQueries', JSON.stringify(customQueries));

      view.initialize();
      expect(localStorage.getItem).toHaveBeenCalledWith('customQueries');

      jQueryDummy = {
        removeClass: function () {
          throw 'Should be a spy';
        },
        addClass: function () {
          throw 'Should be a spy';
        },
        text: function () {
          throw 'Should be a spy';
        },
        val: function () {
          return 'myname';
        }
      };
      spyOn(jQueryDummy, 'removeClass');
      spyOn(jQueryDummy, 'addClass');
      spyOn(jQueryDummy, 'text');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );

      view.checkSaveName();
      expect(jQueryDummy.removeClass).toHaveBeenCalledWith('button-success');
      expect(jQueryDummy.addClass).toHaveBeenCalledWith('button-warning');
      expect(jQueryDummy.text).toHaveBeenCalledWith('Update');
      document.body.removeChild(div2);
    });

    it('should select and edit a custom query from table view', function () {
      var customQueries = [{
          name: 'myname',
          value: 'for var yx do something'
        }], e = {
          target: 'dontcare'
      };
      localStorage.setItem('customQueries', JSON.stringify(customQueries));

      spyOn(view, 'switchTab');
      spyOn(view, 'deselect');

      view.initialize();
      view.editCustomQuery(e);

      expect(view.deselect).toHaveBeenCalled();
      expect(view.switchTab).toHaveBeenCalledWith('query-switch');
    });

    it('should delete a custom query', function () {
      var customQueries = [{
          name: 'hallotest',
          value: 'for var yx do something'
        }], e = {
          target: 'dontcare'
      };
      localStorage.setItem('customQueries', JSON.stringify(customQueries));

      view.initialize();

      spyOn(view, 'renderSelectboxes');
      spyOn(view, 'updateTable');

      view.deleteAQL(e);
      expect(view.renderSelectboxes).toHaveBeenCalled();
      expect(view.updateTable).toHaveBeenCalled();
    });

    it('should save a custom query entry with correct name', function () {
      var customQueries = [{
          name: 'hallotest',
          value: 'for var yx do something'
        }], e = {
          target: 'dontcare',
          stopPropagation: function () {throw 'Should be a spy';}
      };
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      view.initialize();

      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);
      $('#new-query-name').val('HelloWorld');

      spyOn(e, 'stopPropagation');
      spyOn(window.modalView, 'hide');
      spyOn(view, 'renderSelectboxes');
      spyOn(queryCollection, 'add');

      view.saveAQL(e);

      expect(e.stopPropagation).toHaveBeenCalled();
      expect(queryCollection.add).toHaveBeenCalled();
      expect(view.renderSelectboxes).toHaveBeenCalled();
      expect(window.modalView.hide).toHaveBeenCalled();
      document.body.removeChild(div2);
    });

    it('should not save a custom query entry with wrong name', function () {
      var customQueries = [{
          name: 'hallotest',
          value: 'for var yx do something'
        }], e = {
          target: 'dontcare',
          stopPropagation: function () {
            throw 'Should be a spy';
          }
      };
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      localStorage.setItem.reset();
      view.initialize();

      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);
      $('#new-query-name').addClass('invalid-input');

      spyOn(e, 'stopPropagation');
      spyOn(window.modalView, 'hide');
      spyOn(view, 'renderSelectboxes');

      view.saveAQL(e);

      expect(e.stopPropagation).toHaveBeenCalled();
      expect(localStorage.setItem).not.toHaveBeenCalled();
      expect(view.renderSelectboxes).not.toHaveBeenCalled();
      expect(window.modalView.hide).not.toHaveBeenCalled();
      document.body.removeChild(div2);
    });

    it('should not save a custom query entry with empty name', function () {
      var customQueries = [{
          name: 'hallotest',
          value: 'for var yx do something'
        }], e = {
          target: 'dontcare',
          stopPropagation: function () {
            throw 'Should be a spy';
          }
      };
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      localStorage.setItem.reset();
      view.initialize();

      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);
      $('#new-query-name').val('');

      spyOn(e, 'stopPropagation');
      spyOn(window.modalView, 'hide');
      spyOn(view, 'renderSelectboxes');

      view.saveAQL(e);

      expect(e.stopPropagation).toHaveBeenCalled();
      expect(localStorage.setItem).not.toHaveBeenCalled();
      expect(view.renderSelectboxes).not.toHaveBeenCalled();
      expect(window.modalView.hide).not.toHaveBeenCalled();
      document.body.removeChild(div2);
    });

    it('should not save a custom query entry with name already taken', function () {
      var e = {
        target: 'dontcare',
        stopPropagation: function () {
          throw 'Should be a spy';
        }
      };
      queryCollection.add({
        name: 'hallotest',
        value: 'fof var yx do somtehing'
      });
      view.initialize();

      div2 = document.createElement('div');
      div2.id = 'new-query-name';
      document.body.appendChild(div2);
      $('#new-query-name').val('hallotest');

      spyOn(e, 'stopPropagation');
      spyOn(window.modalView, 'hide');
      spyOn(view, 'renderSelectboxes');
      spyOn(queryCollection, 'add');

      view.saveAQL(e);

      expect(e.stopPropagation).toHaveBeenCalled();
      expect(queryCollection.add).not.toHaveBeenCalled();
      expect(view.renderSelectboxes).not.toHaveBeenCalled();
      expect(window.modalView.hide).toHaveBeenCalled();
      document.body.removeChild(div2);
    });

    it('should return custom query data by queryName', function () {
      var customQueries = [{
          name: 'hallotest',
          value: 'for var yx do something'
        }],
        returnValue;
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      view.initialize();

      returnValue = view.getCustomQueryValueByName('hallotest');
      expect(returnValue).toEqual('for var yx do something');
    });

    it('should render with custom queries available', function () {
      div2 = document.createElement('div');
      div2.id = 'test123';
      document.body.appendChild(div2);

      localStorage.setItem('customQueries', JSON.stringify(5000));

      view.initialize();
      view.render();
      expect(localStorage.getItem).toHaveBeenCalledWith('customQueries');
      document.body.removeChild(div2);
    });

    it('submit a query and fail without a msg from server', function () {
      // not finished yet
      spyOn(view, 'deselect');
      var old = window.progressView;
      window.progressView = {
        show: function () {}
      };
      spyOn(window.progressView, 'show');
      view.submitQuery();
      expect(view.deselect).toHaveBeenCalled();
      expect(window.progressView.show).toHaveBeenCalled();
      window.progressView = old;
    });

    it('should just run basic functionality of ace editor', function () {
      view.undoText();
      view.redoText();
      view.commentText();
    });

    it('should import the selected custom query (custom query loop in function)', function () {
      div2 = document.createElement('div');
      div2.id = 'findme';
      document.body.appendChild(div2);

      var customQueries = [{
          name: 'findme',
          value: 'for var yx do something'
        }], e = {
          currentTarget: {
            id: 'findme'
          }
      };
      $('#findme').val('findme');
      localStorage.setItem('customQueries', JSON.stringify(customQueries));
      view.initialize();

      view.importSelected(e);
      document.body.removeChild(div2);
    });

    it('should import the selected arango query (arango query loop in function)', function () {
      div2 = document.createElement('div');
      div2.id = 'findme';
      document.body.appendChild(div2);

      var customQueries = [{
          name: 'findme',
          value: 'for var yx do something'
        }], e = {
          currentTarget: {
            id: 'findme'
          }
      };
      $('#findme').val('findme');
      view.queries = customQueries;

      view.importSelected(e);
      document.body.removeChild(div2);
    });

    it('should render the selectboxes for custom and arango queries', function () {
      jQueryDummy = {
        empty: function () {
          throw 'Should be a spy';
        },
        append: function () {
          throw 'Should be a spy';
        }
      };

      spyOn(jQueryDummy, 'empty');
      spyOn(jQueryDummy, 'append');
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      var customQueries = [{
        name: 'findme',
        value: 'for var yx do something'
      }];

      view.queries = customQueries;
      view.customQueries = customQueries;

      spyOn(view, 'sortQueries');
      spyOn(_, 'escape');

      view.renderSelectboxes();

      expect(view.sortQueries).toHaveBeenCalled();
      expect(jQueryDummy.empty).toHaveBeenCalled();
      expect(jQueryDummy.append).toHaveBeenCalled();
      expect(_.escape).toHaveBeenCalled();
    });
  });
}());

/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect, runs, waitsFor*/
/* global _, $, DBSelectionView*/

(function () {
  'use strict';

  describe('DB Selection', function () {
    var view,
      list,
      dbCollection,
      fetched,
      current,
      div;

    beforeEach(function () {
      dbCollection = new window.ArangoDatabase(
        [],
        {shouldFetchUser: true}
      );

      list = ['_system', 'second', 'first'];
      current = new window.CurrentDatabase({
        name: 'first',
        isSystem: false
      });
      _.each(list, function (n) {
        dbCollection.add({name: n});
      });
      fetched = false;
      spyOn(dbCollection, 'fetch');
      spyOn(dbCollection, 'getDatabasesForUser').andReturn(list);
      div = document.createElement('li');
      div.id = 'dbSelect';
      document.body.appendChild(div);
      view = new DBSelectionView(
        {
          collection: dbCollection,
          current: current
        }
      );
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('should select the current db', function () {
      view.render($(div));
      expect($($(div).children()[0]).text()).toEqual('DB: ' + current.get('name') + ' ');
    });

    it('should trigger fetch on collection', function () {
      view.render($(div));
      expect(dbCollection.getDatabasesForUser).toHaveBeenCalled();
    });
  });
}());

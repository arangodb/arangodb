/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('The new logs view', function () {
    var view, allLogs, debugLogs, infoLogs, warnLogs, errLogs, div, fakeCall;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      allLogs = new window.ArangoLogs(
        {upto: true, loglevel: 4}
      );
      debugLogs = new window.ArangoLogs(
        {loglevel: 4}
      );
      infoLogs = new window.ArangoLogs(
        {loglevel: 3}
      );
      warnLogs = new window.ArangoLogs(
        {loglevel: 2}
      );
      errLogs = new window.ArangoLogs(
        {loglevel: 1}
      );

      fakeCall = function () {
        return;
      };

      spyOn(allLogs, 'fetch').andCallFake(function (options) {
        return fakeCall(options);
      });

      view = new window.LogsView({
        logall: allLogs,
        logdebug: debugLogs,
        loginfo: infoLogs,
        logwarning: warnLogs,
        logerror: errLogs
      });

      expect(allLogs.fetch).toHaveBeenCalled();
      view.render();
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('assert basics', function () {
      expect(view.currentLoglevel).toEqual('logall');
      expect(view.id).toEqual('#logContent');
      expect(view.events).toEqual({
        'click #arangoLogTabbar button': 'setActiveLoglevel',
        'click #logTable_first': 'firstPage',
        'click #logTable_last': 'lastPage'
      });
      expect(view.tabbarElements).toEqual({
        id: 'arangoLogTabbar',
        titles: [
          ['Debug', 'logdebug'],
          ['Warning', 'logwarning'],
          ['Error', 'logerror'],
          ['Info', 'loginfo'],
          ['All', 'logall']
        ]
      });
      expect(view.tableDescription).toEqual({
        id: 'arangoLogTable',
        titles: ['Loglevel', 'Date', 'Message'],
        rows: []
      });
    });

    it('check set not same active log level function', function () {
      var button = {
        currentTarget: {
          id: 'logdebug'
        }
      };
      spyOn(view, 'convertModelToJSON');
      view.setActiveLoglevel(button);
      expect(view.convertModelToJSON).toHaveBeenCalled();
      expect(view.currentLoglevel).toBe(button.currentTarget.id);
    });

    it('check set same active log level function', function () {
      var button = {
        currentTarget: {
          id: 'logall'
        }
      };
      spyOn(view, 'convertModelToJSON');
      view.setActiveLoglevel(button);
      expect(view.convertModelToJSON).not.toHaveBeenCalled();
      expect(view.currentLoglevel).toBe(button.currentTarget.id);
    });

    it('check set active log level to loginfo', function () {
      allLogs.fetch.reset();
      spyOn(infoLogs, 'fetch');
      $('#loginfo').click();
      expect(infoLogs.fetch).toHaveBeenCalled();
      expect(allLogs.fetch).not.toHaveBeenCalled();
    });

    it('check set checkModelToJSON function', function () {
      spyOn(view, 'render');
      fakeCall = function (options) {
        return options.success();
      };

      view.convertModelToJSON();
      expect(view.render).toHaveBeenCalled();
    });

    it('check rerender function', function () {
      spyOn(view, 'convertModelToJSON');
      view.rerender();
      expect(view.convertModelToJSON).toHaveBeenCalled();
    });
  });
}());

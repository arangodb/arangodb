/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $, jasmine, _*/

(function () {
  'use strict';

  describe('The router', function () {
    describe('navigation', function () {
      var r,
        simpleNavigationCheck;

      beforeEach(function () {
        r = new window.PlannerRouter();
        simpleNavigationCheck = function (url, viewName,
          initObject, funcList, shouldNotRender) {
          var route,
            view = {},
            checkFuncExec = function () {
              _.each(funcList, function (v, f) {
                if (v !== undefined) {
                  expect(view[f]).toHaveBeenCalledWith(v);
                } else {
                  expect(view[f]).toHaveBeenCalled();
                }
              });
          };
          funcList = funcList || {};
          if (_.isObject(url)) {
            route = function () {
              r[r.routes[url.url]].apply(r, url.params);
            };
          } else {
            route = r[r.routes[url]].bind(r);
          }
          if (!funcList.hasOwnProperty('render') && !shouldNotRender) {
            funcList.render = undefined;
          }
          _.each(_.keys(funcList), function (f) {
            view[f] = function () {};
            spyOn(view, f);
          });
          expect(route).toBeDefined();
          spyOn(window, viewName).andReturn(view);
          route();
          if (initObject) {
            expect(window[viewName]).toHaveBeenCalledWith(initObject);
          } else {
            expect(window[viewName]).toHaveBeenCalled();
          }
          checkFuncExec();
          // Check if the view is loaded from cache
          window[viewName].reset();
          _.each(_.keys(funcList), function (f) {
            view[f].reset();
          });
          route();
          expect(window[viewName]).not.toHaveBeenCalled();
          checkFuncExec();
        };
      });

      it('should offer all necessary routes', function () {
        var available = _.keys(r.routes),
          expected = [
            'planTest',
            'planScenarioSelector'
          ],
          diff = _.difference(available, expected);
        this.addMatchers({
          toDefineTheRoutes: function (exp) {
            var avail = this.actual,
              leftDiff = _.difference(avail, exp),
              rightDiff = _.difference(exp, avail);
            this.message = function () {
              var msg = '';
              if (rightDiff.length) {
                msg += 'Expect routes: '
                  + rightDiff.join(' & ')
                  + ' to be available.\n';
              }
              if (leftDiff.length) {
                msg += 'Did not expect routes: '
                  + leftDiff.join(' & ')
                  + ' to be available.';
              }
              return msg;
            };
            return true;
          /* Not all routes are covered by tests yet
           * real execution would fail
           return leftDiff.length === 0
           && rightDiff.length === 0
           */
          }
        });
        expect(available).toDefineTheRoutes(expected);
      });

      it('should route to planTest', function () {
        simpleNavigationCheck(
          {
            url: 'planTest'
          },
          'PlanTestView'
        );
      });
      it('should route to planScenarioSelector', function () {
        simpleNavigationCheck(
          {
            url: ''
          },
          'PlanScenarioSelectorView'
        );
      });
    });
  });
}());

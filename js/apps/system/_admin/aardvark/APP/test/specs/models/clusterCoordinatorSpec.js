/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('ClusterCoordinator', function () {
    var clusterCoordinator = new window.ClusterCoordinator();
    it('forList', function () {
      expect(clusterCoordinator.forList()).toEqual({
        name: clusterCoordinator.get('name'),
        status: clusterCoordinator.get('status'),
        url: clusterCoordinator.get('url')
      });
    });
  });
}());

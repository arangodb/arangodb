/* jshint unused: false */
/* global window, $, Backbone, document, arangoHelper */

(function () {
  'use strict';
  // We have to start the app only in production mode, not in test mode
  if (!window.hasOwnProperty('TEST_BUILD')) {
    $(document).ajaxSend(function (event, jqxhr, settings) {
      var currentJwt = window.arangoHelper.getCurrentJwt();
      if (currentJwt) {
        jqxhr.setRequestHeader('Authorization', 'bearer ' + currentJwt);
      }
    });

    $.ajaxSetup({
      error: function (x, status, error) {
        if (x.status === 401) {
          // session might be expired. check if jwt is still valid
          arangoHelper.checkJwt();
        }
      }
    });

    $(document).ready(function () {
      window.App = new window.Router();
      Backbone.history.start();
      window.App.handleResize();
    });

    // create only this one global event listener
    $(document).click(function (e) {
      e.stopPropagation();

      // hide user info dropdown if out of focus
      if (!$(e.target).hasClass('subBarDropdown') &&
        !$(e.target).hasClass('dropdown-header') &&
        !$(e.target).hasClass('dropdown-footer') &&
        !$(e.target).hasClass('toggle')) {
        if ($('#userInfo').is(':visible')) {
          $('.subBarDropdown').hide();
        }
      }
    });
  }
}());

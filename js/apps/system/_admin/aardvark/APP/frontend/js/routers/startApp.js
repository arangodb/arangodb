/* jshint unused: false */
/* global window, $, Backbone, document, arangoHelper */

(function () {
  'use strict';
  // We have to start the app only in production mode, not in test mode
  if (!window.hasOwnProperty('TEST_BUILD')) {
    $(document).ajaxSend(function (event, jqxhr, settings) {
      jqxhr.setRequestHeader('X-Arango-Frontend', 'true');
      var currentJwt = window.arangoHelper.getCurrentJwt();
      if (currentJwt) {
        jqxhr.setRequestHeader('Authorization', 'bearer ' + currentJwt);
      }
    });

    function updateOptions(options) {
      const update = { ...options };
      update.headers = {
        ...update.headers,
        'X-Arango-Frontend': 'true'
      };
      var currentJwt = window.arangoHelper.getCurrentJwt();
      if (currentJwt) {
        update.headers = {
          ...update.headers,
          Authorization: 'bearer ' + currentJwt
        };
      }
      return update;
    };
    
    window.arangoFetch = function (url, options) {
      return fetch(url, updateOptions(options));
    };

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

    // create only the following global event listeners
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
      
      // also note that the web UI was actively used
      arangoHelper.noteActivity();
    });

    $('body').on('keyup', function (e) {
      // hide modal dialogs when pressing ESC
      if (e.keyCode === 27) {
        if (window.modalView) {
          window.modalView.hide();
        }
      }
      
      // also note that the web UI was actively used
      arangoHelper.noteActivity();
    });
  }
}());

(function () {
  'use strict';

  window.checkVersion = function () {
    // this checks for version updates
    $.ajax({
      type: 'GET',
      cache: false,
      url: arangoHelper.databaseUrl('/_admin/status?overview=true'),
      contentType: 'application/json',
      processData: false,
      async: true,
      success: function (data) {
        window.frontendConfig.version = data;
        var currentVersion = window.versionHelper.fromString(data.version);

        $('.navbar #currentVersion').html(
          data.version + '<i class="fa fa-exclamation-circle"></i>'
        );
        $('#currentVersion').removeClass('out-of-date');
      }
    });
  };
}());

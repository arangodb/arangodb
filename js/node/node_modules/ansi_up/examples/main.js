// For browser_amd.html
require(['ansi_up',"jquery-1.7.2.min.js"], function(au) {

  var a2h = new au.default;

  var txt  = "\n\n\033[1;33;40m 33;40  \033[1;33;41m 33;41  \033[1;33;42m 33;42  \033[1;33;43m 33;43  \033[1;33;44m 33;44  \033[1;33;45m 33;45  \033[1;33;46m 33;46  \033[1m\033[0\n\n\033[1;33;42m >> Tests OK\n\n"

  $(function () {

    $("#console").html( a2h.ansi_to_html(txt) );

  });

});

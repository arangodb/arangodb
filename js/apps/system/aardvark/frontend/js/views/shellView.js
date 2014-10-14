/*jshint browser: true, evil: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, window, ace, jqconsole, handler, help, location*/
/*global templateEngine*/

(function() {
  "use strict";
  window.shellView = Backbone.View.extend({
    resizing: false,

    el: '#content',

    template: templateEngine.createTemplate("shellView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));

      this.replShell();

      $("#shell_workspace").trigger("resize", [ 150 ]);

      this.resize();

      // evil: the resize event is globally bound to window, but there is
      // no elegant alternative... (is there?)
      var self = this;
      $(window).resize(function () {
        self.resize();
      });

      this.executeJs("start_pretty_print()");

      return this;
    },

    resize: function () {
      // prevent endless recursion
      if (! this.resizing) {
        this.resizing = true;
        var windowHeight = $(window).height() - 250;
        $('#shell_workspace').height(windowHeight);
        this.resizing = false;
      }
    },

    executeJs: function (data) {
      var internal = require("internal");
      try {
        var result = window.eval(data);
        if (result !== undefined) {
          internal.browserOutputBuffer = "";
          internal.printShell.apply(internal.printShell, [ result ]);
          jqconsole.Write('==> ' + internal.browserOutputBuffer + '\n', 'jssuccess');
        }
        internal.browserOutputBuffer = "";
      } catch (e) {
        if (e instanceof internal.ArangoError) {
          jqconsole.Write(e.message + '\n', 'jserror');
        }
        else {
          jqconsole.Write(e.name + ': ' + e.message + '\n', 'jserror');
        }
      }
    },

    replShellPromptHelper: function(command) {
      // Continue line if can't compile the command.
      try {
        var f = new Function(command);
      }
      catch (e) {
        if (/[\[\{\(]$/.test(command)) {
          return 1;
        }
        return 0;
      }
      return false;
    },

    replShellHandlerHelper: function(command) {

    },

    replShell: function () {
      var self = this;
      // Creating the console.
      var internal = require("internal");
      var client = require("org/arangodb/arangosh");
      var header = 'Welcome to arangosh. Copyright (c) ArangoDB GmbH\n';
      window.jqconsole = $('#replShell').jqconsole(header, 'JSH> ', "...>");
      this.executeJs(internal.print(client.HELP));
      // Abort prompt on Ctrl+Z.
      jqconsole.RegisterShortcut('Z', function() {
        jqconsole.AbortPrompt();
        handler();
      });
      // Move to line end Ctrl+E.
      jqconsole.RegisterShortcut('E', function() {
        jqconsole.MoveToEnd();
        handler();
      });
      jqconsole.RegisterMatching('{', '}', 'brace');
      jqconsole.RegisterMatching('(', ')', 'paren');
      jqconsole.RegisterMatching('[', ']', 'bracket');

      // Handle a command.
      var handler = function(command) {
        if (command === 'help') {
          //command = "require(\"arangosh\").HELP";
          command = help();
        }
        if (command === "exit") {
          location.reload();
        }

        self.executeJs(command);
        jqconsole.Prompt(true, handler, self.replShellPromptHelper(command));
      };

      // Initiate the first prompt.
      handler();
    }

  });
}());

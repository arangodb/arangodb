/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, evil: true, es5:true  */
/*global require, exports, Backbone, EJS, $, window, ace, jqconsole, handler, help, location*/

var shellView = Backbone.View.extend({
  resizing: false,

  el: '#content',
  events: {
    'click #editor-run'     : 'submitEditor'
  },

  template: new EJS({url: 'js/templates/shellView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);

    this.replShell();

    $("#shell_workspace").trigger("resize", [ 150 ]);

    this.resize();
    $.gritter.removeAll();

    // evil: the resize event is globally bound to window, but there is
    // no elegant alternative... (is there?)
    var self = this;
    $(window).resize(function () {
      self.resize();
    });

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
    try {
      var internal = require("internal");
      var result = window.eval(data);

      if (result !== undefined) {
        internal.browserOutputBuffer = "";
        internal.printShell.apply(internal.printShell, [ result ]);
        jqconsole.Write('==> ' + internal.browserOutputBuffer + '\n', 'jssuccess');
      }
      internal.browserOutputBuffer = "";

    } catch (e) {
      jqconsole.Write('ReferenceError: ' + e.message + '\n', 'jserror');
    }
  },
  replShell: function () {
    // Creating the console.
    var internal = require("internal");
    var arangodb = require("org/arangodb");
    var client = require("org/arangodb/arangosh");
    var header = 'Welcome to arangosh Copyright (c) triAGENS GmbH.\n';
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

    var that = this;
    // Handle a command.
    var handler = function(command) {
      if (command === 'help') {
        //command = "require(\"arangosh\").HELP";
        command = help();
      }
      if (command === "exit") {
        location.reload();
      }

      that.executeJs(command);
      jqconsole.Prompt(true, handler, function(command) {
        // Continue line if can't compile the command.
        try {
          var test = new Function(command);
        }
        catch (e) {
          if (/[\[\{\(]$/.test(command)) {
            return 1;
          }
            return 0;
          }
          return false;
        });
      };

      // Initiate the first prompt.
      handler();
    },
    evaloutput: function (data) {
      this.executeJs(data);
    }

  });

var shellView = Backbone.View.extend({
  el: '#content',
  events: {
    'click #editor-run'     : 'submitEditor',
    'mouseleave .vsplitbar' : 'renderEditor'
  },

  template: new EJS({url: '/_admin/html/js/templates/shellView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    this.replShell();
    this.editor();
    $("#shell_workspace").splitter({
      dock: true
    });
    $("#shell_workspace").trigger("resize", [ 200 ]);
    $('.vsplitbar').append('<div id="editor-run"><img src="img/right_icon.png"></img></div>');
    $.gritter.removeAll();
    return this;
  },
  renderEditor: function () {
    var editor = ace.edit("editor");
    editor.resize()
  },
  editor: function () {
    var editor = ace.edit("editor");
    editor.getSession().setMode("ace/mode/javascript");
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
  submitEditor: function () {
    var editor = ace.edit("editor");
    var data = editor.getValue();
    
    this.executeJs(data);
    jqconsole.Focus();
  },
  replShell: function () {
    // Creating the console.
    var header = 'Welcome to arangosh Copyright (c) 2012 triAGENS GmbH.\n';
    window.jqconsole = $('#replShell').jqconsole(header, 'JSH> ', "...>");

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
        command = "require(\"arangosh\").HELP";
      }
      if (command === "exit") {
        location.reload();
      }

      that.executeJs(command);
      jqconsole.Prompt(true, handler, function(command) {
        // Continue line if can't compile the command.
        try {
          Function(command);
        } catch (e) {
          if (/[\[\{\(]$/.test(command)) {
            return 1;
          } else {
            return 0;
          }
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

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
  submitEditor: function () {
    var editor = ace.edit("editor");
    var data = editor.getValue();
        try {
          if (window.eval(data) === undefined) {
          }
          else {
            jqconsole.Write('==> ' + window.eval(data) + '\n', 'jssuccess');
          }
        } catch (e) {
          jqconsole.Write('ReferenceError: ' + e.message + '\n', 'jserror');
      }
      jqconsole.Focus();
  },
  replShell: function () {
    // Creating the console.
    var header = 'Welcome to arangosh Copyright (c) 2012 triAGENS GmbH.\n';
    window.jqconsole = $('#replShell').jqconsole(header, 'JSH> ', "hehe");

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
    jqconsole.RegisterMatching('(', ')', 'paran');
    jqconsole.RegisterMatching('[', ']', 'bracket');
    // Handle a command.
    var handler = function(command) {
      if (command === 'help') {
        command = "require(\"arangosh\").HELP";
      }
      if (command === "exit") {
        location.reload();
      }
        try {
          if (window.eval(command) === undefined) {
          }
          else {
            jqconsole.Write('==> ' + window.eval(command) + '\n', 'jssuccess');
          }
        } catch (e) {
          jqconsole.Write('ReferenceError: ' + e.message + '\n', 'jserror');
      }
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
      try {
        var result = eval(data); 
        if (result !== undefined) {
          print(result);
        }
      }
      catch(e) {
        $('#shellContent').append('<p class="shellError">Error:' + e + '</p>');
      }
    }

  });

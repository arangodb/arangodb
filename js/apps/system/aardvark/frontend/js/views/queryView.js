/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global arangoHelper*/

var queryView = Backbone.View.extend({
    el: '#content',

    initialize: function () {
        this.getAQL();
        this.getSystemQueries();
        localStorage.setItem("queryContent", "");
        localStorage.setItem("queryOutput", "");
    },

    events: {
        'click #submitQueryIcon': 'submitQuery',
        'click #submitQueryButton': 'submitQuery',
        'click #commentText': 'commentText',
        'click #uncommentText': 'uncommentText',
        'click #undoText': 'undoText',
        'click #redoText': 'redoText',
        'click #smallOutput': 'smallOutput',
        'click #bigOutput': 'bigOutput',
        'click #clearOutput': 'clearOutput',
        'click #clearInput': 'clearInput',
        'click #addAQL': 'addAQL',
        'click #editAQL': 'editAQL',
        'click #save-new-query': 'saveAQL',
        'click #save-edit-query': 'saveAQL',
        'click #delete-edit-query': 'showDeleteField',
        'click #confirmDeleteQuery': 'deleteAQL',
        'click #abortDeleteQuery': 'hideDeleteField',
        'keydown #new-query-name': 'listenKey',
        'change #queryModalSelect': 'updateEditSelect',
        'change #querySelect': 'importSelected',
        'change #querySize': 'changeSize',
        'keypress #aqlEditor': 'aqlShortcuts'
    },

    listenKey: function (e) {
        if (e.keyCode === 13) {
            this.saveAQL(e);
        }
    },

    clearOutput: function () {
        var outputEditor = ace.edit("queryOutput");
        outputEditor.setValue('');
    },

    clearInput: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.setValue('');
    },

    smallOutput: function () {
        var outputEditor = ace.edit("queryOutput");
        outputEditor.getSession().foldAll();
    },

    bigOutput: function () {
        var outputEditor = ace.edit("queryOutput");
        outputEditor.getSession().unfold();
    },

    aqlShortcuts: function (e) {
        if (e.ctrlKey && e.keyCode === 13) {
            this.submitQuery();
        }
        else if (e.metaKey && !e.ctrlKey && e.keyCode === 13) {
            this.submitQuery();
        }
        else if (e.ctrlKey && e.keyCode === 90) {
            // TODO: undo/redo seems to work even without this. check if can be removed
            this.undoText();
        }
        else if (e.ctrlKey && e.shiftKey && e.keyCode === 90) {
            // TODO: undo/redo seems to work even without this. check if can be removed
            this.redoText();
        }
    },

    queries: [
    ],

    customQueries: [],

    template: new EJS({url: 'js/templates/queryView.ejs'}),

    render: function () {
        $(this.el).html(this.template.text);

        // fill select box with # of results    
        var querySize = 1000;
        if (typeof Storage) {
          if (localStorage.getItem("querySize") > 0) {
            querySize = parseInt(localStorage.getItem("querySize"), 10);
          }
        }

        var sizeBox = $('#querySize');
        sizeBox.empty();
        [ 100, 250, 500, 1000, 2500, 5000 ].forEach(function (value) {
          sizeBox.append('<option value="' + value + '"' + 
                         (querySize === value ? ' selected' : '') + 
                         '>' + value + ' results</option>');
        });

        var outputEditor = ace.edit("queryOutput");
        outputEditor.setReadOnly(true);
        outputEditor.setHighlightActiveLine(false);
        outputEditor.getSession().setMode("ace/mode/json");
        outputEditor.setValue('');

        var inputEditor = ace.edit("aqlEditor");
        inputEditor.getSession().setMode("ace/mode/aql");
        inputEditor.commands.addCommand({
            name: "togglecomment",
            bindKey: {win:"Ctrl-Shift-C", linux:"Ctrl-Shift-C", mac:"Command-Shift-C"},
            exec: function (editor) {
                editor.toggleCommentLines();
            },
            multiSelectAction: "forEach"
        });

        inputEditor.getSession().selection.on('changeCursor', function (e) {
            var inputEditor = ace.edit("aqlEditor");
            var session = inputEditor.getSession();
            var cursor = inputEditor.getCursorPosition();
            var token = session.getTokenAt(cursor.row, cursor.column);
            if (token) {
                if (token.type === "comment") {
                    $("#commentText")
                        .removeClass("arango-icon-comment")
                        .addClass("arango-icon-uncomment")
                        .attr("data-original-title", "Uncomment");
                } else {
                    $("#commentText")
                        .addClass("arango-icon-comment")
                        .removeClass("arango-icon-uncomment")
                        .attr("data-original-title", "Comment");
                }
            }
        });
        $('#queryOutput').resizable({
            handles: "s",
            ghost: true,
            stop: function () {
                setTimeout(function () {
                    var outputEditor = ace.edit("queryOutput");
                    outputEditor.resize();
                }, 200);
            }
        });

        $('#aqlEditor').resizable({
            handles: "s",
            ghost: true,
            //helper: "resizable-helper",
            stop: function () {
                setTimeout(function () {
                    var inputEditor = ace.edit("aqlEditor");
                    inputEditor.resize();
                }, 200);
            }
        });

        arangoHelper.fixTooltips(".queryTooltips, .glyphicon", "top");

        $('#aqlEditor .ace_text-input').focus();
        $.gritter.removeAll();

        if (typeof Storage) {
            var queryContent = localStorage.getItem("queryContent");
            var queryOutput = localStorage.getItem("queryOutput");
            inputEditor.setValue(queryContent);
            outputEditor.setValue(queryOutput);
        }

        var windowHeight = $(window).height() - 250;
        $('#queryOutput').height(windowHeight / 3);
        $('#aqlEditor').height(windowHeight / 2);

        inputEditor.resize();
        outputEditor.resize();

        this.renderSelectboxes();
        this.deselect(outputEditor);
        this.deselect(inputEditor);

        $('#queryDiv').show();
        //outputEditor.setTheme("ace/theme/merbivore_soft");

        return this;
    },

    deselect: function (editor) {
        var current = editor.getSelection();
        var currentRow = current.lead.row;
        var currentColumn = current.lead.column;

        current.setSelectionRange({
            start: {
                row: currentRow,
                column: currentColumn
            },
            end: {
                row: currentRow,
                column: currentColumn
            }
        });

        editor.focus();
    },

    addAQL: function () {
        //render options
        $('#new-query-name').val('');
        $('#new-aql-query').modal('show');
        setTimeout(function () {
            $('#new-query-name').focus();
        }, 500);
    },
    editAQL: function () {
        if (this.customQueries.length === 0) {
            arangoHelper.arangoNotification("No custom queries available");
            return;
        }

        this.hideDeleteField();
        $('#queryDropdown').slideToggle();
        //$('#edit-aql-queries').modal('show');
        this.renderSelectboxes(true);
        this.updateEditSelect();
    },
    getAQL: function () {
        if (localStorage.getItem("customQueries")) {
            this.customQueries = JSON.parse(localStorage.getItem("customQueries"));
        }
    },
    showDeleteField: function () {
        $('#reallyDeleteQueryDiv').show();
    },
    hideDeleteField: function () {
        $('#reallyDeleteQueryDiv').hide();
    },
    deleteAQL: function () {
        var queryName = $('#queryModalSelect').val();
        var tempArray = [];

        $.each(this.customQueries, function (k, v) {
            if (queryName !== v.name) {
                tempArray.push({
                    name: v.name,
                    value: v.value
                });
            }
        });

        this.customQueries = tempArray;
        localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
        $('#edit-aql-queries').modal('hide');
        arangoHelper.arangoNotification("Query deleted");
        this.renderSelectboxes();
    },
    saveAQL: function (e) {

        var self = this;
        var inputEditor = ace.edit("aqlEditor");
        var queryName = $('#new-query-name').val();
        var content = inputEditor.getValue();

        if (e) {
            if (e.target.id === 'save-edit-query') {
                content = $('#edit-aql-textarea').val();
                queryName = $('#queryModalSelect').val();
            }
        }

        if (queryName.trim() === '') {
            arangoHelper.arangoNotification("Illegal query name");
            return;
        }

        //check for already existing entry
        var tempArray = [];
        var quit = false;
        $.each(this.customQueries, function (k, v) {
            if (e.target.id !== 'save-edit-query') {
                if (v.name === queryName) {
                    quit = true;
                    return;
                }
            }
            if (v.name !== queryName) {
                tempArray.push({
                    name: v.name,
                    value: v.value
                });
            }
        });

        if (quit === true) {
            arangoHelper.arangoNotification("Name already taken!");
            return;
        }

        this.customQueries = tempArray;

        this.customQueries.push({
            name: queryName,
            value: content
        });

        $('#new-aql-query').modal('hide');
        $('#edit-aql-queries').modal('hide');

        localStorage.setItem("customQueries", JSON.stringify(this.customQueries));
        arangoHelper.arangoNotification("Query saved");
        this.renderSelectboxes();
    },
    updateEditSelect: function () {
        var value = this.getCustomQueryValueByName($('#queryModalSelect').val());
        $('#edit-aql-textarea').val(value);
        $('#edit-aql-textarea').focus();
    },
    getSystemQueries: function () {
        var self = this;
        $.ajax({
            type: "GET",
            cache: false,
            url: "js/arango/aqltemplates.json",
            contentType: "application/json",
            processData: false,
            async: false,
            success: function (data) {
                self.queries = data;
            },
            error: function (data) {
                arangoHelper.arangoNotification("Error while loading system templates");
            }
        });
    },
    getCustomQueryValueByName: function (qName) {
        var returnVal;
        $.each(this.customQueries, function (k, v) {
            if (qName === v.name) {
                returnVal = v.value;
            }
        });
        return returnVal;
    },
    importSelected: function (e) {
        var inputEditor = ace.edit("aqlEditor");
        $.each(this.queries, function (k, v) {
            if ($('#' + e.currentTarget.id).val() === v.name) {
                inputEditor.setValue(v.value);
            }
        });
        $.each(this.customQueries, function (k, v) {
            if ($('#' + e.currentTarget.id).val() === v.name) {
                inputEditor.setValue(v.value);
            }
        });

        this.deselect(ace.edit("aqlEditor"));
    },
    changeSize: function (e) {
        if (Storage) {
            localStorage.setItem("querySize", parseInt($('#' + e.currentTarget.id).val(), 10));
        }
    },
    renderSelectboxes: function (modal) {
        this.sortQueries();
        var selector = '';
        if (modal === true) {
            selector = '#queryModalSelect';
            $(selector).empty();
            $.each(this.customQueries, function (k, v) {
                $(selector).append('<option id="' + v.name + '">' + v.name + '</option>');
            });
        }
        else {
            selector = '#querySelect';
            $(selector).empty();
            $(selector).append('<option id="emptyquery">Insert Query</option>');

            $(selector).append('<optgroup label="Example queries">');
            $.each(this.queries, function (k, v) {
                $(selector).append('<option id="' + v.name + '">' + v.name + '</option>');
            });
            $(selector).append('</optgroup>');

            if (this.customQueries.length > 0) {
                $(selector).append('<optgroup label="Custom queries">');
                $.each(this.customQueries, function (k, v) {
                    $(selector).append('<option id="' + v.name + '">' + v.name + '</option>');
                });
                $(selector).append('</optgroup>');
            }
        }
    },
    undoText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.undo();
    },
    redoText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.redo();
    },
    commentText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.toggleCommentLines();

//        var value;
//        var newValue;
//        var flag = false;
//        var cursorRange = inputEditor.getSelection().getRange();
//
//        var regExp = new RegExp(/\*\//);
//        var regExp2 = new RegExp(/\/\*/);
//
//        if (cursorRange.end.row === cursorRange.start.row) {
//            //single line comment /* */
//            value = inputEditor.getSession().getLine(cursorRange.start.row);
//            if (value.search(regExp) === -1 && value.search(regExp2) === -1) {
//                newValue = '/*' + value + '*/';
//                flag = true;
//            }
//            else if (value.search(regExp) !== -1 && value.search(regExp2) !== -1) {
//                newValue = value.replace(regExp, '').replace(regExp2, '');
//                flag = true;
//            }
//            if (flag === true) {
//                inputEditor.find(value, {
//                    range: cursorRange
//                });
//                inputEditor.replace(newValue);
//            }
//        }
//        else {
//            //multi line comment
//            value = inputEditor.getSession().getLines(cursorRange.start.row, cursorRange.end.row);
//            var firstString = value[0];
//            var lastString = value[value.length - 1];
//            var newFirstString;
//            var newLastString;
//
//            if (firstString.search(regExp2) === -1 && lastString.search(regExp) === -1) {
//                newFirstString = '/*' + firstString;
//                newLastString = lastString + '*/';
//                flag = true;
//            }
//            else if (firstString.search(regExp2) !== -1 && lastString.search(regExp) !== -1) {
//                newFirstString = firstString.replace(regExp2, '');
//                newLastString = lastString.replace(regExp, '');
//                flag = true;
//            }
//            if (flag === true) {
//                inputEditor.find(firstString, {
//                    range: cursorRange
//                });
//                inputEditor.replace(newFirstString);
//                inputEditor.find(lastString, {
//                    range: cursorRange
//                });
//                inputEditor.replace(newLastString);
//            }
//        }
//        cursorRange.end.column = cursorRange.end.column + 2;
//        inputEditor.getSelection().setSelectionRange(cursorRange, false);
    },
    sortQueries: function () {
        this.queries = _.sortBy(this.queries, 'name');
        this.customQueries = _.sortBy(this.customQueries, 'name');
    },
    submitQuery: function () {
        var self = this;
        var sizeBox = $('#querySize');
        var inputEditor = ace.edit("aqlEditor");
        var data = {
            query: inputEditor.getValue(),
            batchSize: parseInt(sizeBox.val(), 10)
        };
        var outputEditor = ace.edit("queryOutput");

        $.ajax({
            type: "POST",
            url: "/_api/cursor",
            data: JSON.stringify(data),
            contentType: "application/json",
            processData: false,
            success: function (data) {
                outputEditor.setValue(arangoHelper.FormatJSON(data.result));
                if (typeof Storage) {
                    localStorage.setItem("queryContent", inputEditor.getValue());
                    localStorage.setItem("queryOutput", outputEditor.getValue());
                }
                self.deselect(outputEditor);
            },
            error: function (data) {
                try {
                    var temp = JSON.parse(data.responseText);
                    outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);

                    if (typeof Storage) {
                        localStorage.setItem("queryContent", inputEditor.getValue());
                        localStorage.setItem("queryOutput", outputEditor.getValue());
                    }
                }
                catch (e) {
                    outputEditor.setValue('ERROR');
                }
            }
        });
        outputEditor.resize();
        this.deselect(inputEditor);

    }

});

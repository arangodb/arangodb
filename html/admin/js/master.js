///////////////////////////////////////////////////////////////////////////////
/// master.js 
/// arangodb js api  
///////////////////////////////////////////////////////////////////////////////
var welcomeMSG = "" 
+ "                                        _      \n"
+ "   __ _ _ __ __ _ _ __   __ _  ___  ___| |__   \n"
+ "  / _` | '__/ _` | '_ \\ / _` |/ _ \\/ __| '_ \\  \n"
+ " | (_| | | | (_| | | | | (_| | (_) \\__ \\ | | | \n"
+ "  \\__,_|_|  \\__,_|_| |_|\\__, |\\___/|___/_| |_| \n"
+ "                        |___/                  \n"
+ "                                               \n"
+ "Welcome to arangosh Copyright (c) 2012 triAGENS GmbH."


// documents global vars
var collectionCount;
var totalCollectionCount;
var collectionCurrentPage;  
var globalCollectionName;  
var globalCollectionID;
var globalCollectionRev;
var checkCollectionName; 
var printedHelp = false; 
var open = false;
var rowCounter = 0; 
var shArray = []; 

$(document).ready(function() {       
showCursor();

//hide incomplete functions 
$("#uploadFile").attr("disabled", "disabled");
$("#uploadFileImport").attr("disabled", "disabled");
$("#uploadFileSearch").attr("disabled", "disabled");

///////////////////////////////////////////////////////////////////////////////
/// global variables 
///////////////////////////////////////////////////////////////////////////////
var tableView = true;
var sid = ($.cookie("sid")); 
var currentUser; 
//logtable vars
var currentPage = 1; 
var currentAmount; 
var currentTableID = "#logTableID"; 
var currentLoglevel = 5;  
//live click for all log tables 

var tables = ["#logTableID", "#critLogTableID", "#warnLogTableID", "#infoLogTableID", "#debugLogTableID"];

$.each(tables, function(v, i ) {
  $(i + '_prev').live('click', function () {

    if ( i == "#logTableID" ) {
      createNextPagination("all");  
    }
    else {
      createNextPagination();  
    }
  });
  $(i + '_next').live('click', function () {
    if ( i == "#logTableID" ) {
      createPrevPagination("all");  
    }
    else {
      createPrevPagination();  
    }
  });
  $(i + '_last').live('click', function () {
    createLogTable(currentLoglevel);
  });
  $(i+ '_first').live('click', function () {
    createLastLogPagination(i); 
    
  });
});


$("#documents_prev").live('click', function () {
  createPrevDocPagination();
});

$("#documents_next").live('click', function () {
  createNextDocPagination();
});

$("#documents_first").live('click', function () {
  createFirstPagination("#documentsTable"); 
});

$("#documents_last").live('click', function () {
  createLastPagination("#documentsTable"); 
});
///////////////////////////////////////////////////////////////////////////////
/// html customizations  
///////////////////////////////////////////////////////////////////////////////
$('#logView ul').append('<button class="enabled" id="refreshLogButton"><img src="/_admin/html/media/icons/refresh_icon16.png" width=16 height=16></button><div id=tab_right align=right><form><input type="text" id="logSearchField" placeholder="Search..."></input><button id="submitLogSearch" style="visibility:hidden;"></button></form></div>');

///////////////////////////////////////////////////////////////////////////////
/// initialize jquery tabs 
///////////////////////////////////////////////////////////////////////////////

$("#tabs").tabs({
  select: function(event, ui) {
    if (ui.index == 0) {
      currentLoglevel = 5; 
      createLogTable(5); 
    }
    else {
      currentLoglevel = ui.index; 
      createLogTable(ui.index); 
    }
  }    
});

///////////////////////////////////////////////////////////////////////////////
/// disable grey'd out buttons
///////////////////////////////////////////////////////////////////////////////
  $(".nofunction").attr("disabled", "true");

///////////////////////////////////////////////////////////////////////////////
/// checks for a login user cookie, creates new sessions if null  
///////////////////////////////////////////////////////////////////////////////

if ($.cookie("sid") == null) {
  $('#logoutButton').hide();
  $('#movetologinButton').show();
  $.ajax({
    type: "POST",
    url: "/_admin/user-manager/session/",
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      $.cookie("sid", data.sid);
      },
      error: function(data) {
      }
  });
}

///////////////////////////////////////////////////////////////////////////////
/// if user exists, then:   
///////////////////////////////////////////////////////////////////////////////

if ($.cookie("rights") != null || $.cookie("user") != null) {
  $('#loginWindow').hide();
  $('#movetologinButton').hide();
  $('#logoutButton').show();
  $('#activeUser').text($.cookie("user") + '!'); 
  currentUser = $.cookie("user"); 
}
else {
  $('#logoutButton').hide();
}

///////////////////////////////////////////////////////////////////////////////
/// draws collection table   
///////////////////////////////////////////////////////////////////////////////

var collectionTable = $('#collectionsTableID').dataTable({
    "aaSorting": [[ 2, "desc" ]],
    "bPaginate": false, 
    "bFilter": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": false, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{"sWidth":"150px", "bSortable":false}, {"sWidth": "200px"}, {"sWidth": "200px"}, null, {"sWidth": "200px"}, {"sWidth": "200px"} ],
    "aoColumnDefs": [{ "sClass": "alignRight", "aTargets": [ 4, 5 ] }],
    "oLanguage": {"sEmptyTable": "No collections"}
});

///////////////////////////////////////////////////////////////////////////////
/// draws the document edit table 
///////////////////////////////////////////////////////////////////////////////

var documentEditTable = $('#documentEditTableID').dataTable({
    "aaSorting": [[ 1, "desc" ]],
    "bAutoWidth": false, 
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{"sClass":"read_only", "bSortable": false, "sWidth": "30px"}, 
                  {"sClass":"writeable", "bSortable": false, "sWidth":"400px" }, 
                  {"sClass":"writeable", "bSortable": false},
                  {"bVisible": false } ], 
    "oLanguage": {"sEmptyTable": "No documents"}
});

///////////////////////////////////////////////////////////////////////////////
/// draws new doc view table 
///////////////////////////////////////////////////////////////////////////////

var newDocumentTable = $('#NewDocumentTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sClass":"read_only","bSortable": false, "sWidth": "30px"}, 
                  {"sClass":"writeable", "bSortable": false, "sWidth":"250px" }, 
                  {"sClass":"writeable", "bSortable": false },
                  {"bVisible": false } ] 
  });

///////////////////////////////////////////////////////////////////////////////
/// draws documents table  
///////////////////////////////////////////////////////////////////////////////

var documentsTable = $('#documentsTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": false, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"read_only", "bSortable": false, "sWidth":"80px"}, 
                 { "sClass":"read_only","bSortable": false, "sWidth": "200px"}, 
                 { "sClass":"read_only","bSortable": false, "sWidth": "100px"},  
                 { "bSortable": false, "sClass": "cuttedContent"}],
    "oLanguage": { "sEmptyTable": "No documents"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws crit log table  
///////////////////////////////////////////////////////////////////////////////

var critLogTable = $('#critLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No critical logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws warn log table 
///////////////////////////////////////////////////////////////////////////////

var warnLogTable = $('#warnLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}],
    "oLanguage": {"sEmptyTable": "No warning logfiles available"}
  });
///////////////////////////////////////////////////////////////////////////////
/// draws info log table 
///////////////////////////////////////////////////////////////////////////////

var infoLogTable = $('#infoLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}],
    "oLanguage": {"sEmptyTable": "No info logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws debug log table
///////////////////////////////////////////////////////////////////////////////

var debugLogTable = $('#debugLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No debug logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws "all" log table 
///////////////////////////////////////////////////////////////////////////////

var logTable = $('#logTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// creates layout using jquery ui   
///////////////////////////////////////////////////////////////////////////////

  $('body').layout({                                                                                               
    closable: false,             
    resizable: false,                                                                 
    applyDefaultStyles: false,   
    north__spacing_open:0,                                   
    north__spacing_closed: 0,                                                                           
    center__spacing_open:0,                                                      
    center__spacing_open:0, 
    south__spacing_closed: 0,  
    south__spacing_closed: 0 
  });

///////////////////////////////////////////////////////////////////////////////
/// location check    
///////////////////////////////////////////////////////////////////////////////

  $.address.change(function(event) {  

///////////////////////////////////////////////////////////////////////////////
/// Home   
///////////////////////////////////////////////////////////////////////////////

    if (location.hash == '' || location.hash =='#') {
      drawCollectionsTable();
      $('#subCenterView').hide();
      $('#centerView').show();
      $('#collectionsView').show(); 
      createnav("Collections"); 
      highlightNav("#nav1");
    }

///////////////////////////////////////////////////////////////////////////////
/// new document table view (collection) 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 12) == "#collection?" ) {
      $("#addNewDocButton").removeAttr("disabled");
      tableView = true; 
      $('#toggleNewDocButtonText').text('Edit Source'); 
      
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split("=");
      
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collID[0],
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text(collectionName);
          $('#nav2').attr('href', '#showCollection?' +collID[0]);
          $('#nav1').attr('class', 'arrowbg');
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections'); 
      $('#nav1').attr('href', '#');
      $('#nav2').attr('class', 'arrowbg');
      $('#nav3').text('new document'); 
      highlightNav("#nav3");

      newDocumentTable.fnClearTable(); 
      documentTableMakeEditable('#NewDocumentTableID');
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#newDocumentView').show();
      $('#NewDocumentTableView').show();
      $('#NewDocumentSourceView').hide();
    }

///////////////////////////////////////////////////////////////////////////////
///  showe edit documents view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 14) == "#editDocument?") {
      tableView = true; 
      $("#addEditedDocRowButton").removeAttr("disabled");
      $('#toggleEditedDocButton').val('Edit Source'); 
      $('#toggleEditedDocButtonText').text('Edit Source'); 
      
      $('#documentEditSourceView').hide();
      $('#documentEditTableView').show();
      var collectiondocID = location.hash.substr(14, location.hash.length); 
      collectionID = collectiondocID.split("/"); 

      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text(collectionName);
          $('#nav2').attr('href', '#showCollection?' +collectionID[0]);
        },
        error: function(data) {
        }
      });
 
      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav1').attr('class', 'arrowbg');
      $('#nav2').attr('class', 'arrowbg');
      $('#nav3').text('Edit document:' + collectionID[1]); 

      $.ajax({
        type: "GET",
        url: "/_api/document/" + collectiondocID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          $('#documentEditSourceBox').val(JSON.stringify(data));  
          documentEditTable.fnClearTable(); 
          hideAllSubDivs();
          $('#collectionsView').hide();
          $('#documentEditView').show();
          $('#documentEditSourceView').hide();
          $.each(data, function(key, val) {
            if (key == '_id') {
              documentEditTable.fnAddData(["", key, val, JSON.stringify(val)]);
            }
            else if (key == '_rev') {
              documentEditTable.fnAddData(["", key, val, JSON.stringify(val)]);
            }
            else if (key != '_rev' && key != '_id') {
              documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), JSON.stringify(val)]);
            }
          });
          documentTableMakeEditable('#documentEditTableID');
          showCursor();
        },
        error: function(data) {
        }
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  show colletions documents view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16) == "#showCollection?") {
      $('#nav1').removeClass('highlighted'); 
      var collectionID = location.hash.substr(16, location.hash.length); 
      globalAjaxCursorChange();
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID + "/count", 
        contentType: "application/json",
        processData: false,
        async: false,  
        success: function(data) {
          globalCollectionName = data.name;
          test = data.name; 
          collectionCount = data.count; 
          $('#nav2').text(globalCollectionName);
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav2').attr('href', null);
      $('#nav3').text(''); 
      highlightNav("#nav2");
      $("#nav3").removeClass("arrowbg");
      $("#nav2").removeClass("arrowbg");
      $("#nav1").addClass("arrowbg");

      $.ajax({
        type: 'PUT',
        url: '/_api/simple/all/',
        data: '{"collection":"' + globalCollectionName + '","skip":0,"limit":10}', 
        contentType: "application/json",
        success: function(data) {
          $.each(data.result, function(k, v) {
            documentsTable.fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class="prettify">' + cutByResolution(JSON.stringify(v)) + "</pre>"]);  
          });
        $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
        showCursor();
        },
        error: function(data) {
          
        }
      });
      documentsTable.fnClearTable(); 
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#documentsView').show();
      totalCollectionCount = Math.ceil(collectionCount / 10); 
      collectionCurrentPage = 1;
      $('#documents_status').text("Showing page 1 of " + totalCollectionCount); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows edit collection view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16)  == "#editCollection?") {
      var collectionID = location.hash.substr(16, location.hash.length); 
      var collectionName;
      var tmpStatus; 
 
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text('Edit: ' + collectionName);
          $('#editCollectionName').val(data.name); 
          $('#editCollectionID').text(data.id);

          switch (data.status) {
          case 1: tmpStatus = "new born collection"; break; 
          case 2: tmpStatus = "unloaded"; break; 
          case 3: tmpStatus = "loaded"; break; 
          case 4: tmpStatus = "in the process of being unloaded"; break; 
          case 5: tmpStatus = "deleted"; break; 
          }

          $('#editCollectionStatus').text(tmpStatus); 
          checkCollectionName = collectionName; 
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav1').attr('class', 'arrowbg');
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#editCollectionView').show();
      $('#editCollectionName').focus();
    }

///////////////////////////////////////////////////////////////////////////////
///  shows log view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#logs") {
      createLogTable(5); 
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#logView').show();
      createnav ("Logs"); 
      showCursor();
    }

///////////////////////////////////////////////////////////////////////////////
///  shows status view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#status") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#statusView').show();
      createnav ("Status"); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows config view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#config") {
      hideAllSubDivs();
      $('#configContent').empty();  
      $('#collectionsView').hide();
      $('#configView').show();
      createnav ("Configuration"); 
      var switcher = "primary";
      var insertType;  

      $.ajax({
        type: "GET", url: "/_admin/config/description",contentType: "application/json", processData: false, async: false,   
        success: function(data) {
          $.each(data, function(key, val) {
            if (key == "error" || key == "code") {
            }
            else {
              $('#configContent').append('<div class="customToolbar">' + val.name + '</div>');
              $.each(val, function(k, v) {
                if (v.name != undefined) {
                  switch(v.type) { 
                    case 'integer':
                      if (v.readonly == true) {
                        insertType = '<a class="conf_integer" id="conf_' + k + '">123456</a>'; break;
                      }
                      else { 
                        insertType = '<a class="conf_integer editInt" id="conf_' + k + '">123456</a>'; break;
                      }
                    case 'string':
                      if (v.readonly == true) {  
                        insertType = '<a class="conf_string" id="conf_' + k + '">string</a>'; break;
                      }
                      else {
                        insertType = '<a class="editString conf_string" id="conf_' + k + '">string</a>'; break;
                      }
                    case 'pull-down':
                      insertType = '<select class="conf_pulldown" id="conf_' + k + '" name="someselect" size="1">';
                      $.each(v.values, function(KEY, VAL) {
                        insertType += '<option>' + VAL + '</option>';
                      }); 
                      insertType += '</select>';
                      break; 
                    case 'boolean': 
                      insertType = '<select class="conf_boolean" id="conf_' + k + '" name="someselect" size="1">';
                      insertType += '<option>true</option><option>false</option>'; break;
                    //TODO Section 
                    case 'section':
                      insertType = '<a class="conf_section" id="conf_' + k + '">someval</a>'; break;
                  } 
                  $('#configContent').append('<tr><td>' + v.name + '</td><td>' + insertType + '</td></tr>');
                  makeStringEditable(); 
                  makeIntEditable(); 
                }
              });
            }
          });
          $.ajax({
            type: "GET", url: "/_admin/config/configuration",contentType: "application/json", processData:false, async:false, 
            success: function(data) {
              var currentID;
              var currentClass;  
              $.each(data, function(key, val) {
                if (key == "error" || key == "code") {
                }
                else {
                  $.each(val, function(k, v) {
                    currentID = "#conf_" + k; 
                    currentClass = $(currentID).attr('class');

                    if ($(currentID).hasClass('conf_string')) {
                      $(currentID).text(v.value);  
                    }
                    else if ($(currentID).hasClass('conf_integer')) {
                      $(currentID).text(v.value);  
                    }
                    else if ($(currentID).hasClass('conf_boolean')) {
                      $(currentID).val(v.value);  
                    }
                    else if ($(currentID).hasClass('conf_pulldown')) {
                      $(currentID).val(v.value);  
                    }
                    //TODO Section 
                    else if ($(currentID).hasClass('conf_section')) {
                      $(currentID).text(v.file.value);  
                    }


                  }); 
                }
              });
            },
            error: function(data) {
            }
          });
        },
        error: function(data) {
        }
      });
/* 
      var content={"Menue":{"Haha":"wert1", "ahha":"wert2"}, "Favoriten":{"top10":"content"},"Test":{"testing":"hallo 123 test"}}; 
      $("#configContent").empty();

      $.each(content, function(data) {
        $('#configContent').append('<div class="customToolbar">' + data + '</div>');
        $.each(content[data], function(key, val) {
          if (switcher == "primary") {
            $('#configContent').append('<a class="toolbar_left toolbar_primary">' + key + '</a><a class="toolbar_right toolbar_primary">' + val + '</a><br>');
          switcher = "secondary"; 
          }
          else if (switcher == "secondary") {
            $('#configContent').append('<a class="toolbar_left toolbar_secondary">' + key + '</a><a class="toolbar_right toolbar_secondary">' + val + '</a><br>');
          switcher = "primary"; 
          }
        });         
      }); 
*/
    }

///////////////////////////////////////////////////////////////////////////////
///  shows query view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#query") {
      hideAllSubDivs(); 
      $('#queryContent').val('');
      $('#collectionsView').hide();
      $('#queryView').show();
      createnav ("Query"); 
      $('#queryContent').focus();
    }

///////////////////////////////////////////////////////////////////////////////
///  shows avocsh view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#avocsh") {
      hideAllSubDivs(); 
      $('#avocshContent').val('');
      $('#collectionsView').hide();
      $('#avocshView').show();
      createnav ("ArangoDB Shell"); 
      $('#avocshContent').focus();
      if (printedHelp === false) {
        print(welcomeMSG + require("arangosh").HELP);
        printedHelp = true; 
        start_pretty_print(); 
      }
      $("#avocshContent").autocomplete({
        source: shArray
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  shows create new collection view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#createCollection") {
      $('#nav1').attr('href', '#'); 
      $('#nav1').text('Collections');
      $('#nav2').text('Create new collection');
      $('#nav1').attr('class', 'arrowbg'); 

      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#createCollectionView').show();
      $('#createCollName').focus();
      $('#createCollName').val('');
      $('#createCollSize').val('');
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves edited document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedDocButton').live('click', function () {
    
    if (tableView == true) {
      var data = documentEditTable.fnGetData();
      var result = {}; 
      var collectionID; 

      for (row in data) {
        var row_data = data[row];
        if ( row_data[1] == "_id" ) {
          collectionID = row_data[3]; 
        } 
        else {
          result[row_data[1]] = JSON.parse(row_data[3]);
        }
        
      }

      $.ajax({
        type: "PUT",
        url: "/_api/document/" + JSON.parse(collectionID),
        data: JSON.stringify(result), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          var collID = JSON.parse(collectionID).split("/");  
          window.location.href = "#showCollection?" + collID[0];  
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
    else {
      try {
        var collectionID; 
        var boxContent = $('#documentEditSourceBox').val();
        collectionID = globalCollectionID;  
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 

        $.ajax({
          type: "PUT",
          url: "/_api/document/" + collectionID,
          data: JSON.stringify(parsedContent), 
          contentType: "application/json",
          processData: false, 
          success: function(data) {
            tableView = true;
            $('#toggleEditedDocButton').val('Edit Source'); 
            var collID = collectionID.split("/");  
            window.location.href = "#showCollection?" + collID[0];  
          },
          error: function(data) {
           alert(JSON.stringify(data)); 
          }
        });
      }
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// live click function for refreshLogButton 
///////////////////////////////////////////////////////////////////////////////

  $('#refreshLogButton').live('click', function () {
    var selected = $("#tabs").tabs( "option", "selected" ); 
    switch (selected) {
    case 0:
      createLogTable(5);
      break; 
    default:
      createLogTable(selected);
      break;
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// adds a new row in edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#addEditedDocRowButton').live('click', function () {
    if (tableView == true) {
      rowCounter = rowCounter + 1; 
      documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>', "somekey" + rowCounter, value2html("editme"), JSON.stringify("editme")]);
      documentTableMakeEditable('#documentEditTableID');
      showCursor();
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves a new created document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveNewDocButton').live('click', function () {
    
    if (tableView == true) {
      var data = newDocumentTable.fnGetData();
      var result = {}; 
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split("="); 

      for (row in data) {
        var row_data = data[row];
        result[row_data[1]] = JSON.parse(row_data[3]);
      }

      $.ajax({
        type: "POST",
        url: "/_api/document?collection=" + collID, 
        data: JSON.stringify(result), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          window.location.href = "#showCollection?" + collID;  
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
    else {

      try {
        var collectionID = location.hash.substr(12, location.hash.length); 
        var collID = collectionID.split("="); 
        var boxContent = $('#NewDocumentSourceBox').val();
        var jsonContent = JSON.parse(boxContent);
        $.each(jsonContent, function(row1, row2) {
          if ( row1 == '_id') {
            collectionID = row2; 
          } 
        });

        $.ajax({
          type: "POST",
          url: "/_api/document?collection=" + collID, 
          data: JSON.stringify(jsonContent), 
          contentType: "application/json",
          processData: false, 
          success: function(data) {
            tableView = true;
            window.location.href = "#showCollection?" + collID;  
          },
          error: function(data) {
            alert(JSON.stringify(data)); 
          }
        });
      }
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// adds a new row in new document view  
///////////////////////////////////////////////////////////////////////////////

  $('#addNewDocButton').live('click', function () {
    if (tableView == true) {
      rowCounter = rowCounter + 1; 
      newDocumentTable.fnAddData(['<button class="enabled" id="deleteNewDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>', "somekey" + rowCounter, value2html("editme"), JSON.stringify("editme")]);
      documentTableMakeEditable('#NewDocumentTableID');
      showCursor();
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// toggle button for source / table - edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#toggleEditedDocButton').live('click', function () {
    if (tableView == true) {
        var content = documentEditTable.fnGetData();
        var result = {}; 
 
        for (row in content) {
          var row_data = content[row];
          result[row_data[1]] = JSON.parse(row_data[3]);
        }
        
        globalCollectionRev = result._rev; 
        globalCollectionID = result._id; 
        delete result._id;
        delete result._rev;

        var myFormattedString = FormatJSON(result);
        $('#documentEditSourceBox').val(myFormattedString); 
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = false; 
        $('#toggleEditedDocButtonText').text('Edit Table'); 
        $("#addEditedDocRowButton").attr("disabled", "disabled");
    }
    else {
      try {
        var boxContent = $('#documentEditSourceBox').val(); 
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 
        documentEditTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
            documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"</button>',key, value2html(val), JSON.stringify(val)]);
          });
            documentEditTable.fnAddData(['', "_id", globalCollectionID, JSON.stringify(globalCollectionID)]);
            documentEditTable.fnAddData(['', "_rev", globalCollectionRev, JSON.stringify(globalCollectionRev)]);
  
        documentTableMakeEditable ('#documentEditTableID'); 
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = true; 
        $('#toggleEditedDocButtonText').text('Edit Source'); 
        $("#addEditedDocRowButton").removeAttr("disabled");
      }
 
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// cancel editing a doc  
///////////////////////////////////////////////////////////////////////////////

  $('#cancelEditedDocument').live('click', function () {
    var start = window.location.hash.indexOf('?'); 
    var end = window.location.hash.indexOf('/'); 
    var collectionID = window.location.hash.substring(start + 1, end); 
    window.location.href = "#showCollection?" + collectionID; 
    $('#nav1').removeClass('highlighted'); 
  });

///////////////////////////////////////////////////////////////////////////////
/// undo live changes of an edited document  
///////////////////////////////////////////////////////////////////////////////

  $('#undoEditedDocument').live('click', function () {
    location.reload();  
  });

///////////////////////////////////////////////////////////////////////////////
/// perform logout 
///////////////////////////////////////////////////////////////////////////////

  $('#logoutButton').live('click', function () {
      $.ajax({
        type: "PUT",
        url: '/_admin/user-manager/session/' + sid + '/logout',
        data: JSON.stringify({'user': currentUser}), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          $('#loginWindow').show();
          $('#logoutButton').hide();
          $('#movetologinButton').show();
          $('#activeUser').text('Guest!');  
          $.cookie("rights", null);
          $.cookie("user", null);
          currentUser = null;
          window.location.href = ""; 
        },
        error: function(data) {
        }
      });
  });

///////////////////////////////////////////////////////////////////////////////
/// perform login  
///////////////////////////////////////////////////////////////////////////////

  $('#loginButton').live('click', function () {
    var username = $('#usernameField').val();
    var password = $('#passwordField').val();
    var shapassword;

    if (password != '') {
      shapassword = $.sha256(password);   
    }
    else {
      shapassword = password;
    }

    $.ajax({
      type: "PUT",
      url: '/_admin/user-manager/session/' + sid + '/login',
      data: JSON.stringify({'user': username, 'password': shapassword}), 
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        currentUser = $.cookie("user"); 
        $('#loginWindow').hide();
        $('#logoutButton').show();
        $('#movetologinButton').hide();
        $('#activeUser').text(username + '!');  
        $.cookie("rights", data.rights);
        $.cookie("user", data.user);

        /*animation*/
        $('#movetologinButton').text("Login");
        $('#footerSlideContent').animate({ height: '25px' });
        $('#footerSlideButton').css('backgroundPosition', 'top left');
        open = false;

        return false; 
      },
      error: function(data) {
        alert(JSON.stringify(data)); 
        return false; 
      }
    });
    return false; 
  });

///////////////////////////////////////////////////////////////////////////////
/// toggle button for source / table - new document view 
///////////////////////////////////////////////////////////////////////////////
  
  $('#toggleNewDocButton').live('click', function () {
    if (tableView == true) {
      try {
        var content = newDocumentTable.fnGetData();
        var result = {}; 
 
        for (row in content) {
          var row_data = content[row];
          result[row_data[1]] = JSON.parse(row_data[3]);
        }

        var myFormattedString = FormatJSON(result)
        $('#NewDocumentSourceBox').val(myFormattedString); 
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = false; 
        $('#toggleNewDocButtonText').text('Edit Table'); 
        $("#addNewDocButton").attr("disabled", "disabled");
      }
  
      catch(e) {
        alert(e); 
      }
    }
    else {
      try {
        var boxContent = $('#NewDocumentSourceBox').val();  
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 

        newDocumentTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
          if (key == '_id' || key == '_rev') {
            newDocumentTable.fnAddData(["", key, value2html(val), JSON.stringify(val)]);
          }
          else {
              newDocumentTable.fnAddData(['<button class="enabled" id="deleteNewDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), JSON.stringify(val)]);
          }
        });
        documentTableMakeEditable ('#NewDocumentTableID'); 
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = true; 
        $('#toggleNewDocButtonText').text('Edit Source'); 
        $("#addNewDocButton").removeAttr("disabled");
      }
 
      catch(e) {
        alert(e); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// cancel new doc view 
///////////////////////////////////////////////////////////////////////////////

  $('#cancelNewDocument').live('click', function () {
    var start = window.location.hash.indexOf('?'); 
    var end = window.location.hash.indexOf('=');  
    var collectionID = window.location.hash.substring(start + 1, end); 
    window.location.href = "#showCollection?" + collectionID; 
    $('#nav1').removeClass('highlighted'); 
  });

///////////////////////////////////////////////////////////////////////////////
/// deletes a row in edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#deleteEditedDocButton').live('click', function () {
    var row = $(this).closest("tr").get(0);
    documentEditTable.fnDeleteRow(documentEditTable.fnGetPosition(row));
  });
///////////////////////////////////////////////////////////////////////////////
/// submit log search 
///////////////////////////////////////////////////////////////////////////////

  $('#submitLogSearch').live('click', function () {  

    hideLogPagination(); 
    var content = $('#logSearchField').val();
    var selected = $("#tabs").tabs( "option", "selected" ); 
    
    var currentTableID; 
    if (selected == 1) {currentTableID = "#critLogTableID";  } 
    else if (selected == 2) {currentTableID = "#warnLogTableID";} 
    else if (selected == 3) {currentTableID = "#infoLogTableID";} 
    else if (selected == 4) {currentTableID = "#debugLogTableID";} 
 
    switch (selected) {
    case 0:
      if(content == '') {
        createLogTable(5); 
      }
      else {
        $('#logTableID').dataTable().fnClearTable();
        $('#logTableID_status').text('Showing all entries for: "' + content + '"'); 
        $.getJSON("/_admin/log?search=" + content + "&upto=5", function(data) {
          var totalAmount = data.totalAmount; 
          var items=[];
          var i=0; 
     
          $.each(data.lid, function () {
            $('#logTableID').dataTable().fnAddData([data.level[i], data.text[i]]);
            i++;
          });
        });
      }
      break; 
      default:
        if(content == '') {
          createLogTable(selected); 
        } 
        else { 
          $(currentTableID).dataTable().fnClearTable();
          $(currentTableID + '_status').text('Showing all entries for: "' + content + '"'); 
          $.getJSON("/_admin/log?search=" + content + "&level=" + selected, function(data) {
            var totalAmount = data.totalAmount; 
            var items=[];
            var i=0; 
            $.each(data.lid, function () {
              $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]);
              i++;
            });
          });
        }
    }
  return false;          
  });

///////////////////////////////////////////////////////////////////////////////
/// deletes a row in new document view
///////////////////////////////////////////////////////////////////////////////

  $('#deleteNewDocButton').live('click', function () {
    var row = $(this).closest("tr").get(0);
    newDocumentTable.fnDeleteRow(documentEditTable.fnGetPosition(row));
  });

///////////////////////////////////////////////////////////////////////////////
/// submit avocsh content 
///////////////////////////////////////////////////////////////////////////////

var lastFormatQuestion = true;  
 
 $('#submitAvoc').live('click', function () {
    var data = $('#avocshContent').val();

    var r = [ ];
    for (var i = 0; i < shArray.length; ++i) {
      if (shArray[i] != data) {
        r.push(shArray[i]);
      }
    }

    shArray = r;
    if (shArray.length > 4) {
      shArray.shift();
    }
    shArray.push(data);
   
    $("#avocshContent").autocomplete({
      source: shArray
    });

    if (data == "exit") {
      location.reload();
      return;  
    }

    var command;
    if (data == "help") {
      command = "require(\"arangosh\").HELP";
    }
    else if (data == "reset") {
      command = "$('#avocshWindow').html(\"\");undefined;";
    }
    else {
      formatQuestion = JSON.parse($('input:radio[name=formatshellJSONyesno]:checked').val());
      if (formatQuestion != lastFormatQuestion) {
        if (formatQuestion == true) {
          start_pretty_print();
          lastFormatQuestion = true;
        } 
        if (formatQuestion == false) {
          stop_pretty_print(); 
          lastFormatQuestion = false;
        }
      }

      command = data;
    }

    var client = "arangosh> " + escapeHTML(data) + "<br>";
 
    $('#avocshWindow').append('<b class="avocshClient">' + client + '</b>');
    evaloutput(command);
    $("#avocshWindow").animate({scrollTop:$("#avocshWindow")[0].scrollHeight}, 1);
    $("#avocshContent").val('');
    return false; 
  });

  $('#refreshShell').live('click', function () {
    location.reload(); 
    return false; 
  });
///////////////////////////////////////////////////////////////////////////////
/// submit query content 
///////////////////////////////////////////////////////////////////////////////

  $('#submitQuery').live('click', function () {
    var data = {query:$('#queryContent').val()};
    var formattedJSON; 
    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data), 
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        $("#queryOutput").empty();
        
        var formatQuestion = JSON.parse($('input:radio[name=formatJSONyesno]:checked').val());
        if (formatQuestion === true) {
          $("#queryOutput").append('<pre><font color=green>' + FormatJSON(data.result) + '</font></pre>'); 
        }
        else {
          $("#queryOutput").append('<a class="querySuccess"><font color=green>' + JSON.stringify(data.result) + '</font></a>'); 
        }
      },
      error: function(data) {
        var temp = JSON.parse(data.responseText);
        $("#queryOutput").empty();
        $("#queryOutput").append('<a class="queryError"><font color=red>[' + temp.errorNum + '] ' + temp.errorMessage + '</font></a>'); 
      }
    });
  });

///////////////////////////////////////////////////////////////////////////////
/// draws view of creating a new document
///////////////////////////////////////////////////////////////////////////////

  $('#addDocumentButton').live('click', function () {
    toSplit = window.location.hash;
    var collID = toSplit.split("?");
    window.location.href = "#collection?" + collID[1] + "=newDocument";  
  });

///////////////////////////////////////////////////////////////////////////////
/// delete a document
///////////////////////////////////////////////////////////////////////////////

  $('#documentsView tr td button').live('click', function () {
    var aPos = documentsTable.fnGetPosition(this.parentElement);
    var aData = documentsTable.fnGetData(aPos[0]);
    var row = $(this).closest("tr").get(0);
    var documentID = aData[1];
   
    if (this.id == "deleteDoc") { 
    try { 
      $.ajax({ 
        type: 'DELETE', 
        contentType: "application/json",
        url: "/_api/document/" + documentID 
      });
    }

    catch(e) {
      alert(e); 
    }

    documentsTable.fnDeleteRow(documentsTable.fnGetPosition(row));
    }

    if (this.id == "editDoc") {
      window.location.href = "#editDocument?" + documentID; 
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// check top menue buttons 
///////////////////////////////////////////////////////////////////////////////

  $('#menue button').live('click', function () {
    if (this.id == "Collections") {
      $('#subCenterView').hide();
      $('#collectionsView').show();
      window.location.href = "#";
    }
    if (this.id == "Logs") {
      window.location.href = "#logs";
    }
    if (this.id == "Status") {
      window.location.href = "#status";
    }
    if (this.id == "Configuration") {
      window.location.href = "#config";
    }
    if (this.id == "Documentation") {
      return 0; 
    }
    if (this.id == "Query") {
      window.location.href = "#query";
    }
    if (this.id == "AvocSH") {
      window.location.href = "#avocsh";
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves an edited collection 
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedCollection').live('click', function () {
    var newColName = $('#editCollectionName').val(); 
    var currentid = $('#editCollectionID').text();

    if (newColName == checkCollectionName) {
      alert("Nothing to do...");
      return 0;  
    } 
    
      $.ajax({
        type: "PUT",
        url: "/_api/collection/" + currentid + "/rename",
        data: '{"name":"' + newColName + '"}',  
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection renamed"); 
          window.location.href = ""; 
          $('#subCenterView').hide();
          $('#centerView').show();
          drawCollectionsTable(); 
        },
        error: function(data) {
          var temp = JSON.parse(data.responseText);
          alert("Error: " + JSON.stringify(temp.errorMessage));  
        }
      });
  });

///////////////////////////////////////////////////////////////////////////////
/// cancels the creation of a new collection 
///////////////////////////////////////////////////////////////////////////////

  $('#cancelNewCollection').live('click', function () {
    window.location.href = "#"; 
  });

///////////////////////////////////////////////////////////////////////////////
/// saves a new collection  
///////////////////////////////////////////////////////////////////////////////

  $('#saveNewCollection').live('click', function () {
      var wfscheck = $('input:radio[name=waitForSync]:checked').val();
      var systemcheck = $('input:radio[name=isSystem]:checked').val();
      var collName = $('#createCollName').val(); 
      var collSize = $('#createCollSize').val();
      var journalSizeString;
 
      if (collSize == '') { 
        journalSizeString = ''; 
      }
      else {
        collSize = JSON.parse(collSize) * 1024 * 1024;  
        journalSizeString = ', "journalSize":' + collSize; 
      }
      if (collName == '') {
        alert("No collection name entered. Aborting..."); 
        return 0; 
      } 
      
      $.ajax({
        type: "POST",
        url: "/_api/collection",
        data: '{"name":"' + collName + '", "waitForSync":' + JSON.parse(wfscheck) + ',"isSystem":' + JSON.parse(systemcheck)+ journalSizeString + '}',
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection created"); 
          window.location.href = ""; 
          $('#subCenterView').hide();
          $('#centerView').show();
          drawCollectionsTable(); 
        },
        error: function(data) {
          try {
            var responseText = JSON.parse(data.responseText);
            alert(responseText.errorMessage);
          }
          catch (e) {
            alert(data.responseText);
          }
        }
      });
  }); 

///////////////////////////////////////////////////////////////////////////////
/// live-click function for buttons: "createCollection" & "refreshCollections" 
///////////////////////////////////////////////////////////////////////////////

  $('#centerView button').live('click', function () {
    if (this.id == "createCollection") {
      window.location.href = "#createCollection";
      return false; 
    }
    if (this.id == "refreshCollections") {
      drawCollectionsTable(); 
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// live-click function for buttons in "collectionsTableID td"  
///////////////////////////////////////////////////////////////////////////////

  $('#collectionsTableID td button').live('click', function () {
    var collectionsTable = $('#collectionsTableID').dataTable();
    var aPos = collectionsTable.fnGetPosition(this.parentElement);
    var aData = collectionsTable.fnGetData(aPos[0]);
    var collectionID = aData[1];
    var collectionName = aData[2]; 
     
///////////////////////////////////////////////////////////////////////////////
///delete a collection
///////////////////////////////////////////////////////////////////////////////

  if (this.id == "delete") {
    $( "#dialog-deleteCollection" ).dialog({
      resizable: false,
      height:180,
      modal: true,
      buttons: {
        "Delete collection": function() {
          $( this ).dialog( "close" );
            $.ajax({
              type: 'DELETE', 
              url: "/_api/collection/" + collectionID,
              success: function () {
                drawCollectionsTable();
              }, 
              error: function () {
                alert('Error'); 
              }
            });
          },
          Cancel: function() {
            $( this ).dialog( "close" );
        }
      }
    });
  }




///////////////////////////////////////////////////////////////////////////////
/// jump to edit collection view
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "edit") {
      window.location.href = "#editCollection?" + collectionID;
    }

///////////////////////////////////////////////////////////////////////////////
/// jump to doc view of a collection
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "showdocs" ) {
      window.location.href = "#showCollection?" + collectionID; 
      $('#nav1').removeClass('highlighted'); 
    }

///////////////////////////////////////////////////////////////////////////////
/// load a collection into memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "load") {
      globalAjaxCursorChange();
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/load",
        success: function () {
          drawCollectionsTable();
        }, 
        error: function (data) {
          var temp = JSON.parse(data.responseText);
          alert("Error: " + JSON.stringify(temp.errorMessage));  
          drawCollectionsTable();
        }
      });
    }
 
///////////////////////////////////////////////////////////////////////////////
/// unload a collection from memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "unload") {
      globalAjaxCursorChange();
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/unload",
        success: function () {
        }, 
        error: function () {
          alert('Error'); 
        }
      });
      drawCollectionsTable();
    }
  });
});

///////////////////////////////////////////////////////////////////////////////
/// hide all divs inside of #centerView
///////////////////////////////////////////////////////////////////////////////

function hideAllSubDivs () {
  $('#subCenterView').show();
  $('#collectionsEditView').hide();
  $('#newCollectionView').hide();
  $('#documentsView').hide();
  $('#documentEditView').hide(); 
  $('#newDocumentView').hide();
  $('#createCollectionView').hide();
  $('#editCollectionView').hide();  
  $('#logView').hide();
  $('#statusView').hide(); 
  $('#configView').hide();
  $('#avocshView').hide();
  $('#queryView').hide();  
}

///////////////////////////////////////////////////////////////////////////////
/// draw and fill collections table
///////////////////////////////////////////////////////////////////////////////

function drawCollectionsTable () {
  var collectionsTable = $('#collectionsTableID').dataTable();
  collectionsTable.fnClearTable();
  $.getJSON("/_api/collection/", function(data) {
    var items=[];
    $.each(data.collections, function(key, val) {
      var tempStatus = val.status;
      if (tempStatus == 1) {
        tempStatus = "new born collection";
      }
      else if (tempStatus == 2) {
        tempStatus = "<font color=orange>unloaded</font>";
        items.push(['<button class="enabled" id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16"></button><button class="enabled" id="load"><img src="/_admin/html/media/icons/connect_icon16.png" width="16" height="16"></button><button><img src="/_admin/html/media/icons/zoom_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button><button><img src="/_admin/html/media/icons/doc_edit_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button>', 
        val.id, val.name, tempStatus, "", ""]);
       }
      else if (tempStatus == 3) {
        tempStatus = "<font color=green>loaded</font>";
        var alive; 
        var size; 
      
        $.ajax({
          type: "GET",
          url: "/_api/collection/" + val.id + "/figures",
          contentType: "application/json",
          processData: false,
          async: false,   
          success: function(data) {
            size = data.figures.journals.fileSize + data.figures.datafiles.fileSize; 
            alive = data.figures.alive.count; 
          },
          error: function(data) {
          }
        });
        
        items.push(['<button class="enabled" id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16" title="Delete"></button><button class="enabled" id="unload"><img src="/_admin/html/media/icons/not_connected_icon16.png" width="16" height="16" title="Unload"></button><button class="enabled" id="showdocs"><img src="/_admin/html/media/icons/zoom_icon16.png" width="16" height="16" title="Show Documents"></button><button class="enabled" id="edit" title="Edit"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', 
        val.id, val.name, tempStatus,  bytesToSize(size), alive]);
      }
      else if (tempStatus == 4) {
        tempStatus = "in the process of being unloaded"; 
        items.push(['<button id="delete"><img src="/_admin/html/media/icons/round_minus_icon16_nofunction.png" class="nofunction" width="16" height="16"></button><button class="enabled" id="load"><img src="/_admin/html/media/icons/connect_icon16.png" width="16" height="16"></button><button><img src="/_admin/html/media/icons/zoom_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button><button><img src="/_admin/html/media/icons/doc_edit_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button>', 
        val.id, val.name, tempStatus, "", ""]);
      }
      else if (tempStatus == 5) {
        tempStatus = "deleted"; 
        items.push(["", val.id, val.name, tempStatus, "", ""]);
      }
/*      else {
        tempStatus = "corrupted"; 
        items.push(["", "<font color=red>"+ val.id + "</font>", "<font color=red>"+ val.name + "</font>", "<font color=red>" + tempStatus + "</font>", "", ""]);
      }*/
    });
  $('#collectionsTableID').dataTable().fnAddData(items);
  showCursor();
  });
}

///////////////////////////////////////////////////////////////////////////////
/// short function to enable edit mode for a table 
///////////////////////////////////////////////////////////////////////////////

function documentTableMakeEditable (tableID) {
  var documentEditTable = $(tableID).dataTable();
  var i = 0; 
  $('.writeable', documentEditTable.fnGetNodes() ).each(function () {
    if ( i == 1) {
      $(this).removeClass('writeable');
      i = 0; 
    }
    if (this.innerHTML == "_id" || this.innerHTML == "_rev") {
      $(this).removeClass('writeable');
      i = 1; 
    }
  });
  //jeditable init
  $('.writeable', documentEditTable.fnGetNodes()).editable(function(value, settings) {
    var aPos = documentEditTable.fnGetPosition(this);
    if (aPos[1] == 1) {
      //return key column
      documentEditTable.fnUpdate(value, aPos[0], aPos[1]);
      return value;
    }
    if (aPos[1] == 2) {
      var oldContent = JSON.parse(documentEditTable.fnGetData(aPos[0], aPos[1] + 1));
      var test = getTypedValue(value);
      if (String(value) == String(oldContent)) {
        // no change
        return value2html(oldContent);
      }
      else {
        // change update hidden row
       //MARKER 
        documentEditTable.fnUpdate(JSON.stringify(test), aPos[0], aPos[1] + 1); 
        // return visible row 
        return value2html(test);
      } 
    } 
  },{
    data: function() {
      var aPos = documentEditTable.fnGetPosition(this);
      var value = documentEditTable.fnGetData(aPos[0], aPos[1]);

      if (aPos[1] == 1) {
        return value; 
      }
      if (aPos[1] == 2) {
        var oldContent = documentEditTable.fnGetData(aPos[0], aPos[1] + 1);
        if (typeof(oldContent) == 'object') {
          //grep hidden row and paste in visible row
          return value2html(oldContent);
        }
        else {
          return oldContent;  
        }
      }
    }, 
    //type: 'textarea',
    type: "autogrow", 
    tooltip: 'click to edit', 
    cssclass : 'jediTextarea', 
    submit: 'Okay',
    cancel: 'Cancel', 
    onblur: 'cancel',
    //style: 'display: inline',
    autogrow: {lineHeight: 16, minHeight: 30}
  });
}

///////////////////////////////////////////////////////////////////////////////
/// escape a string 
///////////////////////////////////////////////////////////////////////////////

function escaped (value) {
  return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#38;");
}

///////////////////////////////////////////////////////////////////////////////
/// get value of user edited tabel cell 
///////////////////////////////////////////////////////////////////////////////

function getTypedValue (value) {
  if (value == 'true') {
    return true;
  }
  if (value == 'false') {
    return false;
  }
  if (value == 'null') {
    return null;
  }
  if (value.match(/^-?((\d+)?\.)?\d+$/)) {
    // TODO: support exp notation
    return parseFloat(value);
  }
  
  try {
    // assume list or object
    var test = JSON.parse(value);
    if (test instanceof Array) {
      // value is an array
      return test;
    }
    if (typeof(test) == 'object') {
      // value is an object 
      return test;
    }
  }
  catch (err) {
  }

  // fallback: value is a string
  value = value + '';
  
  if (value.substr(0, 1) == '"' && value.substr(-1) == '"' ) {
    value = value.substr(1, value.length-2);
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////////
/// checks type fo typed cell value 
///////////////////////////////////////////////////////////////////////////////

function value2html (value) {
  var checked = typeof(value); 
  switch(checked) { 
    case 'number': 
    return ("<a class=\"sh_number\">" + value + "</a>");
    case 'string': 
    return ("<a class=\"sh_string\">"  + escaped(value) + "</a>");
    case 'boolean': 
    return ("<a class=\"sh_keyword\">" + value + "</a>");
    case 'object':
    if (value instanceof Array) {
      return ("<a class=\"sh_array\">" + escaped(JSON.stringify(value)) + "</a>");
    }
    else {
      return ("<a class=\"sh_object\">"+ escaped(JSON.stringify(value)) + "</a>");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/// draw breadcrumb navigation  
///////////////////////////////////////////////////////////////////////////////

function createnav (menue) {
  $('#nav1').text(menue);
  $('#nav1').attr('class', ''); 
  $('#nav2').text('');
  $('#nav2').attr('class', ''); 
  $('#nav3').text('');
  $('#nav3').attr('class', ''); 
  $('#nav4').text('');

  if (menue == "Collections") {
    $('#nav1').attr('href', '#');
  }
  else {
    $('#nav1').attr('href',null);
  } 
}

///////////////////////////////////////////////////////////////////////////////
/// animated footer div  
///////////////////////////////////////////////////////////////////////////////

$(function() {
  $('#footerSlideButton').click(function() {
    if(open === false) {
      $('#footerSlideContent').animate({ height: '120px' });
      $(this).css('backgroundPosition', 'bottom left');
      open = true;
      $('#movetologinButton').text("Hide");
    } 
    else {
      $('#footerSlideContent').animate({ height: '25px' });
      $(this).css('backgroundPosition', 'top left');
      open = false;
      $('#movetologinButton').text("Login");
    }
  });

  $('#movetologinButton').click(function() {
    if(open === false) {
      $('#movetologinButton').text("Hide");
      $('#footerSlideContent').animate({ height: '120px' });
      $('#footerSlideButton').css('backgroundPosition', 'bottom left');
      open = true;
    } 
    else {
      $('#movetologinButton').text("Login");
      $('#footerSlideContent').animate({ height: '25px' });
      $('#footerSlideButton').css('backgroundPosition', 'top left');
      open = false;
    }
  });
});

///////////////////////////////////////////////////////////////////////////////
/// Log tables pagination  
///////////////////////////////////////////////////////////////////////////////
function createLogTable(loglevel) {
 
  $("#logSearchField").val(""); 
  $(":input").focus(); 
  showLogPagination();
  currentPage = 1;  
  currentLoglevel = loglevel;  
  var url = "/_admin/log?level="+loglevel+"&size=10";
//set tableid  
  if (loglevel == 1) {currentTableID = "#critLogTableID";  } 
  else if (loglevel == 2) {currentTableID = "#warnLogTableID";} 
  else if (loglevel == 3) {currentTableID = "#infoLogTableID";} 
  else if (loglevel == 4) {currentTableID = "#debugLogTableID";} 
  else if (loglevel == 5) {
    currentTableID = "#logTableID";
    url = "/_admin/log?upto=4&size=10"; 
  } 
//get first rows
  $.getJSON(url, function(data) { 
    var items=[];
    var i=0; 
    currentAmount = data.totalAmount; 
    var totalPages = Math.ceil(currentAmount / 10); 
//clear table   
    $(currentTableID).dataTable().fnClearTable();
//draw first 10 rows
    $.each(data.lid, function () {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]);
      i++;
    });

  if (totalPages == 0) {
    $(currentTableID + '_status').text("Showing page 0 of 0");
    return 0;  
  }

  $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });
}

function createPrevDocPagination() {
  if (collectionCurrentPage == 1) {
    return 0; 
  }
  var prevPage = JSON.parse(collectionCurrentPage) -1; 
  var offset = prevPage * 10 - 10; 

  $('#documentsTableID').dataTable().fnClearTable();
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data.result, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class="prettify">' + cutByResolution(JSON.stringify(v)) + '</pre>']);  
      });
      $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
    },
    error: function(data) {        
    }
  });
  collectionCurrentPage = prevPage; 
  $('#documents_status').text("Showing page " + collectionCurrentPage + " of " + totalCollectionCount); 
}
function createNextDocPagination () {

  if (collectionCurrentPage == totalCollectionCount) {
    return 0; 
  }
   
  var nextPage = JSON.parse(collectionCurrentPage) +1; 
  var offset =  collectionCurrentPage * 10; 

  $('#documentsTableID').dataTable().fnClearTable();
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data.result, function(k, v) {
        $("#documentsTableID").dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class=prettify>' + cutByResolution(JSON.stringify(v)) + '</pre>']);  
      });
      $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
    },
    error: function(data) {        
    }
  });
  collectionCurrentPage = nextPage; 
  $('#documents_status').text("Showing page " + collectionCurrentPage + " of " + totalCollectionCount); 
}

function createPrevPagination(checked) {
  var prevPage = JSON.parse(currentPage) - 1; 
  var offset = prevPage * 10 - 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;
  var totalPages = Math.ceil(currentAmount / 10); 

  if (currentPage == 1 || totalPages == 0 ) {
    return 0; 
  }
  if (checked == "all") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable(); 

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++; 
    });
  currentPage = JSON.parse(currentPage) - 1; 
  $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  }); 
}

function createNextPagination(checked) { 

  var totalPages = Math.ceil(currentAmount / 10); 
 var offset = currentPage * 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;

  if (currentPage == totalPages || totalPages == 0 ) {
    return 0; 
  }
  if (checked == "all") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable();

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++
    });
    currentPage = JSON.parse(currentPage) + 1; 
    $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });
}
    
function showCursor() {
  $('.enabled').mouseover(function () {
    $(this).css('cursor', 'pointer');
  });
}

function cutByResolution (string) {
  if (string.length > 1024) {
    return escaped(string.substr(0, 1024)) + '...';
  }
  return escaped(string);
}

function createFirstPagination () {

  if (collectionCurrentPage == 1) {
    return 0; 
  }

  $('#documentsTableID').dataTable().fnClearTable();
  
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":0,"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data.result, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class=prettify>' + cutByResolution(JSON.stringify(v)) + '</pre>' ]);  
      });
      $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
      collectionCurrentPage = 1;
      $('#documents_status').text("Showing page 1 of " + totalCollectionCount); 
    },
    error: function(data) {
      
    }
  });
}

function createLastLogPagination (tableid) {
  var totalPages = Math.ceil(currentAmount / 10); 
  var offset = (totalPages * 10) - 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;

  if (currentPage == totalPages || totalPages == 0 ) {
    return 0; 
  }
  if (tableid = "#logTableID") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable();

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++
    });
    currentPage = totalPages;  
    $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });

}

function createLastPagination () {
  if (totalCollectionCount == collectionCurrentPage) {
    return 0
  }

  $('#documentsTableID').dataTable().fnClearTable();
  
  var offset = totalCollectionCount * 10 - 10; 
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data.result, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class=prettify>' + cutByResolution(JSON.stringify(v)) + '</pre>' ]);  
      });
      $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
      collectionCurrentPage = totalCollectionCount;
      $('#documents_status').text("Showing page " + totalCollectionCount + " of " + totalCollectionCount); 
    },
    error: function(data) {
    }
  });
}

function globalAjaxCursorChange() {  
  $("html").bind("ajaxStart", function(){  
    $(this).addClass('busy');  
  }).bind("ajaxStop", function(){  
    $(this).removeClass('busy');  
  });  
} 

function FormatJSON(oData, sIndent) {
    if (arguments.length < 2) {
        var sIndent = "";
    }
    var sIndentStyle = "    ";
    var sDataType = RealTypeOf(oData);

    if (sDataType == "array") {
        if (oData.length == 0) {
            return "[]";
        }
        var sHTML = "[";
    } else {
        var iCount = 0;
        $.each(oData, function() {
            iCount++;
            return;
        });
        if (iCount == 0) { // object is empty
            return "{}";
        }
        var sHTML = "{";
    }

    // loop through items
    var iCount = 0;
    $.each(oData, function(sKey, vValue) {
        if (iCount > 0) {
            sHTML += ",";
        }
        if (sDataType == "array") {
            sHTML += ("\n" + sIndent + sIndentStyle);
        } else {
            sHTML += ("\n" + sIndent + sIndentStyle + JSON.stringify(sKey) + ": ");
        }

        // display relevant data type
        switch (RealTypeOf(vValue)) {
            case "array":
            case "object":
                sHTML += FormatJSON(vValue, (sIndent + sIndentStyle));
                break;
            case "boolean":
            case "number":
                sHTML += vValue.toString();
                break;
            case "null":
                sHTML += "null";
                break;
            case "string":
                sHTML += "\"" + vValue.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"";
                break;
            default:
                sHTML += ("TYPEOF: " + typeof(vValue));
        }

        // loop
        iCount++;
    });

    // close object
    if (sDataType == "array") {
        sHTML += ("\n" + sIndent + "]");
    } else {
        sHTML += ("\n" + sIndent + "}");
    }

    // return
    return sHTML;
}

function RealTypeOf(v) {
  if (typeof(v) == "object") {
    if (v === null) return "null";
    if (v.constructor == (new Array).constructor) return "array";
    if (v.constructor == (new Date).constructor) return "date";
    if (v.constructor == (new RegExp).constructor) return "regex";
    return "object";
  }
  return typeof(v);
}

function bytesToSize(bytes, precision) {  
  var kilobyte = 1024;
  var megabyte = kilobyte * 1024;
  var gigabyte = megabyte * 1024;
  var terabyte = gigabyte * 1024;
   
  if ((bytes >= 0) && (bytes < kilobyte)) {
    return bytes + ' B';
   } 
   else if ((bytes >= kilobyte) && (bytes < megabyte)) {
     return (bytes / kilobyte).toFixed(precision) + ' KB';
   } 
   else if ((bytes >= megabyte) && (bytes < gigabyte)) {
     return (bytes / megabyte).toFixed(precision) + ' MB';
   } 
   else if ((bytes >= gigabyte) && (bytes < terabyte)) {
     return (bytes / gigabyte).toFixed(precision) + ' GB';
   } 
   else if (bytes >= terabyte) {
     return (bytes / terabyte).toFixed(precision) + ' TB';
   }   
   else {
     return bytes + ' B';
   }
}

function stateReplace (value) {
  var inString = false;
  var length = value.length;
  var position = 0;
  var escaped = false;

  var output = "";
  while (position < length) {
    var c = value.charAt(position++);

    if (c === '\\') {
      if (escaped) {
        /* case: \ followed by \ */
        output += '\\\\';
        escaped = false;
      } 
      else {
        /* case: single backslash */
        escaped = true;
      }
    }
    else if (c === '"') {
      if (escaped) {
        /* case: \ followed by " */
        output += '\\"';
        escaped = false;
      } 
      else {
        output += '"';
        inString = !inString;
      }
    } 
    else {
      if (inString) {
        if (escaped) {
          output += '\\' + c;
          escaped = false;
        }
        else {
          switch (c) {
            case '\b':
              output += '\\b';
              break;
            case '\f':
              output += '\\f';
              break;
            case '\n':
              output += '\\n';
              break;
            case '\t':
              output += '\\t';
              break;
            case '\r':
              output += '\\r';
              break;
            default:
              output += c;
              break;
          }
        }
      } 
      else {
        if (c >= '!') {
          output += c;
        }
      }
    }
  }

  return output;
}

$('#submitDocPageInput').live('click', function () {
  try {
    var enteredPage = JSON.parse($('#docPageInput').val());
    if (enteredPage > 0 && enteredPage <= totalCollectionCount) {
      $('#documentsTableID').dataTable().fnClearTable();
  
      var offset = enteredPage * 10 - 10; 
      $.ajax({
        type: 'PUT',
        url: '/_api/simple/all/',
        data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
        contentType: "application/json",
        success: function(data) {
          $.each(data.result, function(k, v) {
            $('#documentsTableID').dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, '<pre class="prettify">' + cutByResolution(JSON.stringify(v)) + '</pre>' ]);  
          });
          $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
          collectionCurrentPage = enteredPage;
          $('#documents_status').text("Showing page " + enteredPage + " of " + totalCollectionCount); 
          return false; 
        },
        error: function(data) {
        }
      });
    }
    
    else { 
      return false; 
    }  
  }

  catch(e) {
    alert("No valid Page"); 
  return false; 
  }
  return false; 
});



function highlightNav (string) {
  $("#nav1").removeClass("nonhighlighted");
  $("#nav2").removeClass("nonhighlighted");
  $("#nav3").removeClass("nonhighlighted");
  $("#nav4").removeClass("nonhighlighted");
  $(string).addClass("highlighted");
}

function makeStringEditable () {
  $('.editString').editable(function(value, settings) { 
    return(value);
  },{
    tooltip   : 'Click to edit string...',
    width     : '200px',
    height    : '20px'
  });
}

function makeIntEditable () {
  $('.editInt').editable(function(value, settings) {
    if (is_int(value) == true) {
      return value; 
    }
    else {
      alert("Only integers allowed!"); 
      return 0;
    } 
  },{
    tooltip   : 'Click to edit integer...',
    width     : '200px',
    height    : '20px'
  });
}

function is_int(value){
  if((parseFloat(value) == parseInt(value)) && !isNaN(value)){
      return true;
  } else {
      return false;
  }
}

function showLogPagination () {
  $('#logToolbarAll').show(); 
  $('#logToolbarCrit').show(); 
  $('#logToolbarWarn').show(); 
  $('#logToolbarDebu').show(); 
  $('#logToolbarInfo').show(); 
}

function hideLogPagination() {
  $('#logToolbarAll').hide(); 
  $('#logToolbarCrit').hide(); 
  $('#logToolbarWarn').hide(); 
  $('#logToolbarDebu').hide(); 
  $('#logToolbarInfo').hide(); 
}

function evaloutput (data) {
  try {
    var result = eval(data); 

    if (result !== undefined) {
      print(result);
    }
  }
  catch(e) {
    $('#avocshWindow').append('<p class="avocshError">Error:' + e + '</p>');
  }
}

function validate(evt) {
  var theEvent = evt || window.event;
  var key = theEvent.keyCode || theEvent.which;

  if ((key == 37 || key == 39) && !theEvent.shiftKey) {
    return;
  }
  key = String.fromCharCode( key );
  var regex = /[0-9\.\b]/;
  if( !regex.test(key) ) {
    theEvent.returnValue = false;
    if(theEvent.preventDefault) theEvent.preventDefault();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief arango datafile debugger
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var console = require("console");
var fs = require("fs");

var printf = internal.printf;

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief remove datafile
////////////////////////////////////////////////////////////////////////////////

function RemoveDatafile (collection, type, datafile) {
  var backup = datafile + ".corrupt";

  fs.move(datafile, backup);

  printf("Removed %s at %s\n", type, datafile);
  printf("Backup is in %s\n", backup);
  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wipe entries
////////////////////////////////////////////////////////////////////////////////

function WipeDatafile (collection, type, datafile, lastGoodPos) {
  collection.truncateDatafile(datafile, lastGoodPos);

  printf("Truncated and sealed datafile\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a journal
////////////////////////////////////////////////////////////////////////////////

function DeepCheckJournal (collection, type, datafile, scan, lastGoodPos) {
  var entries = scan.entries;

  if (entries.length == 0) {
    printf("WARNING: The journal is empty. Even the header is missing. Going\n");
    printf("         to remove the datafile.\n");
    printf("\n");

    RemoveDatafile(collection, type, datafile);

    return;
  }

  if (entries.length === lastGoodPos + 3 && entries[lastGoodPos + 2].status === 2) {
    printf("WARNING: The journal was not closed properly, the last entry is corrupted.\n");
    printf("         This might happen ArangoDB was killed and the last entry was not\n");
    printf("         fully written to disk. Going to remove the last entry.\n");
    printf("\n");
  }
  else {
    printf("WARNING: The journal was not closed properly, the last entries is corrupted.\n");
    printf("         This might happen ArangoDB was killed and the last entries were not\n");
    printf("         fully written to disk.\n");
    printf("\n");

    printf("Wipe the last entries (Y/N)? ");
    var line = console.getline();

    if (line !== "yes" && line !== "YES" && line !== "y" && line !== "Y") {
      printf("ABORTING\n");
      return;
    }
  }

  var entry = entries[lastGoodPos];

  WipeDatafile(collection, type, datafile, entry.position + entry.size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

function DeepCheckDatafile (collection, type, datafile, scan, lastGoodPos) {
  var entries = scan.entries;

  if (entries.length == 0) {
    printf("WARNING: The datafile is empty. Even the header is missing. Going\n");
    printf("         to remove the datafile. This should never happen. Datafiles\n");
    printf("         are append-only. Make sure your hard disk does not contain\n");
    printf("         any hardware errors.\n");
    printf("\n");

    RemoveDatafile(collection, type, datafile);

    return;
  }

  printf("WARNING: The datafile contains corrupt entries. This should never happen.\n");
  printf("         Datafiles are append-only. Make sure your hard disk does not contain\n");
  printf("         any hardware errors.\n");
  printf("\n");

  printf("Wipe the last entries (Y/N)? ");
  var line = console.getline();

  if (line !== "yes" && line !== "YES" && line !== "y" && line !== "Y") {
    printf("ABORTING\n");
    return;
  }

  var entry = entries[lastGoodPos];

  WipeDatafile(collection, type, datafile, entry.position + entry.size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile deeply
////////////////////////////////////////////////////////////////////////////////

function DeepCheckDatafile (collection, type, datafile, scan) {
  var entries = scan.entries;

  printf("Entries\n");

  var lastGood = 0;
  var lastGoodPos = 0;
  var stillGood = true;

  for (var i = 0;  i < entries.length;  ++i) {
    var entry = entries[i];
    var s = "unknown";

    switch (entry.status) {
      case 1: s = "OK";  break;
      case 2: s = "OK (end)";  break;
      case 3: s = "FAILED (empty)";  break;
      case 4: s = "FAILED (too small)";  break;
      case 5: s = "FAILED (crc mismatch)";  break;
    }

    printf("  %d: status %s type %d size %d\n", i, s, entry.type, entry.size);

    if (entry.status === 1 || entry.status === 2) {
      if (stillGood) {
        lastGood = entry;
        lastGoodPos = i;
      }
    }
    else {
      stillGood = false;
    }
  }

  if (! stillGood) {
    printf("  Last good position: %d\n", lastGood.position + lastGood.size);
    printf("\n");

    if (type === "journal" || type === "compactor") {
      DeepCheckJournal(collection, type, datafile, scan, lastGoodPos);
    }
    else {
      DeepCheckDatafile(collection, type, datafile, scan, lastGoodPos);
    }
  }

  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

function CheckDatafile (collection, type, datafile, issues) {
  printf("Datafile\n");
  printf("  path: %s\n", datafile);
  printf("  type: %s\n", type);

  var scan = collection.datafileScan(datafile);

  printf("  current size: %d\n", scan.currentSize);
  printf("  maximal size: %d\n", scan.maximalSize);
  printf("  total used: %d\n", scan.endPosition);
  printf("  # of entries: %d\n", scan.numberMarkers);

  // set default value to unknown
  var statusMessage = "UNKNOWN (" + scan.status + ")"; 
  var color = internal.COLORS.COLOR_YELLOW;

  switch (scan.status) {
    case 1:
      statusMessage = "OK";
      color = internal.COLORS.COLOR_GREEN;
      break;

    case 1:
      statusMessage = "NOT OK (reached empty marker)";
      color = internal.COLORS.COLOR_RED;
      break;

    case 3:
      statusMessage = "FATAL (reached corrupt marker)";
      color = internal.COLORS.COLOR_RED;
      break;

    case 4:
      statusMessage = "FATAL (crc failed)";
      color = internal.COLORS.COLOR_RED;
      break;

    case 5:
      statusMessage = "FATAL (cannot open datafile or too small)";
      color = internal.COLORS.COLOR_RED;
      break;
  }

  printf(color);
  printf("  status: %s\n\n", statusMessage);
  printf(internal.COLORS.COLOR_RESET);

  if (scan.status !== 1) {
    issues.push({ 
      collection: collection.name(), 
      path: datafile, 
      type: type, 
      status: scan.status, 
      message: statusMessage, 
      color: color 
    });
  }

  if (scan.numberMarkers === 0) {
    statusMessage = "datafile contains no entries";
    color = internal.COLORS.COLOR_YELLOW;

    issues.push({
      collection: collection.name(), 
      path: datafile, 
      type: type, 
      status: scan.status, 
      message: statusMessage,
      color: color 
    });

    printf(color);
    printf("WARNING: %s\n", statusMessage);
    printf(internal.COLORS.COLOR_RESET);
    RemoveDatafile(collection, type, datafile);
    return;
  }

  if (scan.entries[0].type !== 1000) {
    // asserting a TRI_DF_MARKER_HEADER as first marker
    statusMessage = "datafile contains no datafile header marker at pos #0!";
    color = internal.COLORS.COLOR_YELLOW;

    issues.push({
      collection: collection.name(), 
      path: datafile, 
      type: type, 
      status: scan.status, 
      message: statusMessage,
      color: color 
    });

    printf(color);
    printf("WARNING: %s\n", statusMessage);
    printf(internal.COLORS.COLOR_RESET);
    RemoveDatafile(collection, type, datafile);
    return;
  }

  if (scan.entries.length == 2 && scan.entries[1].type !== 2000) {
    // asserting a TRI_COL_MARKER_HEADER as second marker
    statusMessage = "datafile contains no collection header marker at pos #1!";
    color = internal.COLORS.COLOR_YELLOW;

    issues.push({
      collection: collection.name(), 
      path: datafile, 
      type: type, 
      status: scan.status, 
      message: statusMessage,
      color: color
    });

    printf(color);
    printf("WARNING: %s\n", statusMessage);
    printf(internal.COLORS.COLOR_RESET);
    RemoveDatafile(collection, type, datafile);
    return;
  }

  if (scan.status === 1) {
    return;
  }

  DeepCheckDatafile(collection, type, datafile, scan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a collection
////////////////////////////////////////////////////////////////////////////////

function CheckCollection (collection, issues) {
  printf("Database\n");
  printf("  path: %s\n", internal.db._path);
  printf("\n");

  printf("Collection\n");
  printf("  name: %s\n", collection.name());
  printf("  identifier: %s\n", collection._id);
  printf("\n");

  var datafiles = collection.datafiles();

  printf("Datafiles\n");
  printf("  # of journals: %d\n", datafiles.journals.length);
  printf("  # of compactors: %d\n", datafiles.compactors.length);
  printf("  # of datafiles: %d\n", datafiles.datafiles.length);
  printf("\n");

  for (var i = 0;  i < datafiles.journals.length;  ++i) {
    CheckDatafile(collection, "journal", datafiles.journals[i], issues);
  }

  for (var i = 0;  i < datafiles.datafiles.length;  ++i) {
    CheckDatafile(collection, "datafiles", datafiles.datafiles[i], issues);
  }

  for (var i = 0;  i < datafiles.compactors.length;  ++i) {
    CheckDatafile(collection, "compactor", datafiles.compactors[i], issues);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief select and check a collection
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  var argc = argv.length;
  var collections = internal.db._collections();
  var i;

  printf("%s\n", "    ___      _         __ _ _           ___  ___    ___ ");
  printf("%s\n", "   /   \\__ _| |_ __ _ / _(_) | ___     /   \\/ __\\  / _ \\");
  printf("%s\n", "  / /\\ / _` | __/ _` | |_| | |/ _ \\   / /\\ /__\\// / /_\\/");
  printf("%s\n", " / /_// (_| | || (_| |  _| | |  __/  / /_// \\/  \\/ /_\\\\ ");
  printf("%s\n", "/___,' \\__,_|\\__\\__,_|_| |_|_|\\___| /___,'\\_____/\\____/ ");
  printf("\n");

  if (collections.length == 0) {
    printf("No collections available. Exiting\n");
    return;
  }

  printf("Available collections:\n");

  for (i = 0;  i < collections.length;  ++i) {
    printf("  %d: %s\n", i, collections[i].name());
  }

  printf("  *: all\n");

  printf("\n");

  printf("Collection to check: ");
  var a = [];
  
  while (true) {
    var line = console.getline();

    if (line == "") {
      break;
    }

    if (line === "*") {
      for (i = 0;  i < collections.length;  ++i) {
        a.push(i);
      }

      break;
    }
    else {
      var l = parseInt(line);

      if (l < 0 || l >= collections.length || l === null || l === undefined || isNaN(l)) {
        printf("Please select a number between 0 and %d: ", collections.length - 1);
      }
      else {
        a.push(l);
        break;
      }
    }
  }

  var issues = [ ];

  for (i = 0;  i < a.length;  ++i) {
    var collection = collections[a[i]];

    printf("Checking collection #%d: %s\n", a[i], collection.name());

    var last = Math.round(internal.time());

    // unload all collections not yet unloaded (2) & not corrupted (0)
    while (collection.status() !== 2 && collection.status() !== 0) {
      collection.unload();

      var next = Math.round(internal.time());

      if (next != last) {
        printf("Trying to unload collection '%s'\n", collection.name());
        last = next;
      }
    }

    printf("\n");

    CheckCollection(collection, issues);
  }

 
  if (issues.length > 0) {
    // report issues
    printf("\n%d issue(s) found:\n------------------------------------------\n", issues.length);

    for (i = 0; i < issues.length; ++i) {
      var issue = issues[i];
      printf("  issue #%d\n    collection: %s\n    path: %s\n    type: %s\n",
             i,
             issue.collection,
             issue.path,
             String(issue.type));

      printf("%s", issue.color);
      printf("    message: %s", issue.message);
      printf("%s", internal.COLORS.COLOR_RESET);
      printf("\n\n");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

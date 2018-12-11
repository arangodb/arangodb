////////////////////////////////////////////////////////////////////////////////
/// @brief arango datafile debugger
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var console = require("console");
var fs = require("fs");

var printf = internal.printf;

////////////////////////////////////////////////////////////////////////////////
/// @brief unload a collection
////////////////////////////////////////////////////////////////////////////////

function UnloadCollection (collection) {
  var tries = 0;

  // unload collection if not yet unloaded (2) & not corrupted (0)
  while (collection.status() !== 2 && collection.status() !== 0) {
    collection.unload();

    if (++tries >= 20) {
      break;
    }
    if (tries == 1) {
      printf("Trying to unload collection '%s', current status: %s\n", collection.name(), collection.status());
    }

    internal.wait(1, true);
  }
}

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
/// @brief try to repair a datafile
////////////////////////////////////////////////////////////////////////////////

function TryRepairDatafile (collection, datafile) {
  UnloadCollection(collection);
  return collection.tryRepairDatafile(datafile);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wipe entries
////////////////////////////////////////////////////////////////////////////////

function WipeDatafile (collection, type, datafile, lastGoodPos) {
  UnloadCollection(collection);
  collection.truncateDatafile(datafile, lastGoodPos);

  printf("Truncated and sealed datafile\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief queries user to wipe a datafile
////////////////////////////////////////////////////////////////////////////////

function QueryWipeDatafile (collection, type, datafile, scan, lastGoodPos) {
  var entries = scan.entries;

  if (entries.length === 0) {
    if (type === "journal" || type === "compactor") {
      printf("WARNING: The journal is empty. Even the header is missing. Going\n");
      printf("         to remove the file.\n");
    }
    else {
      printf("WARNING: The datafile is empty. Even the header is missing. Going\n");
      printf("         to remove the datafile. This should never happen. Datafiles\n");
      printf("         are append-only. Make sure your hard disk does not contain\n");
      printf("         any hardware errors.\n");
    }
    printf("\n");

    RemoveDatafile(collection, type, datafile);
    return;
  }

  var ask = true;
  var tryRepair = false;

  if (type === "journal") {
    if (entries.length === lastGoodPos + 3 && entries[lastGoodPos + 2].status === 2) {
      printf("WARNING: The journal was not closed properly, the last entry is corrupted.\n");
      printf("         This might happen if ArangoDB was killed and the last entry was not\n");
      printf("         fully written to disk. Going to remove the last entry.\n");
      ask = false;
    }
    else {
      printf("WARNING: The journal was not closed properly, the last entries are corrupted.\n");
      printf("         This might happen if ArangoDB was killed and the last entries were not\n");
      printf("         fully written to disk.\n");
    }
  }
  else {
    printf("WARNING: The datafile contains corrupt entries. This should never happen.\n");
    printf("         Datafiles are append-only. Make sure your hard disk does not contain\n");
    printf("         any hardware errors.\n");

    tryRepair = true;
  }

  printf("\n");
  
  var entry = entries[lastGoodPos];

  if (ask) {
    var line;

    if (tryRepair) {
      printf("Try to repair the error(s) (Y/N)? ");
      line = console.getline();

      if (line === "yes" || line === "YES" || line === "y" || line === "Y") {
        if (TryRepairDatafile(collection, datafile)) {
          printf("Repair succeeded.\n");
          return;
        }

        printf("Repair failed.\n");
      }
    }

    printf("Wipe the last entries (Y/N)? ");
    line = console.getline();

    if (line !== "yes" && line !== "YES" && line !== "y" && line !== "Y") {
      printf("ABORTING\n");
      return;
    }
  }

  WipeDatafile(collection, type, datafile, entry.position + entry.realSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints details about entries
////////////////////////////////////////////////////////////////////////////////

function PrintEntries (entries, amount) {
  var start, end;

  if (amount > 0) {
    start = 0;
    end = amount;
    if (end > entries.length) {
      end = entries.length;
    }
  }
  else {
    start = entries.length + amount - 1;
    if (start < 0) {
      return;
    }
    if (start < Math.abs(amount)) {
      return;
    }

    end = entries.length;
  }

  for (var i = start;  i < end;  ++i) {
    var entry = entries[i], extra;

    var s = "unknown";

    switch (entry.status) {
      case 1: s = "OK";  break;
      case 2: s = "OK (end)";  break;
      case 3: s = "FAILED (empty)";  break;
      case 4: s = "FAILED (too small)";  break;
      case 5: s = "FAILED (crc mismatch)";  break;
    }

    if (entry.key) {
      extra = " - key: " + entry.key;
    }
    else {
      extra = "";
    }

    printf("  %d: status %s type %d (%s) size %d, tick %s%s\n", i, s, entry.type, entry.typeName, entry.realSize, entry.tick, extra);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile deeply
////////////////////////////////////////////////////////////////////////////////

function DeepCheckDatafile (collection, type, datafile, scan) {
  var entries = scan.entries;

  var diagnosis = "";
  var lastGood, firstBad;
  var lastGoodPos = 0;
  var stillGood = true;

  for (var i = 0;  i < entries.length;  ++i) {
    var entry = entries[i];

    if (entry.status === 1 || entry.status === 2) {
      if (stillGood) {
        lastGood = entry;
        lastGoodPos = i;
      }
    }
    else {
      stillGood = false;
      diagnosis = entry.diagnosis || "";
      firstBad = entry;
      break;
    }
  }

  if (! stillGood) {
    printf("  Last good marker:   start: %d (hex %s), length: %d (hex %s)\n", 
           lastGood.position, 
           lastGood.position.toString(16), 
           lastGood.realSize,
           lastGood.realSize.toString(16));

    printf("  First bad marker:   start: %d (hex %s), length: %d (hex %s)\n", 
           firstBad.position, 
           firstBad.position.toString(16), 
           firstBad.realSize,
           firstBad.realSize.toString(16));

    if (diagnosis) {
      printf("  Diagnosis:          %s\n", diagnosis);
    }
    printf("\n");

    QueryWipeDatafile(collection, type, datafile, scan, lastGoodPos);
  }

  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

function CheckDatafile (collection, type, datafile, issues, details) {
  printf("Datafile\n");
  printf("  path: %s\n", datafile);
  printf("  type: %s\n", type);

  var scan = collection.datafileScan(datafile);

  printf("  current size: %d\n", scan.currentSize);
  printf("  maximal size: %d\n", scan.maximalSize);
  printf("  total used: %d\n", scan.endPosition);
  printf("  # of entries: %d\n", scan.numberMarkers);
  printf("  status: %d\n", scan.status);
  printf("  isSealed: %s\n", scan.isSealed ? "yes" : "no");

  // set default value to unknown
  var statusMessage = "UNKNOWN (" + scan.status + ")";
  var color = internal.COLORS.COLOR_YELLOW;

  switch (scan.status) {
    case 1:
      statusMessage = "OK";
      color = internal.COLORS.COLOR_GREEN;
      if (! scan.isSealed && type === "datafile") {
        color = internal.COLORS.COLOR_YELLOW;
      }
      break;

    case 2:
      statusMessage = "NOT OK (reached empty marker)";
      color = internal.COLORS.COLOR_RED;
      break;

    case 3:
      statusMessage = "FATAL (reached corrupted marker)";
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

  const TRI_DF_MARKER_HEADER = 10;
  const TRI_DF_MARKER_COL_HEADER = 20;

  if (scan.entries[0].type !== TRI_DF_MARKER_HEADER) {
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

  if (scan.entries.length === 2 && scan.entries[1].type !== TRI_DF_MARKER_COL_HEADER) {
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

  if (type !== "journal" && scan.entries.length === 3 && scan.entries[2].type === 0) {
    // got the two initial header markers but nothing else...
    statusMessage = "datafile is empty but not sealed";
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
  
  if (details) {
    // print details
    printf("Entries\n");
    if (details === "FULL") {
      // print all markers
      PrintEntries(scan.entries, scan.entries.length);
    }
    else {
      // print an excerpt of the markers
      PrintEntries(scan.entries, 10);
      if (scan.entries.length > 20) {
        printf("...\n");
      }
      PrintEntries(scan.entries, -10);
    }
  }

  if (scan.status === 1 && scan.isSealed) {
    return;
  }

  DeepCheckDatafile(collection, type, datafile, scan);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a collection
////////////////////////////////////////////////////////////////////////////////

function CheckCollection (collection, issues, details) {
  printf("Database\n");
  printf("  name: %s\n", internal.db._name());
  printf("  path: %s\n", internal.db._path());
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
    CheckDatafile(collection, "journal", datafiles.journals[i], issues, details);
  }

  for (var i = 0;  i < datafiles.datafiles.length;  ++i) {
    CheckDatafile(collection, "datafile", datafiles.datafiles[i], issues, details);
  }

  for (var i = 0;  i < datafiles.compactors.length;  ++i) {
    CheckDatafile(collection, "compactor", datafiles.compactors[i], issues, details);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief select and check a collection
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  printf("%s\n", "    ___      _         __ _ _           ___  ___    ___ ");
  printf("%s\n", "   /   \\__ _| |_ __ _ / _(_) | ___     /   \\/ __\\  / _ \\");
  printf("%s\n", "  / /\\ / _` | __/ _` | |_| | |/ _ \\   / /\\ /__\\// / /_\\/");
  printf("%s\n", " / /_// (_| | || (_| |  _| | |  __/  / /_// \\/  \\/ /_\\\\ ");
  printf("%s\n", "/___,' \\__,_|\\__\\__,_|_| |_|_|\\___| /___,'\\_____/\\____/ ");
  printf("\n");
  
  if (!internal.db._engine().supports.dfdb) {
    printf("\narango-dfdb is not intended to be used with this storage engine (" + internal.db._engine().name + ").\n");
    return;
  }

  var databases = internal.db._databases();
  var i;

  var collectionSorter = function (l, r) {
    var lName = l.name().toLowerCase();
    var rName = r.name().toLowerCase();

    if (lName !== rName) {
      return lName < rName ? -1 : 1;
    }

    return 0;
  };

  var pad = function (s, l) {
    if (s.length < l) {
      s += Array(l - s.length).join(" ");
    }
    return s;
  };

  if (databases.length == 0) {
    printf("No databases available. Exiting\n");
    return;
  }

  databases.sort();
  printf("Available databases:\n");

  for (i = 0;  i < databases.length;  ++i) {
    printf("  %d: %s\n", i, pad(databases[i], 4));
  }

  var line;

  printf("Database to check: ");
  while (true) {
    line = console.getline();

    if (line == "") {
      printf("Exiting. Please wait.\n");
      return;
    }
    else {
      var l = parseInt(line);

      if (l < 0 || l >= databases.length || l === null || l === undefined || isNaN(l)) {
        printf("Please select a number between 0 and %d: ", databases.length - 1);
      }
      else {
        internal.db._useDatabase(databases[l]);
        break;
      }
    }
  }

  printf("\n");


  var collections = internal.db._collections();
  if (collections.length == 0) {
    printf("No collections available. Exiting\n");
    return;
  }

  // sort the collections
  collections.sort(collectionSorter);

  printf("Available collections:\n");

  for (i = 0;  i < collections.length;  ++i) {
    printf("  %d: %s (%s)\n", pad(i, 4), pad(collections[i].name(), 40), collections[i]._id);
  }

  printf("  *: all\n");

  printf("\n");

  printf("Collection to check: ");
  var a = [];

  while (true) {
    line = console.getline();

    if (line == "") {
      printf("Exiting. Please wait.\n");
      return;
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

  printf("\n");
  printf("Prints details (Y/N/full)? ");

  var details = false;

  while (true) {
    line = console.getline();

    if (line === "") {
      printf("Exiting. Please wait.\n");
      return;
    }

    line = line.toUpperCase();
    if (line === "Y" || line === "YES") {
      details = true;
    }
    else if (line === "F" || line === "FULL") {
      details = "FULL";
    }

    break;
  }

  var issues = [ ];

  for (i = 0;  i < a.length;  ++i) {
    var collection = collections[a[i]];

    printf("Checking collection #%d: %s\n", a[i], collection.name());
    printf("-------------------------------------------------------------------\n");

    UnloadCollection(collection);
    printf("\n");

    CheckCollection(collection, issues, details);
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
  else {
    printf("\nNo issues found.\n");
  }
}

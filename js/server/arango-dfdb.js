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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief C-like printf to stdout
////////////////////////////////////////////////////////////////////////////////

function printf () {
  var text = internal.sprintf.apply(internal.springf, arguments);

  internal.output(text);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile deeply
////////////////////////////////////////////////////////////////////////////////

function DeepCheckDatafile (collection, type, datafile, scan) {
  var entries = scan.entries;

  printf("Entries\n");

  var lastGood = 0;
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

    if (entry.status === 1 || entry.status == 2) {
      if (stillGood) {
        lastGood = entry;
      }
    }
    else {
      stillGood = false;
    }
  }

  printf("Last good position: %d\n", lastGood.position + lastGood.size);

  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a datafile
////////////////////////////////////////////////////////////////////////////////

function CheckDatafile (collection, type, datafile) {
  printf("Datafile\n");
  printf("  path: %s\n", datafile);
  printf("  type: %s\n", type);

  var scan = collection.datafileScan(datafile);

  printf("  current size: %d\n", scan.currentSize);
  printf("  maximal size: %d\n", scan.maximalSize);
  printf("  total used: %d\n", scan.endPosition);
  printf("  # of entries: %d\n", scan.numberMarkers);

  switch (scan.status) {
    case 1:
      printf("  status: OK\n");
      break;

    case 2:
      printf("  status: NOT OK (reached empty marker)\n");
      break;

    case 3:
      printf("  status: FATAL (reached corrupt marker)\n");
      break;

    case 4:
      printf("  status: FATAL (crc failed)\n");
      break;

    default:
      printf("  status: UNKNOWN (%d)\n", scan.status);
      break;
  }

  printf("\n");

  if (scan.numberMarkers === 0) {
    printf("WARNING: datafile contains no entries!\n");
    RemoveDatafile(collection, type, datafile);
    return;
  }

  if (scan.entries[0].type !== 1000) {
    printf("WARNING: datafile contains no header marker!\n");
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

function CheckCollection (collection) {
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
    CheckDatafile(collection, "journal", datafiles.journals[i]);
  }

  for (var i = 0;  i < datafiles.datafiles.length;  ++i) {
    CheckDatafile(collection, "datafiles", datafiles.datafiles[i]);
  }

  for (var i = 0;  i < datafiles.compactors.length;  ++i) {
    CheckDatafile(collection, "compactor", datafiles.compactors[i]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief select and check a collection
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  var argc = argv.length;
  var collections = internal.db._collections();

  printf("%s\n", "    ___      _         __ _ _           ___  ___    ___ ");
  printf("%s\n", "   /   \\__ _| |_ __ _ / _(_) | ___     /   \\/ __\\  / _ \\");
  printf("%s\n", "  / /\\ / _` | __/ _` | |_| | |/ _ \\   / /\\ /__\\// / /_\\/");
  printf("%s\n", " / /_// (_| | || (_| |  _| | |  __/  / /_// \\/  \\/ /_\\\\ ");
  printf("%s\n", "/___,' \\__,_|\\__\\__,_|_| |_|_|\\___| /___,'\\_____/\\____/ ");
  printf("\n");

  printf("Available collections:\n");

  for (var i = 0;  i < collections.length;  ++i) {
    printf("  %d: %s\n", i, collections[i].name());
  }

  printf("\n");

  printf("Collection to check: ");
  var a = 0;
  
  while (true) {
    var a = parseInt(console.getline());

    if (a < 0 || a >= collections.length) {
      printf("Please select a number between 0 and %d: ", collections.length - 1);
    }
    else {
      break;
    }
  }

  printf("Checking collection #%d: %s\n\n", a, collections[a].name());

  CheckCollection(collections[a]);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

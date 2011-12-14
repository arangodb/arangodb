////////////////////////////////////////////////////////////////////////////////
/// @brief system actions for collections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ActionsAdmin
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about all collections
///
/// @REST{GET /_system/collections}
///
/// Returns information about all collections of the database. The returned
/// array contains the following entries.
///
/// - path: The server directory containing the database.
/// - collections : An associative array of all collections.
///
/// An entry of collections is again an associative array containing the
/// following entries.
///
/// - name: The name of the collection.
/// - status: The status of the collection. 1 = new born, 2 = unloaded,
///     3 = loaded, 4 = corrupted.
///
/// @verbinclude rest15
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collections",
  function (req, res) {
    var colls;
    var coll;
    var result;

    colls = db._collections();
    result = {
      path : db._path,
      collections : {}
    };

    for (var i = 0;  i < colls.length;  ++i) {
      coll = colls[i];

      result.collections[coll._name] = {
        id : coll._id,
        name : coll._name,
        status : coll.status(),
        figures : coll.figures()
      };
    }

    res.responseCode = 200;
    res.contentType = "application/json";
    res.body = JSON.stringify(result);
  }
);

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @REST{GET /_system/collection/load?collection=@FA{identifier}}
///
/// Loads a collection into memory.
///
/// @verbinclude restX
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collection/load",
  function (req, res) {
    try {
      req.collection.load();

      res.responseCode = 204;
      res.contentType = "application/json";
    }
    catch (err) {
      res.responseCode = 500;
      res.contentType = "application/json";
      res.body = JSON.stringify(err);
    }
  },
  {
    parameters : {
      collection : "collection-identifier"
    }
  }

);

////////////////////////////////////////////////////////////////////////////////
/// @brief information about a collection
///
/// @REST{GET /_system/collection/info?collection=@FA{identifier}}
///
/// Returns information about a collection
///
/// @verbinclude rest16
////////////////////////////////////////////////////////////////////////////////

defineSystemAction("collection/info",
  function (req, res) {
    try {
      result = {};
      result.id = req.collection._id;
      result.name = req.collection._name;
      result.status = req.collection.status();
      result.figures = req.collection.figures();

      res.responseCode = 200;
      res.contentType = "application/json";
      res.body = JSON.stringify(result);
    }
    catch (err) {
      res.responseCode = 500;
      res.contentType = "application/json";
      res.body = JSON.stringify({ 'error' : "" + err });
    }
  },
  {
    parameters : {
      collection : "collection-identifier"
    }
  }

);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

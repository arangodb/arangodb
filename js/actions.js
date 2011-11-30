////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions functions
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
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Json V8 JSON
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as documents
////////////////////////////////////////////////////////////////////////////////

function queryResult (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = query.toArray();

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : time() - t,
    documents : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = toJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a result of a query as references
////////////////////////////////////////////////////////////////////////////////

function queryReferences (req, res, query) {
  var result;
  var offset = 0;
  var page = 0;
  var blocksize = 0;
  var t;
  var result;

  t = time();

  if (req.blocksize) {
    blocksize = req.blocksize;

    if (req.page) {
      page = req.page;
      offset = page * blocksize;
      query = query.skip(offset).limit(blocksize);
    }
    else {
      query = query.limit(blocksize);
    }
  }

  result = [];

  while (query.hasNext()) {
    result.push(query.nextRef());
  }

  result = {
    total : query.count(),
    count : query.count(true),
    offset : offset,
    blocksize : blocksize,
    page : page,
    runtime : time() - t,
    references : result
  };

  res.responseCode = 200;
  res.contentType = "application/json";
  res.body = toJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief import helper
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
/// @author Achim Brandt
/// @author Copyright 2008-2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ImportHelper.h"

#include <sstream>

#include "BasicsC/csv.h"

#include <Basics/StringUtils.h>

#include "Variant/VariantArray.h"
#include "Variant/VariantBoolean.h"
#include "Variant/VariantInt64.h"
#include "Variant/VariantString.h"

#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include "json.h"

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace std;

namespace triagens {
  namespace v8client {

    ////////////////////////////////////////////////////////////////////////////////
    /// constructor and destructor
    ////////////////////////////////////////////////////////////////////////////////

    ImportHelper::ImportHelper (httpclient::SimpleHttpClient* _client, size_t maxUploadSize)
    : _client (_client), _maxUploadSize (maxUploadSize) {
      _quote = '"';
      _separator = ',';
      regcomp(&_doubleRegex, "^[-+]?([0-9]+\\.?[0-9]*|\\.[0-9]+)([eE][-+]?[0-8]+)?$", REG_ICASE | REG_EXTENDED);
      regcomp(&_intRegex, "^[-+]?([0-9]+)$", REG_ICASE | REG_EXTENDED);
    }

    ImportHelper::~ImportHelper () {
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// public functions
    ////////////////////////////////////////////////////////////////////////////////

    bool ImportHelper::importCsv (const string& collectionName, const string& fileName) {
      _collectionName = collectionName;
      _firstLine = "";
      _numberLines = 0;
      _numberOk = 0;
      _numberError = 0;
      _outputBuffer.clear();
      _lineBuffer.clear();
      _errorMessage = "";

      // read and convert
      int fd = open(fileName.c_str(), O_RDONLY);

      if (fd < 0) {
        _errorMessage = TRI_LAST_ERROR_STR;
        return false;
      }

      TRI_csv_parser_t parser;

      TRI_InitCsvParser(&parser,
              ProcessCsvBegin,
              ProcessCsvAdd,
              ProcessCsvEnd);

      parser._separator = _separator;
      parser._quote = _quote;
      parser._dataAdd = this;

      char buffer[10240];

      while (true) {
        v8::HandleScope scope;

        ssize_t n = read(fd, buffer, sizeof (buffer));

        if (n < 0) {
          TRI_DestroyCsvParser(&parser);
          _errorMessage = TRI_LAST_ERROR_STR;
          return false;
        }
        else if (n == 0) {
          break;
        }

        TRI_ParseCsvString2(&parser, buffer, n);
      }

      if (_outputBuffer.length() > 0) {
        sendCsvBuffer();
      }

      TRI_DestroyCsvParser(&parser);

      close(fd);

      return true;
    }

    bool ImportHelper::importJson (const string& collectionName, const string& fileName) {
      _collectionName = collectionName;
      _firstLine = "";
      _numberLines = 0;
      _numberOk = 0;
      _numberError = 0;
      _outputBuffer.clear();
      _errorMessage = "";

      // read and convert
      int fd = open(fileName.c_str(), O_RDONLY);

      if (fd < 0) {
        _errorMessage = TRI_LAST_ERROR_STR;
        return false;
      }

      char buffer[10240];

      while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n < 0) {
          _errorMessage = TRI_LAST_ERROR_STR;
          return false;
        }
        else if (n == 0) {
          break;
        }

        _outputBuffer.appendText(buffer, n);

        if (_outputBuffer.length() > _maxUploadSize) {
          // send all data before last '\n'

          const char* first = _outputBuffer.c_str();
          char* pos = (char*) memrchr(first, '\n', _outputBuffer.length());

          if (pos != 0) {
            size_t len = pos - first + 1;
            sendJsonBuffer(first, len);
            _outputBuffer.erase_front(len);
          }

        }
      }

      if (_outputBuffer.length() > 0) {
        sendJsonBuffer(_outputBuffer.c_str(), _outputBuffer.length());
      }

      _numberLines = _numberError + _numberOk;
      
      close(fd);

      return true;
    }



    ////////////////////////////////////////////////////////////////////////////////
    /// private functions
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief start a new csv line
    ////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvBegin (TRI_csv_parser_t* parser, size_t row) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) ih->beginLine(row);
    }

    void ImportHelper::beginLine(size_t row) {
      if (_lineBuffer.length() > 0) {
        // error
        ++_numberError;
        _lineBuffer.clear();        
      }
      
      ++_numberLines;
      
      if (row > 0) {
        _lineBuffer.appendChar('\n');
      }
      _lineBuffer.appendChar('[');      
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief adds a new CSV field
    ////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) ih->addField(field, row, column, escaped);
    }

    void ImportHelper::addField (char const* field, size_t row, size_t column, bool escaped) {
      if (column > 0) {
        _lineBuffer.appendChar(',');
      }

      if (row == 0) {
        // head line    
        _lineBuffer.appendChar('"');
        _lineBuffer.appendText(StringUtils::escapeUnicode(field));
        _lineBuffer.appendChar('"');
      }
      else {
        if (escaped) {
          _lineBuffer.appendChar('"');
          _lineBuffer.appendText(StringUtils::escapeUnicode(field));
          _lineBuffer.appendChar('"');
        }
        else {
          string s(field);
          if (s.length() == 0) {
            // do nothing
            _lineBuffer.appendText("null");
          }
          else if ("true" == s || "false" == s) {
            _lineBuffer.appendText(field);
          }
          else {
            if (regexec(&_intRegex, s.c_str(), 0, 0, 0) == 0) {
              int64_t num = StringUtils::int64(s);
              _lineBuffer.appendInteger(num);
            }
            else if (regexec(&_doubleRegex, s.c_str(), 0, 0, 0) == 0) {
              double num = StringUtils::doubleDecimal(s);
              _lineBuffer.appendDecimal(num);
            }
            else {
              _lineBuffer.appendChar('"');
              _lineBuffer.appendText(StringUtils::escapeUnicode(field));
              _lineBuffer.appendChar('"');
            }
          }
        }
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief ends a CSV line
    ////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) ih->addLastField(field, row, column, escaped);
    }

    void ImportHelper::addLastField (char const* field, size_t row, size_t column, bool escaped) {
      if (column == 0 && StringUtils::trim(field) == "") {
        // ignore empty line
        _lineBuffer.clear();
        return;
      }

      addField(field, row, column, escaped);
            
      _lineBuffer.appendChar(']');      

      if (row == 0) {
        // save the first line
        _firstLine = _lineBuffer.c_str();
      }
      else if (row > 0 && _firstLine == "") {
        // error
        ++_numberError;
        _lineBuffer.clear();
        return;
      }
      
      // read a complete line
      
      if (_lineBuffer.length() > 0) {
        _outputBuffer.appendText(_lineBuffer);
        _lineBuffer.clear();        
      }
      else {
        ++_numberError;
      }
      
      if (_outputBuffer.length() > _maxUploadSize) {
        sendCsvBuffer();
        _outputBuffer.appendText(_firstLine);
      }

    }

    void ImportHelper::sendCsvBuffer () {
      map<string, string> headerFields;
      SimpleHttpResult* result = _client->request(SimpleHttpClient::POST, "/_api/import?collection=" + StringUtils::urlEncode(_collectionName), _outputBuffer.c_str(), _outputBuffer.length(), headerFields);

      handleResult(result);

      _outputBuffer.clear();
    }

    void ImportHelper::sendJsonBuffer (char const* str, size_t len) {
      map<string, string> headerFields;
      SimpleHttpResult* result = _client->request(SimpleHttpClient::POST, "/_api/import?type=documents&collection=" + StringUtils::urlEncode(_collectionName), str, len, headerFields);

      handleResult(result);
    }

    void ImportHelper::handleResult (SimpleHttpResult* result) {
      if (!result) {
        return;
      }

      VariantArray* va = result->getBodyAsVariantArray();

      if (va) {
        VariantBoolean* vb = va->lookupBoolean("error");
        if (vb && vb->getValue()) {
          // is error

        }

        VariantInt64* vi = va->lookupInt64("created");
        if (vi && vi->getValue() > 0) {
          _numberOk += (size_t) vi->getValue();
        }

        vi = va->lookupInt64("errors");
        if (vi && vi->getValue() > 0) {
          _numberError += (size_t) vi->getValue();
        }

        delete va;
      }

      delete result;
    }


  }
}

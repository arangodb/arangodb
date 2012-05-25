////////////////////////////////////////////////////////////////////////////////
/// @brief V8 client connection
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

#ifndef TRIAGENS_V8_CLIENT_CONNECTION_H
#define TRIAGENS_V8_CLIENT_CONNECTION_H 1

#include <Basics/Common.h>
#include <v8.h>

#include <string>
#include <map>
#include <sstream>

namespace triagens {
  namespace httpclient {
    class SimpleHttpClient;
    class SimpleHttpResult;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace v8client {

    class V8ClientConnection {
    private:
      V8ClientConnection (V8ClientConnection const&);
      V8ClientConnection& operator= (V8ClientConnection const&);

    public:
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructor
      ///
      /// @param string hostname            server hostname
      /// @param int port                   server port
      /// @param double requestTimeout      timeout in seconds for one request
      /// @param size_t retries             maximum number of request retries
      /// @param double connTimeout         timeout in seconds for the tcp connect 
      ///
      ////////////////////////////////////////////////////////////////////////////////

      V8ClientConnection (const string& hostname, 
                        int port, 
                        double requestTimeout,
                        size_t retries,
                        double connectionTimeout,
                        bool warn);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructor
      ////////////////////////////////////////////////////////////////////////////////

      ~V8ClientConnection ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if it is connected
      ////////////////////////////////////////////////////////////////////////////////

      bool isConnected () {
        return _connected;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the version and build number of the arango server
      ////////////////////////////////////////////////////////////////////////////////

      const string& getVersion () {
        return _version;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief do a "GET" request
      ///
      /// @param string location                     the request location
      /// @param map<string, string> headerFields    additional header fields
      ///
      /// @return v8::Value                          a V8 JavaScript object
      ////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> getData (std::string const& location, map<string, string> const& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief do a "DELETE" request
      ///
      /// @param string location                     the request location
      /// @param map<string, string> headerFields    additional header fields
      ///
      /// @return v8::Value                          a V8 JavaScript object
      ////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> deleteData (std::string const& location, map<string, string> const& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief do a "HEAD" request
      ///
      /// @param string location                     the request location
      /// @param map<string, string> headerFields    additional header fields
      ///
      /// @return v8::Value                          a V8 JavaScript object
      ////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> headData (std::string const& location, map<string, string> const& headerFields);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief do a "POST" request
      ///
      /// @param string location                     the request location
      /// @param string body                         the request body
      /// @param map<string, string> headerFields    additional header fields
      ///
      /// @return v8::Value                          a V8 JavaScript object
      ////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> postData (std::string const& location, std::string const& body, map<string, string> const& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief do a "PUT" request
      ///
      /// @param string location                     the request location
      /// @param string body                         the request body
      /// @param map<string, string> headerFields    additional header fields
      ///
      /// @return v8::Value                          a V8 JavaScript object
      ////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> putData (std::string const& location, std::string const& body, map<string, string> const& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the last http return code
      ///
      /// @return int          the code
      ////////////////////////////////////////////////////////////////////////////////

      int getLastHttpReturnCode () {
        return _lastHttpReturnCode;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the last error message
      ///
      /// @return string          the error message
      ////////////////////////////////////////////////////////////////////////////////

      const std::string& getErrorMessage () {
        return _lastErrorMessage;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the hostname 
      ///
      /// @return string          the server name
      ////////////////////////////////////////////////////////////////////////////////

      const std::string& getHostname ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the port
      ///
      /// @return string          the server port
      ////////////////////////////////////////////////////////////////////////////////
      
      int getPort ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the simple http client
      ///
      /// @return triagens::httpclient::SimpleHttpClient*    then client connection
      ////////////////////////////////////////////////////////////////////////////////
            
      triagens::httpclient::SimpleHttpClient* getHttpClient() {
        return _client;
      }

    private:
      
      v8::Handle<v8::Value> requestData (int method, std::string const& location, std::string const& body, map<string, string> const& headerFields);
      
    private:
      std::string _version;
      bool _connected;

      int _lastHttpReturnCode;
      std::string _lastErrorMessage;
      
      triagens::httpclient::SimpleHttpClient* _client;
      
      triagens::httpclient::SimpleHttpResult* _httpResult;
    };
  }
}
#endif	/* HTTPCLIENT_H */


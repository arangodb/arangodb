////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H 1

#include <Basics/Common.h>

#include <string>
#include <map>
#include <sstream>

namespace triagens {
  namespace httpclient {
    class SimpleHttpConnection;
    class SimpleHttpResult;        
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace httpclient {

    class SimpleHttpClient {
    private:
      SimpleHttpClient (SimpleHttpClient const&);
      SimpleHttpClient& operator= (SimpleHttpClient const&);
      
    public:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief supported request methods
      ////////////////////////////////////////////////////////////////////////////////

      enum http_method {
        GET, POST, PUT, DELETE, HEAD
      };

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief http status codes
      ////////////////////////////////////////////////////////////////////////////////

      enum http_status_codes {
        HTTP_STATUS_OK = 200,
        HTTP_STATUS_CREATED = 201,
        HTTP_STATUS_ACCEPTED = 202,
        HTTP_STATUS_PARTIAL = 203,
        HTTP_STATUS_NO_RESPONSE = 204,

        HTTP_STATUS_MOVED_PERMANENTLY = 301,
        HTTP_STATUS_FOUND = 302,
        HTTP_STATUS_SEE_OTHER = 303,
        HTTP_STATUS_NOT_MODIFIED = 304,
        HTTP_STATUS_TEMPORARY_REDIRECT = 307,

        HTTP_STATUS_BAD = 400,
        HTTP_STATUS_UNAUTHORIZED = 401,
        HTTP_STATUS_PAYMENT = 402,
        HTTP_STATUS_FORBIDDEN = 403,
        HTTP_STATUS_NOT_FOUND = 404,
        HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
        HTTP_STATUS_UNPROCESSABLE_ENTITY = 422,

        HTTP_STATUS_SERVER_ERROR = 500,
        HTTP_STATUS_NOT_IMPLEMENTED = 501,
        HTTP_STATUS_BAD_GATEWAY = 502,
        HTTP_STATUS_SERVICE_UNAVAILABLE = 503 // Retry later
      };

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

      SimpleHttpClient (const string& hostname, 
                        int port, 
                        double requestTimeout,
                        size_t retries,
                        double connectionTimeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructor
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleHttpClient ();

    public:
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief make a http request
      ///
      /// @param int method                           http method
      /// @param string location                      request location
      /// @param char* body                           request body
      /// @param size_t bodyLength                    body length
      /// @param map<string, string> headerFields     header fields
      ///
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult* request (int method, 
                                 const string& location,
                                 const char* body, 
                                 size_t bodyLength,
                                 const map<string, string>& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get last connection error message
      ///
      /// @return string          the last error message
      ////////////////////////////////////////////////////////////////////////////////
      
      const std::string& getErrorMessage ();
      
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
      /// @brief returns true if the client is connected
      ///
      /// @return bool          true if the client is connected
      ////////////////////////////////////////////////////////////////////////////////
      
      bool isConnected ();

      
    private:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief builds the http request
      ////////////////////////////////////////////////////////////////////////////////
      
      void fillRequestBuffer (stringstream &buffer,
                              int method,
                              const string& location,
                              const char* body, size_t bodyLenght,
                              const map<string, string>& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief writes the request to the socket
      ////////////////////////////////////////////////////////////////////////////////
      
      bool write (stringstream &buffer, double timeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief reads the answer
      ////////////////////////////////////////////////////////////////////////////////
      bool read (SimpleHttpResult* result, double timeout);

      bool readHeader (SimpleHttpResult* result, double timeout);

      bool readBody (SimpleHttpResult* result, double timeout);
      bool readBodyContentLength (SimpleHttpResult* result, double timeout);
      bool readBodyChunked (SimpleHttpResult* result, double timeout);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief connect to server if not connected
      ////////////////////////////////////////////////////////////////////////////////

      bool checkConnection ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief close connection (after error)
      ////////////////////////////////////////////////////////////////////////////////

      void closeConnection ();

      double now ();
            
    private:
      double _requestTimeout;
      size_t _retries;

      
      int _lastError;
      std::string _lastErrorMessage;
      
      SimpleHttpConnection* _connection;      
    };
  }
}
#endif	/* HTTPCLIENT_H */


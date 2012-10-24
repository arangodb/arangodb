////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint, Unix domain socket
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "EndpointUnixDomain.h"

#include <Basics/Common.h>
#include <Basics/FileUtils.h>
#include <Basics/StringUtils.h>
#include <Logger/Logger.h>

#include "Rest/Endpoint.h"

using namespace triagens::basics;
using namespace triagens::rest;

#ifdef TRI_HAVE_LINUX_SOCKETS

// -----------------------------------------------------------------------------
// --SECTION--                                                EndpointUnixDomain
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointUnixDomain::EndpointUnixDomain (const Endpoint::EndpointType type, 
                                        const Endpoint::ProtocolType protocol,
                                        const std::string& specification, 
                                        int listenBacklog,
                                        const std::string& path) :
    Endpoint(type, DOMAIN_UNIX, protocol, ENCRYPTION_NONE, specification, listenBacklog),
    _path(path) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a Unix socket endpoint
////////////////////////////////////////////////////////////////////////////////

EndpointUnixDomain::~EndpointUnixDomain () {
  if (_connected) {
    disconnect();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Rest
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief connect the endpoint
////////////////////////////////////////////////////////////////////////////////

socket_t EndpointUnixDomain::connect (double connectTimeout, double requestTimeout) {
  LOGGER_DEBUG << "connecting to unix endpoint " << _specification;

  assert(_socket == 0);
  assert(!_connected);

  if (_type == ENDPOINT_SERVER && FileUtils::exists(_path)) {
    // socket file already exists
    LOGGER_WARNING << "socket file '" << _path << "' already exists.";

    int error = 0;
    // delete previously existing socket file
    if (FileUtils::remove(_path, &error)) {
      LOGGER_WARNING << "deleted previously existing socket file '" << _path << "'.";
    }
    else {
      LOGGER_ERROR << "unable to delete previously existing socket file '" << _path << "'.";
    
      return 0;
    }
  }

  int listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listenSocket == -1) {
    return 0;
  }
  
  // reuse address
  int opt = 1;
    
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*> (&opt), sizeof (opt)) == -1) {
    LOGGER_ERROR << "setsockopt failed with " << errno << " (" << strerror(errno) << ")";

    close(listenSocket);

    return 0;
  }
  LOGGER_TRACE << "reuse address flag set";
    
  struct sockaddr_un address;

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, 100, "%s", _path.c_str());

  if (_type == ENDPOINT_SERVER) {
    int result = bind(listenSocket, (struct sockaddr*) &address, SUN_LEN(&address));
    if (result != 0) {
      // bind error
      close(listenSocket);

      return 0;
    }
    
    // listen for new connection, executed for server endpoints only
    LOGGER_TRACE << "using backlog size " << _listenBacklog;
    result = listen(listenSocket, _listenBacklog);

    if (result < 0) {
      close(listenSocket);

      LOGGER_ERROR << "listen failed with " << errno << " (" << strerror(errno) << ")";

      return 0;
    }
  }
  else if (_type == ENDPOINT_CLIENT) {
    // connect to endpoint, executed for client endpoints only

    // set timeout
    setTimeout(listenSocket, connectTimeout);

    if (::connect(listenSocket, (const struct sockaddr*) &address, SUN_LEN(&address)) != 0) {
      close(listenSocket);

      return 0;
    }
  }
    
  if (!setSocketFlags(listenSocket)) {
    close(listenSocket);

    return 0;
  }
  
  if (_type == ENDPOINT_CLIENT) {
    setTimeout(listenSocket, requestTimeout);
  }
  
  _connected = true;
  _socket = listenSocket;
  
  return _socket;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect the endpoint
////////////////////////////////////////////////////////////////////////////////

void EndpointUnixDomain::disconnect () {
  if (_connected) {
    assert(_socket);

    _connected = false;
    close(_socket);

    _socket = 0;

    if (_type == ENDPOINT_SERVER) {   
      int error = 0; 
      FileUtils::remove(_path, &error);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init an incoming connection
////////////////////////////////////////////////////////////////////////////////

bool EndpointUnixDomain::initIncoming (socket_t incoming) {
  return setSocketFlags(incoming);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

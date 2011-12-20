////////////////////////////////////////////////////////////////////////////////
/// @brief rest server options
///
/// @file
/// This file contains the description of the rest server.
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
/// @author Dr. Frank Celler
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_REST_REST_SERVER_OPTIONS_H
#define TRIAGENS_FYN_REST_REST_SERVER_OPTIONS_H 1

#include <BasicsC/Common.h>

#include <Basics/Exceptions.h>
#include <Rest/RestModel.h>
#include <Rest/RestModelLoader.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup RestServer
    /// @brief rest model options
    ////////////////////////////////////////////////////////////////////////////////

    struct RestServerOptions {

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructor
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~RestServerOptions () {
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns a descriptive name for the server
      ////////////////////////////////////////////////////////////////////////////////

      virtual string serverDescription () = 0;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the function name creating model
      ////////////////////////////////////////////////////////////////////////////////

      virtual string modelName () {
        THROW_INTERNAL_ERROR("no model loader is known");
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns a descriptive name for the model
      ////////////////////////////////////////////////////////////////////////////////

      virtual string modelDescription () {
        return "";
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns a model loader for the model
      ////////////////////////////////////////////////////////////////////////////////

      virtual RestModelLoader& modelLoader () {
        THROW_INTERNAL_ERROR("no model loader is known");
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the loaded model
      ////////////////////////////////////////////////////////////////////////////////

      virtual RestModel* model () {
        THROW_INTERNAL_ERROR("no model loader is known");
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief shows the daemon mode option
      ////////////////////////////////////////////////////////////////////////////////

      virtual bool allowDaemonMode () {
        return true;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief shows the supervisor option
      ////////////////////////////////////////////////////////////////////////////////

      virtual bool allowSupervisor () {
        return false;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief shows the multi server option
      ////////////////////////////////////////////////////////////////////////////////

      virtual bool allowMultiScheduler (RestModel* model) {
        if (model->getExtensions() == 0) {
          return false;
        }
        else {
          return model->getExtensions()->allowMultiScheduler();
        }
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief shows the server port option
      ////////////////////////////////////////////////////////////////////////////////

      virtual bool showPortOptions (RestModel* model) {
        if (model->getExtensions() == 0) {
          return true;
        }
        else {
          return model->getExtensions()->showPortOptions();
        }
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the server version
      ////////////////////////////////////////////////////////////////////////////////

      virtual string serverVersion (RestModel* model) {
        if (model == 0) {
          return "unknown (no model loaded)";
        }
        else if (model->getExtensions() == 0) {
          return "unknown (no extensions)";
        }
        else {
          return model->getExtensions()->serverVersion();
        }
      }
    };
  }
}

#endif


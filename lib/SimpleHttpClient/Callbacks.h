////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SIMPLE_HTTP_CLIENT_CALLBACKS_H
#define ARANGODB_SIMPLE_HTTP_CLIENT_CALLBACKS_H 1

#include "Basics/Common.h"

namespace arangodb {
class GeneralResponse;

namespace communicator {
class Callbacks {
  public:
    typedef std::function<void(int, std::unique_ptr<GeneralResponse>)>
      OnErrorCallback;

    typedef std::function<void(std::unique_ptr<GeneralResponse>)>
      OnSuccessCallback;

    typedef std::function<void(std::function<void()>)> ScheduleMeCallback;

    Callbacks() {}
    Callbacks(OnSuccessCallback onSuccess, OnErrorCallback onError)
      : _onSuccess(onSuccess), _onError(onError), _scheduleMe(defaultScheduleMe)  {
      }

  public:
    OnSuccessCallback _onSuccess;
    OnErrorCallback _onError;
    ScheduleMeCallback _scheduleMe;

  protected:
    static void defaultScheduleMe(std::function<void()> task) {task();}
};
}
}

#endif

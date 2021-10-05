////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RestartAction.h"

// The following is a global pointer which can be set from within the process
// to configure a restart action which happens directly before main()
// terminates. This is used for our hotbackup restore functionality.
// See below in main() for details.

namespace arangodb {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::function<int()>* restartAction = nullptr;
}

// Here is a sample of how to use this:
//
// extern std::function<int()>* restartAction;
//
// static int myRestartAction() {
//   std::cout << "Executing restart action..." << std::endl;
//   return 0;
// }
//
// And then in some function:
//
// restartAction = new std::function<int()>();
// *restartAction = myRestartAction;
// arangodb::ApplicationServer::server->beginShutdown();
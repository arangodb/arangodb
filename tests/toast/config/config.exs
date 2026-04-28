################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

import Config

# Run exec-port in its own process group so it doesn't receive terminal
# SIGINT. This ensures child process cleanup when the VM terminates.
config :erlexec, portexe: Path.expand("../priv/exec-port-wrapper", __DIR__)

# Global log level: :debug — file handler will get everything.
# Console handler is restricted to :info below.
config :logger, level: :debug

# Console handler: only show info and above, with our custom format.
config :logger, :default_handler, level: :info

config :logger, :default_formatter, format: "$time $metadata[$level] $message\n"

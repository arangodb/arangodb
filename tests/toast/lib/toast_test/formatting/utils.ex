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

defmodule ToastTest.Formatting.Utils do
  @moduledoc false

  def print_header(label, true, color) do
    IO.ANSI.format([
      IO.ANSI.color_background(color),
      IO.ANSI.bright(),
      "\n  ",
      String.replace(label, "\n", "\e[K\n"),
      :reset,
      "\n"
    ])
    |> IO.puts()
  end

  def print_header(label, false, _color) do
    bar = String.duplicate("\u2550", 80)
    IO.puts("\n#{bar}")
    IO.puts(label)
    IO.puts(bar)
  end
end

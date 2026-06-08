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

defmodule Toast.Application do
  @moduledoc false

  use Application

  require Logger

  @impl true
  def start(_type, _args) do
    unless Toast.Env.loaded?(), do: Toast.Env.load() |> Toast.Env.apply!()
    setup_file_logger()
    Toast.Deployment.init_counter()

    Logger.info("Starting Toast 🍞")

    children = [
      {Toast.PortAllocator, []},
      {Toast.Process.Supervisor, []},
      {Toast.Deployment.Supervisor, []},
      # ToastTest.Supervisor owns the ETS-backed state processes (Abort, DeploymentRegistry).
      # They live here rather than under a ToastTest runner process so that their lifetime
      # is tied to the OTP application, not to any individual suite run. This guarantees
      # stable table ownership even if a runner process crashes mid-run, and also covers
      # interactive sessions where ToastTest.Runner is never started at all.
      {ToastTest.Supervisor, []}
    ]

    opts = [strategy: :one_for_one, name: Toast.Supervisor]
    Supervisor.start_link(children, opts)
  end

  def reconfigure_file_logger(result_dir) do
    File.mkdir_p!(result_dir)
    log_file = result_dir |> Path.join("toast.log") |> String.to_charlist()
    :logger.update_handler_config(:toast_file, :config, %{file: log_file})
  catch
    kind, reason ->
      Logger.warning(
        "Failed to reconfigure file logger for #{result_dir}: #{inspect(kind)}: #{inspect(reason)}"
      )

      :ok
  end

  defp setup_file_logger do
    result_dir = Application.get_env(:toast, :result_dir, Toast.Env.default_result_dir())
    File.mkdir_p!(result_dir)
    log_path = Path.join(result_dir, "toast.log")

    handler_config = %{
      config: %{file: String.to_charlist(log_path)},
      level: :debug,
      formatter: {Toast.LogFormatter, %{}}
    }

    case :logger.add_handler(:toast_file, :logger_std_h, handler_config) do
      :ok -> :ok
      {:error, {:already_exist, _}} -> :ok
    end
  end
end

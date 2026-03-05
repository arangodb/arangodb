defmodule Toast.Application do
  @moduledoc false

  use Application

  @impl true
  def start(_type, _args) do
    setup_file_logger()

    children = [
      {Toast.PortAllocator, []},
      {Toast.Process.Supervisor, []},
      {Toast.Deployment.Supervisor, []}
    ]

    opts = [strategy: :one_for_one, name: Toast.Supervisor]
    Supervisor.start_link(children, opts)
  end

  def reconfigure_file_logger(result_dir) do
    File.mkdir_p!(result_dir)
    log_file = Path.join(result_dir, "toast.log") |> String.to_charlist()
    :logger.update_handler_config(:toast_file, :config, %{file: log_file})
  catch
    _, _ -> :ok
  end

  defp setup_file_logger do
    result_dir = Application.get_env(:toast, :result_dir, "toast-results")
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

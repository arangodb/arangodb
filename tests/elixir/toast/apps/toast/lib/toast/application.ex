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

  defp setup_file_logger do
    result_dir = Toast.ResultExporter.result_dir()
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

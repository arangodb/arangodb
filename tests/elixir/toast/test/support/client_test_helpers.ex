defmodule Toast.ClientTestHelpers do
  @moduledoc false

  alias Toast.Client

  def client_with_plug(plug, opts \\ []) do
    Client.new("http://localhost:8529", [{:plug, plug} | opts])
  end

  def json_plug(status \\ 200, body \\ %{}) do
    fn conn ->
      conn
      |> Plug.Conn.put_resp_content_type("application/json")
      |> Plug.Conn.send_resp(status, Jason.encode!(body))
    end
  end
end

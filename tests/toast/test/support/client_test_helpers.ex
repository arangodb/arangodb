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

  def vpack_plug(status \\ 200, body \\ %{}) do
    fn conn ->
      conn
      |> Plug.Conn.put_resp_content_type("application/x-velocypack")
      |> Plug.Conn.send_resp(status, VelocyPack.encode!(body))
    end
  end

  @doc "Reads and decodes request body based on content-type (JSON or vpack)."
  def decode_request_body(conn) do
    {:ok, raw, conn} = Plug.Conn.read_body(conn)

    decoded =
      case Plug.Conn.get_req_header(conn, "content-type") do
        ["application/x-velocypack" <> _ | _] -> VelocyPack.decode!(raw)
        _ -> Jason.decode!(raw)
      end

    {decoded, conn}
  end

  @doc "Sends response encoded to match the request's accept header."
  def send_encoded_response(conn, status, body) do
    case Plug.Conn.get_req_header(conn, "accept") do
      ["application/x-velocypack" <> _ | _] ->
        conn
        |> Plug.Conn.put_resp_content_type("application/x-velocypack")
        |> Plug.Conn.send_resp(status, VelocyPack.encode!(body))

      _ ->
        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(status, Jason.encode!(body))
    end
  end
end

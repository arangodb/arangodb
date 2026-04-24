defmodule ToastTest.TimeTestHelpers do
  @moduledoc false

  def to_us(%DateTime{} = dt), do: DateTime.to_unix(dt, :microsecond)
end

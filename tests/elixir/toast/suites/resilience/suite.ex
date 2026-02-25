defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2
end

defmodule Toast.MixProject do
  use Mix.Project

  def project do
    [
      app: :toast,
      version: "0.1.0",
      elixir: "~> 1.19",
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger],
      mod: {Toast.Application, []}
    ]
  end

  defp deps do
    [
      {:req, "~> 0.5"},
      {:erlexec, "~> 2.0"},
      {:joken, "~> 2.6"},
      {:plug, "~> 1.0", only: :test}
    ]
  end
end

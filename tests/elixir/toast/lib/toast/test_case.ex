defmodule Toast.TestCase do
  @moduledoc deprecated: "Use ToastTest.Case instead"

  defmacro __using__(opts) do
    quote do
      use ToastTest.Case, unquote(opts)
    end
  end

  defdelegate setup_suite!(), to: ToastTest.Case
  defdelegate setup_suite!(mode), to: ToastTest.Case
  defdelegate setup_suite!(mode, opts), to: ToastTest.Case
  defdelegate setup_suite(), to: ToastTest.Case
  defdelegate setup_suite(mode), to: ToastTest.Case
  defdelegate setup_suite(mode, opts), to: ToastTest.Case
  defdelegate register_deployment(deployment), to: ToastTest.Case
  defdelegate get_deployment(), to: ToastTest.Case
end

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

defmodule ToastTest.JavascriptSuiteTest do
  use ExUnit.Case, async: true

  test "defines deployment_config with defaults" do
    defmodule BasicJsSuite do
      use ToastTest.JavascriptSuite, paths: ["some/path"]
    end

    config = BasicJsSuite.deployment_config()
    assert Keyword.get(config, :mode) == :auto
    assert Keyword.get(config, :authentication) == false
  end

  test "passes suite options through to deployment_config" do
    defmodule ClusterJsSuite do
      use ToastTest.JavascriptSuite,
        paths: ["some/path"],
        mode: :cluster,
        server_args: %{"log.level" => "debug"}
    end

    config = ClusterJsSuite.deployment_config()
    assert Keyword.get(config, :mode) == :cluster
    assert Keyword.get(config, :server_args) == %{"log.level" => "debug"}
  end

  test "stores JS paths via __toast_js_paths__/0" do
    defmodule PathJsSuite do
      use ToastTest.JavascriptSuite, paths: ["tests/js/client/aql", "tests/js/common/aql"]
    end

    assert PathJsSuite.__toast_js_paths__() == ["tests/js/client/aql", "tests/js/common/aql"]
  end

  test "implements ToastTest.Suite behaviour" do
    defmodule BehaviourJsSuite do
      use ToastTest.JavascriptSuite, paths: ["p"]
    end

    behaviours = BehaviourJsSuite.__info__(:attributes)[:behaviour] || []
    assert ToastTest.Suite in behaviours
  end

  test "stores extra args via __toast_js_extra_args__/0" do
    defmodule ArgsJsSuite do
      use ToastTest.JavascriptSuite,
        paths: ["p"],
        js_extra_args: %{"agency.supervision-ok-threshold" => "15"}
    end

    assert ArgsJsSuite.__toast_js_extra_args__() == %{
             "agency.supervision-ok-threshold" => "15"
           }
  end

  test "extra args default to empty map" do
    defmodule NoArgsJsSuite do
      use ToastTest.JavascriptSuite, paths: ["p"]
    end

    assert NoArgsJsSuite.__toast_js_extra_args__() == %{}
  end

  test "stores weights via __toast_js_weights__/0" do
    defmodule WeightedJsSuite do
      use ToastTest.JavascriptSuite,
        paths: ["p"],
        weights: %{"heavy-test.js" => 10, "light-test.js" => 2}
    end

    assert WeightedJsSuite.__toast_js_weights__() == %{
             "heavy-test.js" => 10,
             "light-test.js" => 2
           }
  end

  test "weights default to empty map" do
    defmodule NoWeightsJsSuite do
      use ToastTest.JavascriptSuite, paths: ["p"]
    end

    assert NoWeightsJsSuite.__toast_js_weights__() == %{}
  end
end

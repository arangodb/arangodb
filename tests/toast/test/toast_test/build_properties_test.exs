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

defmodule ToastTest.BuildPropertiesTest do
  use ExUnit.Case, async: true

  alias ToastTest.BuildProperties

  @sample_output """
  arm: false
  asan: false
  tsan: false
  coverage: false
  enterprise-version: enterprise
  failure-tests: true
  license: enterprise
  replication2-enabled: true
  v8-version: 12.1.165
  """

  describe "parse/1" do
    test "parses standard version output" do
      props = BuildProperties.parse(@sample_output)

      assert props.arm == false
      assert props.asan == false
      assert props.tsan == false
      assert props.coverage == false
      assert props.failure_tests == true
      assert props.enterprise == true
      assert props.replication2 == true
      assert props.v8 == true
    end

    test "detects asan build" do
      output = String.replace(@sample_output, "asan: false", "asan: true")
      props = BuildProperties.parse(output)
      assert props.asan == true
    end

    test "detects tsan build" do
      output = String.replace(@sample_output, "tsan: false", "tsan: true")
      props = BuildProperties.parse(output)
      assert props.tsan == true
    end

    test "detects coverage build" do
      output = String.replace(@sample_output, "coverage: false", "coverage: true")
      props = BuildProperties.parse(output)
      assert props.coverage == true
    end

    test "detects no failure points" do
      output = String.replace(@sample_output, "failure-tests: true", "failure-tests: false")
      props = BuildProperties.parse(output)
      assert props.failure_tests == false
    end

    test "stores raw key-value pairs" do
      props = BuildProperties.parse(@sample_output)
      assert props.raw["license"] == "enterprise"
      assert props.raw["v8-version"] == "12.1.165"
    end
  end

  describe "exclusions/1" do
    test "clean release build excludes only defaults" do
      props = BuildProperties.parse(@sample_output)
      exclusions = BuildProperties.exclusions(props)

      assert :full_only in exclusions
      assert :replication2 in exclusions
      refute :skip_sanitizer in exclusions
      refute :failure_points in exclusions
    end

    test "asan build excludes sanitizer and instrumented tests" do
      output = String.replace(@sample_output, "asan: false", "asan: true")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :skip_sanitizer in exclusions
      assert :skip_instrumented in exclusions
    end

    test "tsan build excludes sanitizer and instrumented tests" do
      output = String.replace(@sample_output, "tsan: false", "tsan: true")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :skip_sanitizer in exclusions
      assert :skip_instrumented in exclusions
    end

    test "coverage build excludes coverage and instrumented tests" do
      output = String.replace(@sample_output, "coverage: false", "coverage: true")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :skip_coverage in exclusions
      assert :skip_instrumented in exclusions
      refute :skip_sanitizer in exclusions
    end

    test "build without failure points excludes failure_points tests" do
      output = String.replace(@sample_output, "failure-tests: true", "failure-tests: false")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :failure_points in exclusions
    end

    test "build without V8 excludes server_javascript tests" do
      output = String.replace(@sample_output, "v8-version: 12.1.165", "")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :server_javascript in exclusions
    end

    test "ARM build excludes skip_arm tests" do
      output = String.replace(@sample_output, "arm: false", "arm: true")
      props = BuildProperties.parse(output)
      exclusions = BuildProperties.exclusions(props)

      assert :skip_arm in exclusions
    end
  end

  describe "segment_tags/0" do
    test "returns the filename-to-tag mapping" do
      tags = BuildProperties.segment_tags()
      tag_map = Map.new(tags)

      assert tag_map["cluster"] == :cluster_only
      assert tag_map["noncluster"] == :single_only
      assert tag_map["nightly"] == :full_only
      assert tag_map["fp"] == :failure_points
      assert tag_map["r2"] == :replication2
      assert tag_map["noasan"] == :skip_sanitizer
      assert tag_map["noinstr"] == :skip_instrumented
      assert tag_map["nocov"] == :skip_coverage
      assert tag_map["sjs"] == :server_javascript
    end
  end
end

# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "api-http"

################################################################################
## checking timeouts
################################################################################

  context "dealing with timeouts:" do
    before do
      # load the most current routing information
      @old_timeout = ArangoDB.set_timeout(2)
    end

    after do
      ArangoDB.set_timeout(@old_timeout)
    end

    it "calls an action and times out 1" do
      cmd = "/_admin/execute"
      body = "require('internal').wait(4);"
      begin
        ArangoDB.log_post("#{prefix}-http-timeout", cmd, :body => body)
      rescue Timeout::Error
        # if we get any different error, the rescue block won't catch it and
        # the test will fail
      end
    end
  end

  context "dealing with read timeouts:" do
    before do
      # load the most current routing information
      @old_timeout = ArangoDB.set_read_timeout(2)
    end

    after do
      ArangoDB.set_read_timeout(@old_timeout)
    end

    it "calls an action and times out 2" do
      cmd = "/_admin/execute"
      body = "require('internal').wait(4);"
      begin
        ArangoDB.log_post("#{prefix}-http-timeout", cmd, :body => body)
      rescue Timeout::Error
        # if we get any different error, the rescue block won't catch it and
        # the test will fail
      end
    end
  end
end

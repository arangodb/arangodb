# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do

  it "checks reloading users" do
    cmd = "/_admin/auth/reload";
    doc = ArangoDB.log_post("api-auth-reload", cmd, :body => {})

    doc.code.should eq(200)
    doc.parsed_response["error"].should eq(false)
    doc.parsed_response["code"].should eq(200)
  end

end

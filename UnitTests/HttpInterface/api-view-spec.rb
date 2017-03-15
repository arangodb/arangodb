# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/view"
  prefix = "api-view"

  context "dealing with views:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      it "creating a view without body" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-create-missing-body", cmd)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "creating a view without name" do
        cmd = api
        body = "{ \"type\" : \"def\" }"
        doc = ArangoDB.log_post("#{prefix}-create-missing-name", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "creating a view without type" do
        cmd = api
        body = "{ \"name\" : \"abc\" }"
        doc = ArangoDB.log_post("#{prefix}-create-missing-type", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

    end

  end
end

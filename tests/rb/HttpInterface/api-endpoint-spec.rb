# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/endpoint"
  prefix = "api-endpoint"

  context "dealing with endpoints:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do
      name = "UnitTestsDatabase"

      before do
        ArangoDB.delete("/_api/database/#{name}")

        body = "{\"name\" : \"#{name}\" }"
        doc = ArangoDB.log_post("#{prefix}-create", "/_api/database", :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        response = doc.parsed_response
        response["result"].should eq(true)
        response["error"].should eq(false)
      end

      after do
        ArangoDB.delete("/_api/database/#{name}")
      end

      it "use non-system database" do
        doc = ArangoDB.log_get("#{prefix}-get-non-system", "/_db/#{name}#{api}")

        doc.code.should eq(403)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        response = doc.parsed_response
        response["error"].should eq(true)
        response["errorNum"].should eq(1230)
      end

      it "use non-existent database" do
        doc = ArangoDB.log_get("#{prefix}-get-non-existent", "/_db/foobar#{api}")

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        response = doc.parsed_response
        response["error"].should eq(true)
        response["errorNum"].should eq(1228)
      end

    end

################################################################################
## endpoints
################################################################################

    context "retrieving endpoints:" do
      it "retrieves endpoints" do
        doc = ArangoDB.log_get("#{prefix}-get-endpoints", api)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        response = doc.parsed_response
        response[0]["endpoint"].should be_kind_of(String)
      end

    end

  end
end

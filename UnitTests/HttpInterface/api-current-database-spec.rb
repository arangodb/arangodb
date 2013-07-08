# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/current-database"
  prefix = "api-current-database"

################################################################################
## retrieving the current database's properties
################################################################################

  context "retrieving database properties:" do
    it "retrieves the properties via HTTP GET" do
      cmd = api
      doc = ArangoDB.log_get("#{prefix}-get", cmd)
        
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['result'].should have_key('name')
      doc.parsed_response['result'].should have_key('path')
      doc.parsed_response['result'].should have_key('isSystem')
      doc.parsed_response['result']['name'].should be_kind_of(String)
      doc.parsed_response['result']['path'].should be_kind_of(String)
    end
  
  end
end

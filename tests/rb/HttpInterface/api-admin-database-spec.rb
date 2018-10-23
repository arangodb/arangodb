# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_admin/database/target-version"

  context "testing admin/database api:" do
  
    it "testing target-version result" do 
      doc = ArangoDB.log_get("admin-database", api) 

      doc.parsed_response['version'].should match(/^\d{5}$/)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['version'].to_i.should be >= 30400
      doc.code.should eq(200)
    end

  end
end

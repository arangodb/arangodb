# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/aql-builtin"
  prefix = "api-aql-builtin"

  context "dealing with the builtin AQL functions:" do

    it "fetches the list of functions" do
      doc = ArangoDB.log_get("#{prefix}-fetch", api)
        
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      functions = doc.parsed_response['functions']
      found = { }
      functions.each{|f|
        f.should be_kind_of(Hash)
        f.should have_key("name")
        f.should have_key("arguments")

        found[f["name"]] = f["name"]
      }

      # check for a few known functions
      found.should have_key "PI"
      found.should have_key "DEGREES"
      found.should have_key "RADIANS"
      found.should have_key "SIN"
      found.should have_key "COS"
    end

  end
end

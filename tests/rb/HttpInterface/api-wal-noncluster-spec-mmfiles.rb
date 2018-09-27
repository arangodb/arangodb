# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do

  context "dealing with the wal API:" do
    
    before do
      # store old config
      cmd = "/_admin/wal/properties"
      doc = ArangoDB.get(cmd)
      @props = doc.parsed_response
    end
    
    after do
      # restore old config
      cmd = "/_admin/wal/properties"
      doc = ArangoDB.put(cmd, :body => JSON.dump(@props))
    end
    

################################################################################
## get the WAL properties
################################################################################

    it "retrieves the WAL properties" do
      cmd = "/_admin/wal/properties"
      doc = ArangoDB.log_get("wal-properties", cmd)

      doc.code.should eq(200)
      doc.parsed_response.should have_key("allowOversizeEntries")
      doc.parsed_response.should have_key("logfileSize")
      doc.parsed_response.should have_key("historicLogfiles")
      doc.parsed_response.should have_key("reserveLogfiles")
      doc.parsed_response.should have_key("syncInterval")
      doc.parsed_response.should have_key("throttleWait")
      doc.parsed_response.should have_key("throttleWhenPending")
    end

################################################################################
## set the WAL properties
################################################################################

    it "manipulates the WAL properties" do
      cmd = "/_admin/wal/properties"

      properties = {
        "allowOversizeEntries" => false,
        "logfileSize" => 1024 * 1024 * 8,
        "historicLogfiles" => 4,
        "reserveLogfiles" => 5,
        "throttleWait" => 1000 * 10,
        "throttleWhenPending" => 1024 * 1024
      }

      doc = ArangoDB.log_put("wal-properties", cmd, :body => JSON.dump(properties))

      doc.code.should eq(200)
      doc.parsed_response.should have_key("allowOversizeEntries")
      doc.parsed_response["allowOversizeEntries"].should eq(false)
      doc.parsed_response.should have_key("logfileSize")
      doc.parsed_response["logfileSize"].should eq(1024 * 1024 * 8)
      doc.parsed_response.should have_key("historicLogfiles")
      doc.parsed_response["historicLogfiles"].should eq(4)
      doc.parsed_response.should have_key("reserveLogfiles")
      doc.parsed_response["reserveLogfiles"].should eq(5)
      doc.parsed_response.should have_key("syncInterval")
      doc.parsed_response.should have_key("throttleWait")
      doc.parsed_response["throttleWait"].should eq(1000 * 10)
      doc.parsed_response.should have_key("throttleWhenPending")
      doc.parsed_response["throttleWhenPending"].should eq(1024 * 1024)
    end

  end

end

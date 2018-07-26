# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  prefix = "api-miscellaneous"

  context "admin status" do
    it "checks attributes" do
      cmd = "/_admin/status"
      doc = ArangoDB.log_get("#{prefix}-status-attributes", cmd)
      doc["server"].should eq("arango")
      doc.should have_key("mode") # to be removed
      doc.should have_key("operationMode")
      doc["mode"].should eq(doc["operationMode"])
      doc.should have_key("version")
      doc.should have_key("pid")
      doc.should have_key("license")
      doc.should have_key("host")
      # "hostname" is optional in the response
      # doc.should have_key("hostname")


      info = doc["serverInfo"]
      info.should have_key("maintenance")
      info.should have_key("role")
      info.should have_key("writeOpsEnabled") # to be removed
      info.should have_key("readOnly")
      info["writeOpsEnabled"].should eq(!info["readOnly"])
    end
  end
end

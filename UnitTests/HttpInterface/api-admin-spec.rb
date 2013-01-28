# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do

  context "dealing with the admin interface:" do

    before do
      # load the most current routing information
      cmd = "/_admin/routing/reload"
      doc = ArangoDB.log_get("admin-interface-get", cmd)
    end

################################################################################
## check whether admin interface is accessible
################################################################################
  
    it "checks whether the admin interface is available at /_admin/html/index.html" do
      cmd = "/_admin/html/index.html"
      doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain)

      # check response code
      doc.code.should eq(200)
      # check whether HTML result contains expected title
      doc.response.body.should include("ArangoDB - WebAdmin")
    end

    it "checks whether the admin interface is available at /_admin/html" do
      cmd = "/_admin/html"
      begin
        doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
      rescue HTTParty::RedirectionTooDeep => e
        # check response code
        e.response.code.should eq("301")
        e.response.header['location'].should eq("/_admin/html/index.html")
      end
    end

    it "checks whether the admin interface is available at /_admin/html/" do
      cmd = "/_admin/html/"
      begin
        doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
      rescue HTTParty::RedirectionTooDeep => e
        # check response code
        e.response.code.should eq("301")
        e.response.header['location'].should eq("/_admin/html/index.html")
      end
    end
    
    it "checks whether the admin interface is available at /" do
      cmd = "/"
      begin
        doc = ArangoDB.log_get("admin-interface-get", cmd, :format => :plain, :no_follow => true)
      rescue HTTParty::RedirectionTooDeep => e
        # check response code
        e.response.code.should eq("301")
        e.response.header['location'].should eq("/_admin/html/index.html")
      end
    end
    
  end

end

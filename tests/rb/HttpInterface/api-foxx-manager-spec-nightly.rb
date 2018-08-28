# coding: utf-8

require 'rspec'
require 'arangodb.rb'
require 'json'

describe ArangoDB do


  before do
    @api = "/_admin/foxx"
    @prefix = "api-foxx"
    @mount = "/itz"
    @random =  "#{@mount}/random"
  end

  def validate_app (route) 
    doc = ArangoDB.log_get("#{@prefix}-valid", route)
    doc.code.should eq(200)

    body = JSON.dump({mount: @mount})
    doc = ArangoDB.log_post("#{@prefix}-valid-uninstall", "#{@api}/uninstall", :body => body)
    doc.code.should eq(200)

    doc = ArangoDB.log_get("#{@prefix}-valid-uninstall-check", route)
    doc.code.should eq(404)
  end

  describe "Foxx Manager:" do


################################################################################
## Valid Apps
################################################################################

    describe "valid App" do
      before(:each) do
        ## Uninstall eventually mounted apps
        body = JSON.dump({mount: @mount, options: {force: true}})
        ArangoDB.log_post("#{@prefix}-install-store", "#{@api}/uninstall", :body => body)
      end

      it "should install from store" do
        body = JSON.dump({mount: @mount, appInfo: "itzpapalotl", options: {}})
        doc = ArangoDB.log_post("#{@prefix}-install-store", "#{@api}/install", :body => body)
        doc.code.should eq(200)
        validate_app(@random)
      end

      it "should install from github" do
        body = JSON.dump({mount: @mount, appInfo: "git:arangodb/itzpapalotl:v1.2.0", options: {}})
        doc = ArangoDB.log_post("#{@prefix}-install-git", "#{@api}/install", :body => body)
        doc.code.should eq(200)
        validate_app(@random)
      end

      it "should install from local" do
        body = JSON.dump({mount: @mount, appInfo: "./tests/js/common/test-data/apps/itzpapalotl", options: {}})
        doc = ArangoDB.log_post("#{@prefix}-install-store", "#{@api}/install", :body => body)
        doc.code.should eq(200)
        validate_app(@random)
      end

    end

################################################################################
## Broken Apps
################################################################################
    
  end
end   

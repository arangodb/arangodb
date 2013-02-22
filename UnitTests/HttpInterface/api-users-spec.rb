# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/user"
  prefix = "api-users"

  context "user management:" do

    before do
      (0...10).each{|i|
        ArangoDB.delete("/_api/user/users-" + i.to_s);
      }
    end

    after do
      (0...10).each{|i|
        ArangoDB.delete("/_api/user/users-" + i.to_s);
      }
    end

################################################################################
## reloading auth info
################################################################################

    it "reloads the user info" do
      doc = ArangoDB.log_get("#{prefix}-reload", "/_admin/auth/reload")

      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
    end


################################################################################
## adding users 
################################################################################

    context "adding users" do

      it "add user, no username" do
        body = "{ \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1700)
      end
    
      it "add user, empty username" do
        body = "{ \"username\" : \"\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1700)
      end
    
      it "add user, no passwd" do
        body = "{ \"username\" : \"users-1\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)

        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(true)
      end
    
      it "add user, username and passwd" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(true)
      end
    
      it "add user, username passwd, active, extra" do
        body = "{ \"username\" : \"users-2\", \"passwd\" : \"fox\", \"active\" : false, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.get(api + "/users-2")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-2")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => true })
      end
    
      it "add user, duplicate username" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
      
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)
  
        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1702)
      end
    end

################################################################################
## replacing users 
################################################################################

    context "replacing users" do
      it "replace, no user" do
        doc = ArangoDB.log_put("#{prefix}-replace", api) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "replace non-existing user" do
        doc = ArangoDB.log_put("#{prefix}-replace", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "replace already removed user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace", api, :body => body)

        doc.code.should eq(201)
        
        # remove
        doc = ArangoDB.log_delete("#{prefix}-replace", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
       
        # now replace
        doc = ArangoDB.log_put("#{prefix}-replace", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "replace, empty body" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace", api, :body => body)
        
        # replace 
        body = "{ }"
        doc = ArangoDB.log_put("#{prefix}-replace", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(true)
      end
      
      it "replace existing user, no passwd" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace", api, :body => body)
        
        # replace 
        body = "{ \"active\" : false, \"extra\" : { \"foo\" : false } }"
        doc = ArangoDB.log_put("#{prefix}-replace", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => false })
      end
      
      it "replace existing user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace", api, :body => body)
        
        # replace 
        body = "{ \"passwd\" : \"fox2\", \"active\" : false, \"extra\" : { \"foo\" : false } }"
        doc = ArangoDB.log_put("#{prefix}-replace", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => false })
      end

    end

################################################################################
## updating users 
################################################################################

    context "updating users" do
      it "update, no user" do
        doc = ArangoDB.log_patch("#{prefix}-update", api) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "update non-existing user" do
        doc = ArangoDB.log_patch("#{prefix}-update", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "update already removed user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-update", api, :body => body)

        doc.code.should eq(201)
        
        # remove
        doc = ArangoDB.log_delete("#{prefix}-update", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
       
        # now update
        doc = ArangoDB.log_patch("#{prefix}-update", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "update, empty body" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-update", api, :body => body)
        
        # update
        body = "{ }"
        doc = ArangoDB.log_patch("#{prefix}-update", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(true)
        doc.parsed_response['extra'].should eq({ "foo" => true })
      end
      
      it "update existing user, no passwd" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-update", api, :body => body)
        
        # update
        body = "{ \"active\" : false, \"extra\" : { \"foo\" : false } }"
        doc = ArangoDB.log_patch("#{prefix}-update", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => false })
      end
      
      it "update existing user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-update", api, :body => body)
        
        # update 
        body = "{ \"passwd\" : \"fox2\", \"active\" : false }"
        doc = ArangoDB.log_patch("#{prefix}-update", api + "/users-1", :body => body) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        
        doc = ArangoDB.get(api + "/users-1")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-1")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => true })
      end

    end

################################################################################
## removing users 
################################################################################

    context "removing users" do
      it "remove, no user" do
        doc = ArangoDB.log_delete("#{prefix}-remove", api) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "remove non-existing user" do
        doc = ArangoDB.log_delete("#{prefix}-remove", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "remove already removed user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-delete", api, :body => body)

        doc.code.should eq(201)
        
        # remove for the first time
        doc = ArangoDB.log_delete("#{prefix}-remove", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
       
        # remove again 
        doc = ArangoDB.log_delete("#{prefix}-remove", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "remove existing user" do
        body = "{ \"username\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-delete", api, :body => body)
        
        # remove 
        doc = ArangoDB.log_delete("#{prefix}-remove", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
      end

    end

################################################################################
## fetching users 
################################################################################

    context "fetching users" do
      
      it "no user specified" do
        doc = ArangoDB.log_get("#{prefix}-fetch", api)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end
      
      it "fetch non-existing user" do
        doc = ArangoDB.log_get("#{prefix}-fetch", api + "/users-16")

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1703)
      end
      
      it "fetch user" do
        body = "{ \"username\" : \"users-2\", \"passwd\" : \"fox\", \"active\" : false, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-fetch", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        
        doc = ArangoDB.get(api + "/users-2")
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['user'].should eq("users-2")
        doc.parsed_response['active'].should eq(false)
        doc.parsed_response['extra'].should eq({ "foo" => true })
        doc.parsed_response.should_not have_key("passwd")
        doc.parsed_response.should_not have_key("password")
      end
    end

  end
end

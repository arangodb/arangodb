# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/user"
  prefix = "api-user"

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
## validating a user
################################################################################

    context "validating users" do
      it "create a user and validate, empty password" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-1", :body => body)

        doc.code.should eq(200)
        doc.parsed_response['result'].should eq(true)
      end

      it "create a user and validate" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-1", :body => body)

        doc.code.should eq(200)
        doc.parsed_response['result'].should eq(true)
      end
      
      it "create a user and validate" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"derhansgehtInDENWALD\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"derhansgehtInDENWALD\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-1", :body => body)

        doc.code.should eq(200)
        doc.parsed_response['result'].should eq(true)
      end
      
      it "create a user and validate, non-existing user" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-2", :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1703)
      end
      
      it "create a user and validate, invalid password" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"Fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-2", :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1703)
      end
      
      it "create a user and validate, invalid password" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api, :body => body)

        doc.code.should eq(201)
        
        body = "{ \"passwd\" : \"foxxy\" }"
        doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-2", :body => body)

        doc.code.should eq(404)
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorNum'].should eq(1703)
      end
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
        body = "{ \"user\" : \"\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1700)
      end
    
      it "add user, no passwd" do
        body = "{ \"user\" : \"users-1\" }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
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
        body = "{ \"user\" : \"users-2\", \"passwd\" : \"fox\", \"active\" : false, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\" }"
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
      
        doc = ArangoDB.log_post("#{prefix}-add", api, :body => body)
  
        doc.code.should eq(409)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(409)
        doc.parsed_response['errorNum'].should eq(1702)
      end
    end

################################################################################
## replacing users 
################################################################################

    context "replacing users" do
      it "replace, no user" do
        doc = ArangoDB.log_put("#{prefix}-replace-non", api) 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "replace non-existing user" do
        doc = ArangoDB.log_put("#{prefix}-replace-non", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end

      it "replace already removed user" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace-removed", api, :body => body)

        doc.code.should eq(201)
        
        # remove
        doc = ArangoDB.log_delete("#{prefix}-replace-removed", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(202)
       
        # now replace
        doc = ArangoDB.log_put("#{prefix}-replace-removed", api + "/users-1") 
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
      end
      
      it "replace, empty body" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace-empty", api, :body => body)
        
        # replace 
        body = "{ }"
        doc = ArangoDB.log_put("#{prefix}-replace-empty", api + "/users-1", :body => body) 
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace-nopass", api, :body => body)
        
        # replace 
        body = "{ \"active\" : false, \"extra\" : { \"foo\" : false } }"
        doc = ArangoDB.log_put("#{prefix}-replace-nopass", api + "/users-1", :body => body) 
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
        doc = ArangoDB.log_post("#{prefix}-replace-exists", api, :body => body)
        
        # replace 
        body = "{ \"passwd\" : \"fox2\", \"active\" : false, \"extra\" : { \"foo\" : false } }"
        doc = ArangoDB.log_put("#{prefix}-replace-exists", api + "/users-1", :body => body) 
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : true, \"extra\" : { \"foo\" : true } }"
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
      it "fetches all users" do
        doc = ArangoDB.log_get("#{prefix}-fetch", api)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        users = doc.parsed_response['result']
        users.should be_kind_of(Array)

        users.each do |user| 
          user.should have_key("user"); 
          user["user"].should be_kind_of(String) 

          user.should have_key("active")

          user.should have_key("extra")

          user["extra"].should be_kind_of(Hash) 
        end
      end
      
      it "fetches users, requires some created users" do
        body = "{ \"user\" : \"users-1\", \"passwd\" : \"fox\", \"active\" : false, \"extra\" : { \"meow\" : false } }"
        ArangoDB.log_post("#{prefix}-fetch-existing", api, :body => body)
      
        doc = ArangoDB.log_get("#{prefix}-fetch-existing", api)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        users = doc.parsed_response['result']
        users.should be_kind_of(Array)

        found = false
        users.each do |user| 
          if user["user"] == "users-1"
            user["user"].should eq("users-1") 
            user["active"].should eq(false)
            user["extra"].should eq({ "meow" => false })
            found = true
          end
        end

        # assert we have this one user
        found.should eq(true)
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
        body = "{ \"user\" : \"users-2\", \"passwd\" : \"fox\", \"active\" : false, \"extra\" : { \"foo\" : true } }"
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

################################################################################
## operations on non-existing users (that had existed before...)
################################################################################

    it "operate on non-existing users that existed before" do
      doc = ArangoDB.log_delete("#{prefix}-nonexist", api + "/users-1") 
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(404)
      doc.parsed_response['errorNum'].should eq(1703)
        
      doc = ArangoDB.log_put("#{prefix}-non-exist", api + "/users-1") 
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(404)
      doc.parsed_response['errorNum'].should eq(1703)
      
      doc = ArangoDB.log_patch("#{prefix}-non-exist", api + "/users-1") 
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(404)
      doc.parsed_response['errorNum'].should eq(1703)

      doc = ArangoDB.log_get("#{prefix}-nonexist", api + "/users-1")
      doc.code.should eq(404)
      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(404)
      doc.parsed_response['errorNum'].should eq(1703)
    end

  end

################################################################################
## Modifying database permissions 
################################################################################

  context "Modifying permissions" do
    it "granting database" do
      body = "{ \"user\" : \"users-1\", \"passwd\" : \"\" }"
      doc = ArangoDB.log_post("#{prefix}-create user", api, :body => body)
      doc.code.should eq(201)

      body = "{ \"passwd\" : \"\" }"
      doc = ArangoDB.log_post("#{prefix}-validate", api + "/users-1", :body => body)
      doc.code.should eq(200)
      doc.parsed_response['result'].should eq(true)

      body = "{ \"grant\" : \"rw\"}"
      doc = ArangoDB.log_put("#{prefix}-grant", api + "/users-1/database/test", :body => body)
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)

      doc = ArangoDB.log_get("#{prefix}-grant-validate", api + "/users-1/database/test")
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['result'].should eq('rw')
    end

    it "granting collection" do
      body = "{ \"grant\" : \"ro\"}"
      doc = ArangoDB.log_put("#{prefix}-grant", api + "/users-1/database/test/test", :body => body)
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)

      doc = ArangoDB.log_get("#{prefix}-grant-validate", api + "/users-1/database/test/test")
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['result'].should eq('ro')
    end

    it "revoking granted collection" do
      doc = ArangoDB.log_delete("#{prefix}-revoke", api + "/users-1/database/test/test")
      doc.code.should eq(202)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(202)

      doc = ArangoDB.log_get("#{prefix}-validate", api + "/users-1/database/test/test")
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['result'].should eq('none')
    end

    it "revoking granted database" do
      doc = ArangoDB.log_delete("#{prefix}-revoke", api + "/users-1/database/test")
      doc.code.should eq(202)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(202)

      doc = ArangoDB.log_get("#{prefix}-validate", api + "/users-1/database/test")
      doc.code.should eq(200)
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['code'].should eq(200)
      doc.parsed_response['result'].should eq('none')
    end
  end  

end

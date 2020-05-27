# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/view"
  prefix = "api-view"

  context "dealing with views:" do

################################################################################
## error handling
################################################################################

    context "error handling:" do

      context "creation:" do
        after do
          ArangoDB.log_delete("#{prefix}-teardown", api + '/test')
          ArangoDB.log_delete("#{prefix}-teardown", api + '/dup')
        end

        it "creating a view without body" do
          cmd = api
          doc = ArangoDB.log_post("#{prefix}-create-missing-body", cmd)

          doc.code.should eq(400)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(400)
          doc.parsed_response['errorNum'].should eq(10)
        end

        it "creating a view without name" do
          cmd = api
          body = <<-JSON
                 { "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc = ArangoDB.log_post("#{prefix}-create-missing-name", cmd, :body => body)

          doc.code.should eq(400)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(400)
          doc.parsed_response['errorNum'].should eq(10)
        end

        it "creating a view without type" do
          cmd = api
          body = <<-JSON
                 { "name": "abc",
                   "properties": {} }
                 JSON
          doc = ArangoDB.log_post("#{prefix}-create-missing-type", cmd, :body => body)

          doc.code.should eq(400)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(400)
          doc.parsed_response['errorNum'].should eq(10)
        end

        it "creating a view with invalid type" do
          cmd = api
          body = <<-JSON
                 { "name": "test",
                   "type": "foobar",
                   "properties": {} }
                 JSON
          doc = ArangoDB.log_post("#{prefix}-create-invalid-type", cmd, :body => body)

          doc.code.should eq(400)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(400)
          doc.parsed_response['errorNum'].should eq(10)
        end

        it "creating a view without properties" do
          cmd = api
          body = <<-JSON
                 { "name": "test",
                   "type": "arangosearch" }
                 JSON
          doc = ArangoDB.log_post("#{prefix}-create-without properties", cmd, :body => body)

          doc.code.should eq(201)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['name'].should eq("test")
          doc.parsed_response['type'].should eq("arangosearch")

          cmd2 = api + '/test'
          doc2 = ArangoDB.log_delete("#{prefix}-create-without properties", cmd2)

          doc2.code.should eq(200)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        end

        it "duplicate name" do
          cmd1 = api
          body1 = <<-JSON
                 { "name": "dup",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc1 = ArangoDB.log_post("#{prefix}-create-duplicate", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("dup")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api
          body2 = <<-JSON
                 { "name": "dup",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc2 = ArangoDB.log_post("#{prefix}-create-duplicate", cmd2, :body => body2)

          doc2.code.should eq(409)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['error'].should eq(true)
          doc2.parsed_response['code'].should eq(409)
          doc2.parsed_response['errorNum'].should eq(1207)

          cmd3 = api + '/dup'
          doc3 = ArangoDB.log_delete("#{prefix}-create-duplicate", cmd3)

          doc3.code.should eq(200)
          doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        end

      end

      context "rename:" do
        after do
          ArangoDB.log_delete("#{prefix}-teardown", api + '/testView')
          ArangoDB.log_delete("#{prefix}-teardown", api + '/newName')
          ArangoDB.log_delete("#{prefix}-teardown", api + '/dup')
          ArangoDB.log_delete("#{prefix}-teardown", api + '/dup2')
        end

        it "valid change" do
          cmd1 = api
          body1 = <<-JSON
                 { "name": "testView",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc1 = ArangoDB.log_post("#{prefix}-valid-change", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("testView")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api + '/testView/rename'
          body2 = <<-JSON
                 { "name": "newName" }
                 JSON
          doc2 = ArangoDB.log_put("#{prefix}-valid-change", cmd2, :body =>
           body2)

          doc2.code.should eq(200)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['name'].should eq('newName')

          cmd3 = api + '/newName'
          doc3 = ArangoDB.log_delete("#{prefix}-valid-change", cmd3)

          doc3.code.should eq(200)
          doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        end

        it "same name" do
          cmd1 = api
          body1 = <<-JSON
                 { "name": "testView",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc1 = ArangoDB.log_post("#{prefix}-same-name", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("testView")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api + '/testView/rename'
          body2 = <<-JSON
                 { "name": "testView" }
                 JSON
          doc2 = ArangoDB.log_put("#{prefix}-same-name", cmd2, :body =>
           body2)

          doc2.code.should eq(200)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['name'].should eq('testView')

          cmd3 = api + '/testView'
          doc3 = ArangoDB.log_delete("#{prefix}-same-name", cmd3)

          doc3.code.should eq(200)
          doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        end

        it "invalid name" do
          cmd1 = api
          body1 = <<-JSON
                 { "name": "testView",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc1 = ArangoDB.log_post("#{prefix}-rename-invalid", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("testView")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api + '/testView/rename'
          body2 = <<-JSON
                 { "name": "g@rb@ge" }
                 JSON
          doc2 = ArangoDB.log_put("#{prefix}-rename-invalid", cmd2, :body =>
           body2)

          doc2.code.should eq(400)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['error'].should eq(true)
          doc2.parsed_response['code'].should eq(400)
          doc2.parsed_response['errorNum'].should eq(1208)

          cmd3 = api + '/testView'
          doc3 = ArangoDB.log_delete("#{prefix}-rename-invalid", cmd3)

          doc3.code.should eq(200)
          doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        end

        it "duplicate name" do
          cmd1 = api
          body1 = <<-JSON
                 { "name": "dup",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc1 = ArangoDB.log_post("#{prefix}-rename-duplicate", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("dup")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api
          body2 = <<-JSON
                 { "name": "dup2",
                   "type": "arangosearch",
                   "properties": {} }
                 JSON
          doc2 = ArangoDB.log_post("#{prefix}-rename-duplicate", cmd2, :body => body2)

          doc2.code.should eq(201)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['name'].should eq("dup2")
          doc2.parsed_response['type'].should eq("arangosearch")

          cmd3 = api + '/dup2/rename'
          body3 = <<-JSON
                 { "name": "dup" }
                 JSON
          doc3 = ArangoDB.log_put("#{prefix}-rename-duplicate", cmd3, :body =>
           body3)

          doc3.code.should eq(409)
          doc3.headers['content-type'].should eq("application/json; charset=utf-8")
          doc3.parsed_response['error'].should eq(true)
          doc3.parsed_response['code'].should eq(409)
          doc3.parsed_response['errorNum'].should eq(1207)

          cmd4 = api + '/dup'
          doc4 = ArangoDB.log_delete("#{prefix}-rename-duplicate", cmd4)

          doc4.code.should eq(200)
          doc4.headers['content-type'].should eq("application/json; charset=utf-8")

          cmd5 = api + '/dup2'
          doc5 = ArangoDB.log_delete("#{prefix}-rename-duplicate", cmd5)

          doc5.code.should eq(200)
          doc5.headers['content-type'].should eq("application/json; charset=utf-8")
        end

      end

      context "deletion:" do
        it "deleting a non-existent view" do
          cmd = api + "/foobar"
          doc = ArangoDB.log_delete("#{prefix}-delete-non-existent", cmd)

          doc.code.should eq(404)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(404)
          doc.parsed_response['errorNum'].should eq(1203)
        end

      end

      context "retrieval:" do
        it "getting a non-existent view" do
          cmd = api + "/foobar"
          doc = ArangoDB.log_get("#{prefix}-get-non-existent", cmd)

          doc.code.should eq(404)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(404)
          doc.parsed_response['errorNum'].should eq(1203)
        end

        it "getting properties of a non-existent view" do
          cmd = api + "/foobar/properties"
          doc = ArangoDB.log_get("#{prefix}-get-non-existent-properties", cmd)

          doc.code.should eq(404)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(404)
          doc.parsed_response['errorNum'].should eq(1203)
        end

      end

      context "modification:" do
        after do
          ArangoDB.log_delete("#{prefix}-teardown", api + '/lemon')
        end

        it "modifying view directly, not properties" do
          cmd = api + "/foobar"
          body = <<-JSON
                 { "level": "DEBUG" }
                 JSON
          doc = ArangoDB.log_put("#{prefix}-modify-direct", cmd, :body => body)

          doc.code.should eq(400)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(400)
          doc.parsed_response['errorNum'].should eq(10)
        end

        it "modifying a non-existent view" do
          cmd = api + "/foobar/properties"
          body = <<-JSON
                 { "level": "DEBUG" }
                 JSON
          doc = ArangoDB.log_put("#{prefix}-modify-non-existent", cmd, :body => body)

          doc.code.should eq(404)
          doc.headers['content-type'].should eq("application/json; charset=utf-8")
          doc.parsed_response['error'].should eq(true)
          doc.parsed_response['code'].should eq(404)
          doc.parsed_response['errorNum'].should eq(1203)
        end

        it "modifying a view with unacceptable properties" do
          cmd1 = api
          body1 = <<-JSON
                  { "name": "lemon",
                    "type": "arangosearch",
                    "properties": {} }
                  JSON
          doc1 = ArangoDB.log_post("#{prefix}-modify-unacceptable", cmd1, :body => body1)

          doc1.code.should eq(201)
          doc1.headers['content-type'].should eq("application/json; charset=utf-8")
          doc1.parsed_response['name'].should eq("lemon")
          doc1.parsed_response['type'].should eq("arangosearch")

          cmd2 = api + '/lemon/properties'
          body2 = <<-JSON
                 { "bogus": "junk", "consolidationIntervalMsec": 17 }
                 JSON
          doc2 = ArangoDB.log_put("#{prefix}-modify-unacceptable", cmd2, :body => body2)
          doc2.code.should eq(200)
          doc2.headers['content-type'].should eq("application/json; charset=utf-8")
          doc2.parsed_response['name'].should eq("lemon")
          doc2.parsed_response['type'].should eq("arangosearch")
          doc2.parsed_response['id'].should eq(doc1.parsed_response['id'])

          cmd3 = api + '/lemon/properties'
          doc4 = ArangoDB.log_get("#{prefix}-modify-unacceptable", cmd3)
          doc4.parsed_response['consolidationIntervalMsec'].should eq(17)

          cmd4 = api + '/lemon'
          doc4 = ArangoDB.log_delete("#{prefix}-modify-unacceptable", cmd4)

          doc4.code.should eq(200)
          doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        end

      end

    end

    context "add/drop:" do
      after do
        ArangoDB.log_delete("#{prefix}-teardown", api + '/abc')
      end

      it "creating a view" do
        cmd = api
        body = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "properties": {} }
               JSON
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['name'].should eq("abc")
        doc.parsed_response['type'].should eq("arangosearch")
      end

      it "dropping a view" do
        cmd = api
        body = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "properties": {} }
               JSON
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(201)

        cmd1 = api + "/abc"
        doc1 = ArangoDB.log_delete("#{prefix}-drop-a-view", cmd1)

        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")

        cmd2 = api + "/abc"
        doc2 = ArangoDB.log_get("#{prefix}-drop-a-view", cmd2)

        doc2.code.should eq(404)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(404)
      end
    end

    context "add with link to non-existing collection:" do
      after do
        ArangoDB.log_delete("#{prefix}-teardown", api + '/abc')
      end

      it "creating a view with link to non-existing collection" do
        cmd = api
        body = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "links": { "wrong" : {} } 
               }
               JSON
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(404)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(404)
        doc.parsed_response['errorMessage'].should eq("while validating arangosearch link definition, error: collection 'wrong' not found")
        doc.parsed_response['errorNum'].should eq(1203)
      end
    end

    context "add with link to existing collection:" do
      cn = "right"

      before do
        ArangoDB.drop_collection(cn)
        @cid = ArangoDB.create_collection(cn)
      end

      after do
        ArangoDB.log_delete("#{prefix}-teardown", api + '/abc')
        ArangoDB.drop_collection(cn)
      end

      it "creating a view with link to existing collection/drop" do
        cmd = api
        body = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "links": { "right" : { "includeAllFields": true } } 
               }
               JSON
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['name'].should eq("abc")
        doc.parsed_response['type'].should eq("arangosearch")
        doc.parsed_response['links']['right'].should be_kind_of(Object)
      end

      it "dropping a view" do
        cmd = api
        body = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "links": { "right" : { "includeAllFields": true } } 
               }
               JSON
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(201)

        cmd1 = api + "/abc"
        doc1 = ArangoDB.log_delete("#{prefix}-drop-a-view", cmd1)

        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")

        cmd2 = api + "/abc"
        doc2 = ArangoDB.log_get("#{prefix}-drop-a-view", cmd2)

        doc2.code.should eq(404)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(404)
      end
    end

    context "retrieval:" do
      after do
        ArangoDB.log_delete("#{prefix}-teardown", api + '/abc')
        ArangoDB.log_delete("#{prefix}-teardown", api + '/def')
      end

      it "empty list" do
        cmd = api
        doc = ArangoDB.log_get("#{prefix}-empty-list", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['result'].should eq([])
      end

      it "short list" do
        cmd1 = api
        body1 = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq("abc")
        doc1.parsed_response['type'].should eq("arangosearch")

        cmd2 = api
        body2 = <<-JSON
               { "name": "def",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc2 = ArangoDB.log_post("#{prefix}-short-list", cmd2, :body => body2)
        doc2.code.should eq(201)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['name'].should eq("def")
        doc2.parsed_response['type'].should eq("arangosearch")

        cmd3 = api
        doc3 = ArangoDB.log_get("#{prefix}-short-list", cmd3)
        doc3.code.should eq(200)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        doc3.parsed_response.length.should eq(3)
        doc3.parsed_response['result'][0]['name'].should eq("abc")
        doc3.parsed_response['result'][1]['name'].should eq("def")
        doc3.parsed_response['result'][0]['type'].should eq("arangosearch")
        doc3.parsed_response['result'][1]['type'].should eq("arangosearch")
      end

      it "individual views" do
        cmd1 = api
        body1 = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq("abc")
        doc1.parsed_response['type'].should eq("arangosearch")

        cmd2 = api
        body2 = <<-JSON
               { "name": "def",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc2 = ArangoDB.log_post("#{prefix}-short-list", cmd2, :body => body2)
        doc2.code.should eq(201)

        cmd1 = api + '/abc'
        doc1 = ArangoDB.log_get("#{prefix}-individual-views", cmd1)
        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq("abc")
        doc1.parsed_response['type'].should eq("arangosearch")

        cmd2 = api + '/abc/properties'
        doc2 = ArangoDB.log_get("#{prefix}-individual-views", cmd2)
        doc2.code.should eq(200)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['consolidationIntervalMsec'].should eq(10)

        cmd3 = api + '/def'
        doc3 = ArangoDB.log_get("#{prefix}-individual-views", cmd3)
        doc3.code.should eq(200)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        doc3.parsed_response['name'].should eq("def")
        doc3.parsed_response['type'].should eq("arangosearch")

        cmd4 = api + '/def/properties'
        doc4 = ArangoDB.log_get("#{prefix}-individual-views", cmd4)
        doc4.code.should eq(200)
        doc4.headers['content-type'].should eq("application/json; charset=utf-8")
        doc4.parsed_response['consolidationIntervalMsec'].should eq(10)
      end

    end

    context "modification:" do
      after do
        ArangoDB.log_delete("#{prefix}-teardown", api + '/abc')
      end

      it "change properties" do
        cmd1 = api
        body1 = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)

        cmd1 = api + '/abc'
        doc1 = ArangoDB.log_get("#{prefix}-individual-views", cmd1)
        doc1.code.should eq(200)
        
        cmd1 = api + '/abc/properties'
        body1 = <<-JSON
                { "consolidationIntervalMsec": 7 }
                JSON
        doc1 = ArangoDB.log_put("#{prefix}-change-properties", cmd1, :body => body1)
        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq('abc')
        doc1.parsed_response['type'].should eq('arangosearch')
        doc1.parsed_response.should be_kind_of(Hash)

        cmd2 = api + '/abc/properties'
        doc2 = ArangoDB.log_get("#{prefix}-change-properties", cmd2)
        doc2.code.should eq(200)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['consolidationIntervalMsec'].should eq(7)
      end

      it "ignore extra properties" do
        cmd1 = api
        body1 = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)

        cmd1 = api + '/abc'
        doc1 = ArangoDB.log_get("#{prefix}-individual-views", cmd1)
        doc1.code.should eq(200)

        cmd1 = api + '/abc/properties'
        body1 = <<-JSON
                { "consolidationIntervalMsec": 10, "extra": "foobar" }
                JSON
        doc1 = ArangoDB.log_put("#{prefix}-ignore-extra-properties", cmd1, :body => body1)
        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq('abc')
        doc1.parsed_response['type'].should eq('arangosearch')
        doc1.parsed_response.should be_kind_of(Hash)
        doc1.parsed_response['extra'].should eq(nil)

        cmd2 = api + '/abc/properties'
        doc2 = ArangoDB.log_get("#{prefix}-ignore-extra-properties", cmd2)
        doc2.code.should eq(200)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['consolidationIntervalMsec'].should eq(10)
        doc2.parsed_response['extra'].should eq(nil)
      end

      it "accept updates via PATCH as well" do
        cmd1 = api
        body1 = <<-JSON
               { "name": "abc",
                 "type": "arangosearch",
                 "consolidationIntervalMsec": 10
               }
               JSON
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)

        cmd1 = api + '/abc/properties'
        body1 = <<-JSON
                { "consolidationIntervalMsec": 3 }
                JSON
        doc1 = ArangoDB.log_patch("#{prefix}-accept-patch", cmd1, :body => body1)
        doc1.code.should eq(200)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq('abc')
        doc1.parsed_response['type'].should eq('arangosearch')
        doc1.parsed_response.should be_kind_of(Hash)

        cmd2 = api + '/abc/properties'
        doc2 = ArangoDB.log_get("#{prefix}-accept-patch", cmd2)
        doc2.code.should eq(200)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['consolidationIntervalMsec'].should eq(3)
      end

    end

  end
end

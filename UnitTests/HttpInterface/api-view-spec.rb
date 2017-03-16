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
      it "creating a view without body" do
        cmd = api
        doc = ArangoDB.log_post("#{prefix}-create-missing-body", cmd)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "creating a view without name" do
        cmd = api
        body = <<-END
               { "type": "def" }
               END
        doc = ArangoDB.log_post("#{prefix}-create-missing-name", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "creating a view without type" do
        cmd = api
        body = <<-END
               { "name": "abc" }
               END
        doc = ArangoDB.log_post("#{prefix}-create-missing-type", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(400)
      end

      it "duplicate name" do
        cmd1 = api
        body1 = <<-END
               { "name": "dup",
                 "type": "type1" }
               END
        doc1 = ArangoDB.log_post("#{prefix}-create-duplicate", cmd1, :body => body1)

        doc1.code.should eq(201)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq("dup")
        doc1.parsed_response['type'].should eq("type1")

        cmd2 = api
        body2 = <<-END
               { "name": "dup",
                 "type": "type2" }
               END
        doc2 = ArangoDB.log_post("#{prefix}-create-duplicate", cmd1, :body => body2)

        doc2.code.should eq(409)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(409)
        doc2.parsed_response['errorNum'].should eq(1207)

        cmd3 = api + '/dup'
        doc3 = ArangoDB.log_delete("#{prefix}-create-duplicate", cmd3)

        doc3.code.should eq(204)
        doc3.headers['content-type'].should eq("text/plain; charset=utf-8")
      end

    end

    context "add/drop:" do
      it "creating a view" do
        cmd = api
        body = <<-END
               { "name": "abc",
                 "type": "def" }
               END
        doc = ArangoDB.log_post("#{prefix}-create-a-view", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['name'].should eq("abc")
        doc.parsed_response['type'].should eq("def")
      end

      it "dropping a view" do
        cmd1 = api + "/abc"
        doc1 = ArangoDB.log_delete("#{prefix}-drop-a-view", cmd1)

        doc1.code.should eq(204)
        doc1.headers['content-type'].should eq("text/plain; charset=utf-8")

        cmd2 = api + "/abc"
        doc2 = ArangoDB.log_get("#{prefix}-drop-a-view", cmd2)

        doc2.code.should eq(404)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['error'].should eq(true)
        doc2.parsed_response['code'].should eq(404)
      end
    end

    context "retrieval:" do
      it "empty list" do
        cmd = api
        doc = ArangoDB.log_get("#{prefix}-empty-list", cmd)

        doc.code.should eq(200)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response.should eq([])
      end

      it "short list" do
        cmd1 = api
        body1 = <<-END
               {"name": "abc",
                "type": "type1"}
               END
        doc1 = ArangoDB.log_post("#{prefix}-short-list", cmd1, :body => body1)
        doc1.code.should eq(201)
        doc1.headers['content-type'].should eq("application/json; charset=utf-8")
        doc1.parsed_response['name'].should eq("abc")
        doc1.parsed_response['type'].should eq("type1")

        cmd2 = api
        body2 = <<-END
               {"name": "def",
                "type": "type2"}
               END
        doc2 = ArangoDB.log_post("#{prefix}-short-list", cmd2, :body => body2)
        doc2.code.should eq(201)
        doc2.headers['content-type'].should eq("application/json; charset=utf-8")
        doc2.parsed_response['name'].should eq("def")
        doc2.parsed_response['type'].should eq("type2")

        cmd3 = api
        doc3 = ArangoDB.log_get("#{prefix}-short-list", cmd3)
        doc3.code.should eq(200)
        doc3.headers['content-type'].should eq("application/json; charset=utf-8")
        doc3.parsed_response.length.should eq(2)
        doc3.parsed_response[0]['name'].should eq("abc")
        doc3.parsed_response[1]['name'].should eq("def")
        doc3.parsed_response[0]['type'].should eq("type1")
        doc3.parsed_response[1]['type'].should eq("type2")
      end
    end

  end
end

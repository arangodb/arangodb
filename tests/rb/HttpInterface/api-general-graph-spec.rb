# coding: utf-8

require 'rspec'
require 'json'
require 'arangodb.rb'

PREFIX = "api-general-graph"
URLPREFIX = "/_api/gharial"


describe ArangoDB do

  [true, false].each do |sync|

    context "with waitForSync = #{sync}" do

      def drop_graph (waitForSync, graph_name)
        cmd = URLPREFIX + "/" + graph_name
        cmd = cmd + "?waitForSync=#{waitForSync}"
        doc = ArangoDB.delete(cmd)
        return doc
      end

      def get_graph (graph_name)
        cmd = URLPREFIX + "/" + graph_name
        doc = ArangoDB.get(cmd)
        return doc
      end

      def create_graph (waitForSync, name, edge_definitions)
        cmd = URLPREFIX
        cmd = cmd + "?waitForSync=#{waitForSync}"
        body = JSON.dump({:name => name, :edgeDefinitions => edge_definitions})
        doc = ArangoDB.post(cmd, :body => body)
        return doc
      end

      def create_graph_orphans (waitForSync, name, edge_definitions, orphans)
        cmd = URLPREFIX
        cmd = cmd + "?waitForSync=#{waitForSync}"
        body = JSON.dump({:name => name, :edgeDefinitions => edge_definitions, :orphanCollections => orphans})
        doc = ArangoDB.post(cmd, :body => body)
        return doc
      end

      def endpoint (type, graph_name, collection, key)
        result = URLPREFIX + "/" + graph_name + "/" + type;
        if (collection != nil)
          result =  result + "/" + collection
        end
        if (key != nil)
          result =  result + "/" + key
        end
        return result;
      end

      def vertex_endpoint (graph_name, collection = nil, key = nil)
        return endpoint("vertex", graph_name, collection, key)
      end

      def edge_endpoint (graph_name, collection = nil, key = nil)
        return endpoint("edge", graph_name, collection, key)
      end

      def list_edge_collections (graph_name)
        cmd = edge_endpoint(graph_name)
        doc = ArangoDB.get(cmd)
        return doc
      end

      def additional_edge_definition (waitForSync, graph_name, edge_definitions)
        cmd = edge_endpoint(graph_name)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        doc = ArangoDB.post(cmd, :body => JSON.dump(edge_definitions))
        return doc
      end

      def change_edge_definition (waitForSync, graph_name, definition_name, edge_definitions)
        cmd = edge_endpoint(graph_name, definition_name)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        doc = ArangoDB.put(cmd, :body => JSON.dump(edge_definitions))
        return doc
      end

      def delete_edge_definition (waitForSync, graph_name, definition_name)
        cmd = edge_endpoint(graph_name, definition_name)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        doc = ArangoDB.delete(cmd)
        return doc
      end

      def list_vertex_collections (graph_name)
        cmd = vertex_endpoint(graph_name)
        doc = ArangoDB.get(cmd)
        return doc
      end

      def additional_vertex_collection (waitForSync, graph_name, collection_name)
        cmd = vertex_endpoint(graph_name)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        body = { :collection => collection_name }
        doc = ArangoDB.post(cmd, :body => JSON.dump(body))
        return doc
      end

      def delete_vertex_collection (waitForSync, graph_name, collection_name)
        cmd = vertex_endpoint(graph_name, collection_name)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        doc = ArangoDB.delete(cmd)
        return doc
      end

      def create_vertex (waitForSync, graph_name, collection, body, options = {})
        cmd = vertex_endpoint(graph_name, collection)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.post(cmd, :body => JSON.dump(body))
        return doc
      end

      def get_vertex (graph_name, collection, key)
        cmd = vertex_endpoint(graph_name, collection, key)
        doc = ArangoDB.get(cmd)
        return doc
      end

      def update_vertex (waitForSync, graph_name, collection, key, body, keepNull = '', options = {})
        cmd = vertex_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        if keepNull != '' then
          cmd = cmd + "&keepNull=#{keepNull}"
        end
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.patch(cmd, :body => JSON.dump(body))
        return doc
      end

      def replace_vertex (waitForSync, graph_name, collection, key, body, options = {})
        cmd = vertex_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.put(cmd, :body => JSON.dump(body))
        return doc
      end

      def delete_vertex (waitForSync, graph_name, collection, key, options = {})
        cmd = vertex_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.delete(cmd)
        return doc
      end

      def create_edge (waitForSync, graph_name, collection, from, to, body, options = {})
        cmd = edge_endpoint(graph_name, collection)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        body["_from"] = from
        body["_to"] = to
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.post(cmd, :body => JSON.dump(body))
        return doc
      end

      def get_edge (graph_name, collection, key)
        cmd = edge_endpoint(graph_name, collection, key)
        doc = ArangoDB.get(cmd)
        return doc
      end

      def update_edge (waitForSync, graph_name, collection, key, body, keepNull = '', options = {})
        cmd = edge_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        if keepNull != '' then
          cmd = cmd + "&keepNull=#{keepNull}"
        end
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.patch(cmd, :body => JSON.dump(body))
        return doc
      end

      def replace_edge (waitForSync, graph_name, collection, key, body, options = {})
        cmd = edge_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.put(cmd, :body => JSON.dump(body))
        return doc
      end

      def delete_edge (waitForSync, graph_name, collection, key, options = {})
        cmd = edge_endpoint(graph_name, collection, key)
        cmd = cmd + "?waitForSync=#{waitForSync}"
        options.each do |key,value|
          cmd = cmd + "&" + key + "=" + value.to_s
        end
        doc = ArangoDB.delete(cmd)
        return doc
      end

      user_collection = "UnitTestUsers"
      product_collection = "UnitTestProducts"
      friend_collection = "UnitTestFriends"
      bought_collection = "UnitTestBoughts"
      graph_name = "UnitTestGraph"
      unknown_name = "UnitTestUnknown"

      context "testing general graph methods:" do


    ################################################################################
    ## checking graph creation process
    ################################################################################

        context "check creation of graphs" do
          before do
            drop_graph(sync, graph_name)
          end

          after do
            drop_graph(sync, graph_name)
            ArangoDB.drop_collection(bought_collection)
            ArangoDB.drop_collection(friend_collection)
            ArangoDB.drop_collection(product_collection)
            ArangoDB.drop_collection(user_collection)
          end

          it "can create an empty graph" do
            edge_definition = []
            doc = create_graph(sync, graph_name, edge_definition )

            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
          end

          it "can create a graph with definitions" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            edge_definition = [first_def]
            doc = create_graph(sync, graph_name, edge_definition )

            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
          end

          it "can create a graph with orphan collections" do
            orphans = [product_collection];
            doc = create_graph_orphans( sync, graph_name, [], orphans)
            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq([])
            doc.parsed_response['graph']['orphanCollections'].should eq(orphans)
          end

          it "can add additional edge definitions" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            edge_definition = [first_def]
            create_graph(sync, graph_name, edge_definition )
            second_def = { "collection" => bought_collection, "from" => [user_collection], "to" => [product_collection] }
            doc = additional_edge_definition(sync, graph_name, second_def )
            edge_definition.push(second_def)
            edge_definition = edge_definition.sort_by { |d| [ -d["collection"] ] }

            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
          end

          it "can modify existing edge definitions" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [first_def] )
            second_def = { "collection" => friend_collection, "from" => [product_collection], "to" => [user_collection] }
            edge_definition = [second_def]

            doc = change_edge_definition(sync,  graph_name, friend_collection, second_def )
            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
          end

          it "can delete an edge definition" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            edge_definition = [first_def]
            create_graph(sync, graph_name, edge_definition )
            doc = delete_edge_definition(sync,  graph_name, friend_collection )

            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq([])
          end

          it "can add an additional orphan collection" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            edge_definition = [first_def]
            create_graph(sync, graph_name, edge_definition )
            doc = additional_vertex_collection( sync, graph_name, product_collection )

            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
            doc.parsed_response['graph']['orphanCollections'].should eq([product_collection])
          end

          it "can delete an orphan collection" do
            first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            edge_definition = [first_def]
            create_graph(sync, graph_name, edge_definition )
            additional_vertex_collection(sync,  graph_name, product_collection )
            doc = delete_vertex_collection( sync,  graph_name, product_collection )

            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
            doc.parsed_response['graph']['orphanCollections'].should eq([])
          end

          it "can delete a graph again" do
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
            doc = drop_graph(sync, graph_name)
            doc.code.should eq(202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(202)
          end

          it "can not delete a graph twice" do
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
            drop_graph(sync, graph_name)
            doc = drop_graph(sync, graph_name)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should eq("graph 'UnitTestGraph' not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can not create a graph twice" do
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
            doc = create_graph(sync, graph_name, [definition])
            doc.code.should eq(409)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should eq("graph already exists")
            doc.parsed_response['code'].should eq(409)
          end

          it "can get a graph by name" do
            orphans = [product_collection];
            doc = create_graph_orphans( sync, graph_name, [], orphans)
            rev = doc.parsed_response['graph']['_rev']

            doc = get_graph(graph_name)
            doc.code.should eq(200)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(200)
            doc.parsed_response['graph']['name'].should eq(graph_name)
            doc.parsed_response['graph']['_rev'].should eq(rev)
            doc.parsed_response['graph']['edgeDefinitions'].should eq([])
            doc.parsed_response['graph']['orphanCollections'].should eq(orphans)
          end

          it "can get a list of vertex collections" do
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
            additional_vertex_collection(sync, graph_name, product_collection)

            doc = list_vertex_collections(graph_name)
            doc.code.should eq(200)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(200)
            doc.parsed_response['collections'].should eq([product_collection, user_collection])
          end

          it "can get a list of edge collections" do
            definition1 = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            definition2 = { "collection" => bought_collection, "from" => [user_collection], "to" => [product_collection] }
            create_graph(sync, graph_name, [definition1, definition2])

            doc = list_edge_collections(graph_name)
            doc.code.should eq(200)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(200)
            doc.parsed_response['collections'].should eq([bought_collection, friend_collection])
          end
        end

        context "check vertex operations" do
          before do
            drop_graph(sync, graph_name)
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
          end

          after do
            drop_graph(sync, graph_name)
            ArangoDB.drop_collection(friend_collection)
            ArangoDB.drop_collection(user_collection)
          end

          it "can create a vertex" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)
          end
          
          it "can create a vertex, returnNew" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name}, { "returnNew" => "true" })
            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new']['_key'].should_not eq(nil)
            doc.parsed_response['new']['name'].should eq(name)
          end

          it "can get a vertex" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']

            doc = get_vertex(graph_name, user_collection, key)
            doc.code.should eq(200)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(200)
            doc.parsed_response['vertex']['name'].should eq(name)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
          end

          it "can not get a non existing vertex" do
            key = "unknownKey"

            doc = get_vertex(graph_name, user_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can replace a vertex" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']
            oldTag = doc.parsed_response['vertex']['_rev']
            oldTag.should eq(doc.headers['etag'])
            name = "Bob"

            doc = replace_vertex( sync, graph_name, user_collection, key, {"name2" => name})
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex']['name'].should eq(nil)
            doc.parsed_response['vertex']['name2'].should eq(name)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            oldTag.should_not eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
          end
          
          it "can replace a vertex, returnOld" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']
            oldTag = doc.parsed_response['vertex']['_rev']
            oldTag.should eq(doc.headers['etag'])
            name = "Bob"

            doc = replace_vertex( sync, graph_name, user_collection, key, {"name2" => name}, { "returnOld" => "true", "returnNew" => true })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
            
            doc.parsed_response['old']['_key'].should eq(key)
            doc.parsed_response['old']['name'].should eq("Alice")
            doc.parsed_response['old']['name2'].should eq(nil)
            doc.parsed_response['new']['_key'].should eq(key)
            doc.parsed_response['new']['name'].should eq(nil)
            doc.parsed_response['new']['name2'].should eq("Bob")

            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex']['name'].should eq(nil)
            doc.parsed_response['vertex']['name2'].should eq(name)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            oldTag.should_not eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
          end

          it "can not replace a non existing vertex" do
            key = "unknownKey"

            doc = replace_vertex( sync, graph_name, user_collection, key, {"name2" => "bob"})
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can update a vertex" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']
            name2 = "Bob"

            doc = update_vertex( sync, graph_name, user_collection, key, {"name2" => name2}, "")
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex']['name'].should eq(name)
            doc.parsed_response['vertex']['name2'].should eq(name2)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
          end
          
          it "can update a vertex, returnOld" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']
            name2 = "Bob"

            doc = update_vertex( sync, graph_name, user_collection, key, {"name2" => name2}, "", { "returnOld" => "true", "returnNew" => "true" })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
           
            doc.parsed_response['old']['_key'].should eq(key)
            doc.parsed_response['old']['name'].should eq(name)
            doc.parsed_response['old']['name2'].should eq(nil)
            doc.parsed_response['new']['_key'].should eq(key)
            doc.parsed_response['new']['name'].should eq(name)
            doc.parsed_response['new']['name2'].should eq(name2)

            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex']['name'].should eq(name)
            doc.parsed_response['vertex']['name2'].should eq(name2)
            doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['vertex']['_key'].should eq(key)
          end
          
          it "can update a vertex keepNull default(true)" do
            doc = create_vertex( sync, graph_name, user_collection, {"name" => "Alice"})
            key = doc.parsed_response['vertex']['_key']
            doc = update_vertex( sync, graph_name, user_collection, key, {"name" => nil}, "")
            doc.code.should eq(sync ? 200 : 202)
            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex'].key?('name').should eq(true)
            doc.parsed_response['vertex']['name'].should eq(nil)
          end
          
          it "can update a vertex keepNull false" do
            doc = create_vertex( sync, graph_name, user_collection, {"name" => "Alice"})
            key = doc.parsed_response['vertex']['_key']
            doc = update_vertex( sync, graph_name, user_collection, key, {"name" => nil}, false)
            doc.code.should eq(sync ? 200 : 202)
            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex'].key?('name').should eq(false)
          end
          
          it "can update a vertex keepNull true" do
            doc = create_vertex( sync, graph_name, user_collection, {"name" => "Alice"})
            key = doc.parsed_response['vertex']['_key']
            doc = update_vertex( sync, graph_name, user_collection, key, {"name" => nil}, true)
            doc.code.should eq(sync ? 200 : 202)
            doc = get_vertex(graph_name, user_collection, key)
            doc.parsed_response['vertex'].key?('name').should eq(true)
            doc.parsed_response['vertex']['name'].should eq(nil)
          end

          it "can not update a non existing vertex" do
            key = "unknownKey"
            name2 = "Bob"

            doc = update_vertex( sync, graph_name, user_collection, key, {"name2" => name2}, "")
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can delete a vertex" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']

            doc = delete_vertex( sync, graph_name, user_collection, key)
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_vertex(graph_name, user_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end
          
          it "can delete a vertex, returnOld" do
            name = "Alice"
            doc = create_vertex( sync, graph_name, user_collection, {"name" => name})
            key = doc.parsed_response['vertex']['_key']

            doc = delete_vertex( sync, graph_name, user_collection, key, { "returnOld" => "true" })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['removed'].should eq(true)
            doc.parsed_response['old']['_key'].should eq(key)
            doc.parsed_response['old']['name'].should eq(name)
            doc.parsed_response['new'].should eq(nil)

            doc = get_vertex(graph_name, user_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end
          
          it "can not delete a non existing vertex" do
            key = "unknownKey"

            doc = delete_vertex( sync, graph_name, user_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

        end

        context "check edge operations" do

          before do
            drop_graph(sync, graph_name)
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
          end

          after do
            drop_graph(sync, graph_name)
            ArangoDB.drop_collection(friend_collection)
            ArangoDB.drop_collection(user_collection)
          end

          it "can create an edge" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {})
            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)
          end
          
          it "can create an edge, returnNew" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, { "value" => "foo" }, { "returnNew" => "true" })
            doc.code.should eq(sync ? 201 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 201 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new']['value'].should eq("foo")
          end

          it "can get an edge" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = get_edge(graph_name, friend_collection, key)
            doc.code.should eq(200)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(200)
            doc.parsed_response['edge']['type'].should eq(type)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
          end

          it "can not get a non existing edge" do
            key = "unknownKey"

            doc = get_edge(graph_name, friend_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can replace an edge" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']
            oldTag = doc.parsed_response['edge']['_rev']
            oldTag.should eq(doc.headers['etag'])
            type = "divorced"

            doc = replace_edge( sync, graph_name, friend_collection, key, {"type2" => type, "_from" => v1, "_to" => v2})
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge']['type2'].should eq(type)
            doc.parsed_response['edge']['type'].should eq(nil)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            oldTag.should_not eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
          end
          
          it "can replace an edge, returnOld" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']
            oldTag = doc.parsed_response['edge']['_rev']
            oldTag.should eq(doc.headers['etag'])
            type = "divorced"

            doc = replace_edge( sync, graph_name, friend_collection, key, {"type2" => type, "_from" => v1, "_to" => v2}, { "returnOld" => "true", "returnNew" => "true" })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)

            doc.parsed_response['old']['_key'].should eq(key)
            doc.parsed_response['old']['type'].should eq("married")
            doc.parsed_response['old']['type2'].should eq(nil)
            doc.parsed_response['old']['_from'].should eq(v1)
            doc.parsed_response['old']['_to'].should eq(v2)

            doc.parsed_response['new']['_key'].should eq(key)
            doc.parsed_response['new']['type'].should eq(nil)
            doc.parsed_response['new']['type2'].should eq("divorced")
            doc.parsed_response['new']['_from'].should eq(v1)
            doc.parsed_response['new']['_to'].should eq(v2)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge']['type2'].should eq(type)
            doc.parsed_response['edge']['type'].should eq(nil)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            oldTag.should_not eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
          end
          
          it "can not replace a non existing edge" do
            key = "unknownKey"

            # Added _from and _to, because otherwise a 400 might conceal the
            # 404. Another test checking that missing _from or _to trigger
            # errors was added to api-gharial-spec.js.
            doc = replace_edge( sync, graph_name, friend_collection, key, {"type2" => "divorced", "_from" => "#{user_collection}/1", "_to" => "#{user_collection}/2"})
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can not replace a non valid edge" do
            key = "unknownKey"
            doc = replace_edge( sync, graph_name, friend_collection, key, {"type2" => "divorced", "_from" => "1", "_to" => "2"})
            doc.code.should eq(400)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("edge attribute missing or invalid")
            doc.parsed_response['code'].should eq(400)
          end

          it "can update an edge" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']
            type2 = "divorced"

            doc = update_edge( sync, graph_name, friend_collection, key, {"type2" => type2}, "")
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
            
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge']['type'].should eq(type)
            doc.parsed_response['edge']['type2'].should eq(type2)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
          end
          
          it "can update an edge, returnOld" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']
            type2 = "divorced"

            doc = update_edge( sync, graph_name, friend_collection, key, {"type2" => type2}, "", { "returnOld" => "true", "returnNew" => "true" })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
            doc.parsed_response['old']['_key'].should eq(key)
            doc.parsed_response['old']['type'].should eq("married")
            doc.parsed_response['old']['type2'].should eq(nil)
            doc.parsed_response['old']['_from'].should eq(v1)
            doc.parsed_response['old']['_to'].should eq(v2)
            doc.parsed_response['new']['_key'].should eq(key)
            doc.parsed_response['new']['type'].should eq("married")
            doc.parsed_response['new']['type2'].should eq("divorced")
            doc.parsed_response['new']['_from'].should eq(v1)
            doc.parsed_response['new']['_to'].should eq(v2)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge']['type'].should eq(type)
            doc.parsed_response['edge']['type2'].should eq(type2)
            doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
            doc.parsed_response['edge']['_key'].should eq(key)
          end
          
          it "can update an edge keepNull default(true)" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = update_edge( sync, graph_name, friend_collection, key, {"type" => nil}, "")
            doc.code.should eq(sync ? 200 : 202)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge'].key?('type').should eq(true)
            doc.parsed_response['edge']['type'].should eq(nil)
          end
    
          it "can update an edge keepNull true" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = update_edge( sync, graph_name, friend_collection, key, {"type" => nil}, true)
            doc.code.should eq(sync ? 200 : 202)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge'].key?('type').should eq(true)
            doc.parsed_response['edge']['type'].should eq(nil)
          end
    
          it "can update an edge keepNull false" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = update_edge( sync, graph_name, friend_collection, key, {"type" => nil}, false)
            doc.code.should eq(sync ? 200 : 202)

            doc = get_edge(graph_name, friend_collection, key)
            doc.parsed_response['edge'].key?('type').should eq(false)
          end

          it "can not update a non existing edge" do
            key = "unknownKey"

            doc = update_edge( sync, graph_name, friend_collection, key, {"type2" => "divorced"}, "")
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can delete an edge" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = delete_edge( sync, graph_name, friend_collection, key)
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['old'].should eq(nil)
            doc.parsed_response['new'].should eq(nil)

            doc = get_edge(graph_name, friend_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end
          
          it "can delete an edge, returnOld" do
            v1 = create_vertex( sync, graph_name, user_collection, {})
            v1.code.should eq(sync ? 201 : 202)
            v1 = v1.parsed_response['vertex']['_id']
            v2 = create_vertex( sync, graph_name, user_collection, {})
            v2.code.should eq(sync ? 201 : 202)
            v2 = v2.parsed_response['vertex']['_id']
            type = "married"
            doc = create_edge( sync, graph_name, friend_collection, v1, v2, {"type" => type})
            doc.code.should eq(sync ? 201 : 202)
            key = doc.parsed_response['edge']['_key']

            doc = delete_edge( sync, graph_name, friend_collection, key, { "returnOld" => "true" })
            doc.code.should eq(sync ? 200 : 202)
            doc.parsed_response['error'].should eq(false)
            doc.parsed_response['code'].should eq(sync ? 200 : 202)
            doc.parsed_response['old']['_from'].should eq(v1)
            doc.parsed_response['old']['_to'].should eq(v2)
            doc.parsed_response['old']['type'].should eq(type)
            doc.parsed_response['new'].should eq(nil)

            doc = get_edge(graph_name, friend_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

          it "can not delete a non existing edge" do
            key = "unknownKey"

            doc = delete_edge( sync, graph_name, friend_collection, key)
            doc.code.should eq(404)
            doc.parsed_response['error'].should eq(true)
            doc.parsed_response['errorMessage'].should include("document not found")
            doc.parsed_response['code'].should eq(404)
          end

        end

        context "check error codes" do

          before do
            drop_graph(sync, graph_name)
            definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
            create_graph(sync, graph_name, [definition])
          end

          after do
            drop_graph(sync, graph_name)
            ArangoDB.drop_collection(friend_collection)
            ArangoDB.drop_collection(user_collection)
          end

          describe "should throw 404 if graph is unknown on route" do

            def check404 (doc)
              doc.code.should eq(404)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(404)
              doc.parsed_response['errorNum'].should eq(1924)
              doc.parsed_response['errorMessage'].should eq("graph 'UnitTestUnknown' not found")
            end

            it "get graph" do
              check404(get_graph(unknown_name))
            end

            it "delete graph" do
              check404(drop_graph(sync, unknown_name))
            end

            it "list edge collections" do
              check404(list_edge_collections(unknown_name))
            end

            it "add edge definition" do
              definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
              check404(additional_edge_definition(sync, unknown_name, definition))
            end

            it "change edge definition" do
              definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
              check404(change_edge_definition(sync, unknown_name, friend_collection, definition))
            end

            it "delete edge definition" do
              check404(delete_edge_definition(sync, unknown_name, friend_collection))
            end

            it "list vertex collections" do
              check404(list_vertex_collections(unknown_name))
            end

            it "add vertex collection" do
              check404(additional_vertex_collection(sync, unknown_name, user_collection))
            end

            it "delete vertex collection" do
              check404(delete_vertex_collection( sync, unknown_name, user_collection))
            end

            it "create vertex" do
              check404(create_vertex( sync, unknown_name, unknown_name, {}))
            end

            it "get vertex" do
              check404(get_vertex(unknown_name, unknown_name, unknown_name))
            end

            it "update vertex" do
              check404(update_vertex( sync, unknown_name, unknown_name, unknown_name, {}))
            end

            it "replace vertex" do
              check404(replace_vertex( sync, unknown_name, unknown_name, unknown_name, {}))
            end

            it "delete vertex" do
              check404(delete_vertex( sync, unknown_name, unknown_name, unknown_name))
            end

            it "create edge" do
              check404(create_edge( sync, unknown_name, unknown_name, unknown_name, unknown_name, {}))
            end

            it "get edge" do
              check404(get_edge(unknown_name, unknown_name, unknown_name))
            end

            it "update edge" do
              check404(update_edge( sync, unknown_name, unknown_name, unknown_name, {}))
            end

            it "replace edge" do
              check404(replace_edge( sync, unknown_name, unknown_name, unknown_name, {}))
            end

            it "delete edge" do
              check404(delete_edge( sync, unknown_name, unknown_name, unknown_name))
            end

          end

          describe "should throw 404 if collection is unknown on route" do

            def check404 (doc)
              doc.code.should eq(404)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(404)
            end

            def check400 (doc)
              doc.code.should eq(400)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(400)
              puts doc.parsed_response['errorMessage']
              doc.parsed_response['errorMessage'].should include("edge attribute missing or invalid")
            end

            def check404Edge (doc)
              check404(doc)
              doc.parsed_response['errorNum'].should eq(1930)
              doc.parsed_response['errorMessage'].should eq("edge collection not used in graph")
            end

            def check404Vertex (doc)
              check404(doc)
              doc.parsed_response['errorNum'].should eq(1926)
            end

            def check400VertexUnused (doc)
              doc.parsed_response['errorNum'].should eq(1928)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(400)
              puts doc.parsed_response['errorMessage']
            end

            def check404CRUD (doc)
              check404(doc)
              doc.parsed_response['errorNum'].should eq(1203)
              doc.parsed_response['errorMessage'].should start_with("collection or view not found: ")
            end

            def check400CRUD (doc)
              check400(doc)
              doc.parsed_response['errorNum'].should eq(1233)
            end

            it "change edge definition" do
              definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
              check404Edge(change_edge_definition(sync, graph_name, unknown_name, definition))
            end

            it "delete edge definition" do
              check404Edge(delete_edge_definition(sync, graph_name, unknown_name))
            end

            it "delete vertex collection" do
              # this checks if a not used vertex collection can be removed of a graph
              check400VertexUnused(delete_vertex_collection( sync, graph_name, unknown_name))
            end

            it "create vertex" do
              check404CRUD(create_vertex( sync, graph_name, unknown_name, {}))
            end

            it "get vertex" do
              check404CRUD(get_vertex(graph_name, unknown_name, unknown_name))
            end

# TODO add tests where the edge/vertex collection is not part of the graph, but
# the given key exists!
            it "update vertex" do
              check404CRUD(update_vertex( sync, graph_name, unknown_name, unknown_name, {}))
            end

            it "replace vertex" do
              check404CRUD(replace_vertex( sync, graph_name, unknown_name, unknown_name, {}))
            end

            it "delete vertex" do
              check404CRUD(delete_vertex( sync, graph_name, unknown_name, unknown_name))
            end

            it "create edge" do
              check400CRUD(create_edge( sync, graph_name, unknown_name, unknown_name, unknown_name, {}))
            end

            it "get edge" do
              check404CRUD(get_edge(graph_name, unknown_name, unknown_name))
            end

            it "update edge" do
              check404CRUD(update_edge( sync, graph_name, unknown_name, unknown_name, {}))
            end

            it "replace edge (invalid key)" do
              check400CRUD(replace_edge( sync, graph_name, unknown_name, unknown_name, {}))
            end

            it "replace edge (valid key, but not existing)" do
              check404(replace_edge( sync, graph_name, user_collection + "/" + unknown_name, unknown_name, {}))
            end

            it "delete edge" do
              check404CRUD(delete_edge( sync, graph_name, unknown_name, unknown_name))
            end

          end

          describe "should throw 404 if document is unknown on route" do

            def check404 (doc)
              doc.code.should eq(404)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(404)
              doc.parsed_response['errorNum'].should eq(1202)
              doc.parsed_response['errorMessage'].should include("document not found")
            end

            def check404Collection (doc)
              doc.code.should eq(404)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(404)
              doc.parsed_response['errorNum'].should eq(1203)
              doc.parsed_response['errorMessage'].should include("collection or view not found")
            end

            def check400Collection (doc)
              doc.code.should eq(400)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(400)
              doc.parsed_response['errorNum'].should eq(1203)
              doc.parsed_response['errorMessage'].should include("no collection name specified")
            end

            def check400 (doc)
              doc.code.should eq(400)
              doc.parsed_response['error'].should eq(true)
              doc.parsed_response['code'].should eq(400)
              doc.parsed_response['errorNum'].should eq(1233)
              doc.parsed_response['errorMessage'].should include("edge attribute missing or invalid")
            end

            it "get vertex" do
              check404(get_vertex(graph_name, user_collection, unknown_name))
            end

            it "update vertex" do
              check404(update_vertex( sync, graph_name, user_collection, unknown_name, {}))
            end

            it "replace vertex" do
              check404(replace_vertex( sync, graph_name, user_collection, unknown_name, {}))
            end

            it "delete vertex" do
              check404(delete_vertex( sync, graph_name, user_collection, unknown_name))
            end

            it "get edge" do
              check404(get_edge(graph_name, friend_collection, unknown_name))
            end

            it "update edge" do
              check404(update_edge( sync, graph_name, friend_collection, unknown_name, {}))
            end
            
            it "replace edge invalid" do
              check400(replace_edge( sync, graph_name, friend_collection, unknown_name, {"_from" => "1", "_to" => "2"}))
            end

            it "replace edge (collection does not exist) not found" do
              # Added _from and _to, because otherwise a 400 might conceal the
              # 404. Another test checking that missing _from or _to trigger
              # errors was added to api-gharial-spec.js.
              check400Collection(replace_edge( sync, graph_name, friend_collection, unknown_name, {"_from" => "xyz/1", "_to" => "abc/2"}))
            end

            it "replace edge (document does not exist) not found" do
              # Added _from and _to, because otherwise a 400 might conceal the
              # 404. Another test checking that missing _from or _to trigger
              # errors was added to api-gharial-spec.js.
              check404(replace_edge( sync, graph_name, friend_collection, unknown_name, {"_from" => "#{user_collection}/1", "_to" => "#{user_collection}/2"}))
            end

            it "delete edge" do
              check404(delete_edge( sync, graph_name, friend_collection, unknown_name))
            end

          end


        end

      end

    end

  end

end

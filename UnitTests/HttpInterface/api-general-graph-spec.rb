# coding: utf-8

require 'rspec'
require 'json'
require 'arangodb.rb'

PREFIX = "api-general-graph"
URLPREFIX = "/system/gharial"

def drop_graph(graph_name)
  cmd = URLPREFIX + "/" + graph_name
  doc = ArangoDB.delete(cmd)
  return doc
end

def create_graph (name, edge_definitions) 
  cmd = URLPREFIX
  body = JSON.dump({:name => name, :edgeDefinitions => edge_definitions})
  doc = ArangoDB.post(cmd, :body => body)
  return doc
end

def vertex_endpoint(graph_name, collection)
  return URLPREFIX + "/" + graph_name + "/vertex/" + collection
end

def edge_endpoint(graph_name, collection)
  return URLPREFIX + "/" + graph_name + "/edge/" + collection
end

def list_edge_collections (graph_name) 
  cmd = URLPREFIX + "/" + graph_name + "/edge"
  doc = ArangoDB.get(cmd)
  return doc
end

def additional_edge_definition (graph_name, edge_definitions) 
  cmd = URLPREFIX + "/" + graph_name + "/edge"
  doc = ArangoDB.post(cmd, :body => JSON.dump(edge_definitions))
  return doc
end

def change_edge_definition (graph_name, definition_name, edge_definitions) 
  cmd = edge_endpoint(graph_name, definition_name)
  doc = ArangoDB.put(cmd, :body => JSON.dump(edge_definitions))
  return doc
end

def delete_edge_definition (graph_name, definition_name) 
  cmd = edge_endpoint(graph_name, definition_name)
  doc = ArangoDB.delete(cmd)
  return doc
end

def list_vertex_collections (graph_name) 
  cmd = URLPREFIX + "/" + graph_name + "/vertex"
  doc = ArangoDB.get(cmd)
  return doc
end

def additional_vertex_collection (graph_name, collection_name) 
  cmd = URLPREFIX + "/" + graph_name + "/vertex"
  body = { :collection => collection_name }
  doc = ArangoDB.post(cmd, :body => JSON.dump(body))
  return doc
end

def delete_vertex_collection (graph_name, collection_name) 
  cmd = vertex_endpoint(graph_name, collection_name)
  doc = ArangoDB.delete(cmd)
  return doc
end

def create_vertex (graph_name, collection, body) 
  cmd = vertex_endpoint(graph_name, collection)
  doc = ArangoDB.post(cmd, :body => JSON.dump(body))
  return doc
end

def get_vertex (graph_name, collection, key) 
  cmd = vertex_endpoint(graph_name, collection) + "/" + key
  doc = ArangoDB.get(cmd)
  return doc
end

def update_vertex (graph_name, collection, key, body, keepNull) 
  cmd = vertex_endpoint(graph_name, collection)
  cmd = cmd + "/" + key
  if keepNull != '' then
    cmd = cmd + "?keepNull=#{keepNull}"
  end
  doc = ArangoDB.patch(cmd, :body => JSON.dump(body))
  return doc
end

def replace_vertex (graph_name, collection, key, body) 
  cmd = vertex_endpoint(graph_name, collection)
  cmd = cmd + "/" + key
  doc = ArangoDB.put(cmd, :body => JSON.dump(body))
  return doc
end

def delete_vertex (graph_name, collection, key) 
  cmd = vertex_endpoint(graph_name, collection)
  cmd = cmd + "/" + key
  doc = ArangoDB.delete(cmd)
  return doc
end


def create_edge (graph_name, collection, from, to, body) 
  cmd = edge_endpoint(graph_name, collection)
  body["_from"] = from
  body["_to"] = to
  doc = ArangoDB.post(cmd, :body => JSON.dump(body))
  return doc
end

def get_edge (graph_name, collection, key) 
  cmd = edge_endpoint(graph_name, collection) + "/" + key
  doc = ArangoDB.get(cmd)
  return doc
end

def update_edge (graph_name, collection, key, body, keepNull) 
  cmd = edge_endpoint(graph_name, collection) + "/" + key
  if keepNull != '' then
    cmd = cmd + "?keepNull=" + keepNull
  end
  doc = ArangoDB.patch(cmd, :body => JSON.dump(body))
  return doc
end

def replace_edge (graph_name, collection, key, body) 
  cmd = edge_endpoint(graph_name, collection)
  cmd = cmd + "/" + key
  doc = ArangoDB.put(cmd, :body => JSON.dump(body))
  return doc
end

def delete_edge (graph_name, collection, key) 
  cmd = edge_endpoint(graph_name, collection)
  cmd = cmd + "/" + key
  doc = ArangoDB.delete(cmd)
  return doc
end

describe ArangoDB do

  user_collection = "UnitTestUsers"
  product_collection = "UnitTestProducts"
  friend_collection = "UnitTestFriends"
  bought_collection = "UnitTestBoughts"
  graph_name = "UnitTestGraph"

  context "testing general graph methods:" do


################################################################################
## checking graph creation process
################################################################################
  
    context "check creation of graphs" do
      before do
        drop_graph(graph_name)
      end

      after do
        drop_graph(graph_name)
      end

      it "can create an emtpy graph" do
        edge_definition = []
        doc = create_graph( graph_name, edge_definition )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
      end

      it "can create a graph with definitions" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        edge_definition = [first_def]
        doc = create_graph( graph_name, edge_definition )

        doc.code.should eq(201)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
      end

      it "can add additional edge definitions" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        edge_definition = [first_def]
        create_graph( graph_name, edge_definition )
        second_def = { "collection" => bought_collection, "from" => [user_collection], "to" => [product_collection] }
        doc = additional_edge_definition( graph_name, second_def )
        edge_definition.push(second_def)

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
      end

      it "can modify existing edge definitions" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph( graph_name, [first_def] )
        second_def = { "collection" => friend_collection, "from" => [product_collection], "to" => [user_collection] }
        edge_definition = [second_def]

        doc = change_edge_definition( graph_name, friend_collection, second_def )
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
      end

      it "can delete an edge definition" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        edge_definition = [first_def]
        create_graph( graph_name, edge_definition )
        doc = delete_edge_definition( graph_name, friend_collection )

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq([])
      end

      it "can add an additional orphan collection" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        edge_definition = [first_def]
        create_graph( graph_name, edge_definition )
        doc = additional_vertex_collection( graph_name, product_collection )

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
        doc.parsed_response['graph']['orphanCollections'].should eq([product_collection])
      end

      it "can delete an orphan collection" do
        first_def = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        edge_definition = [first_def]
        create_graph( graph_name, edge_definition )
        additional_vertex_collection( graph_name, product_collection )
        doc = delete_vertex_collection( graph_name, product_collection )

        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['graph']['name'].should eq(graph_name)
        doc.parsed_response['graph']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['graph']['edgeDefinitions'].should eq(edge_definition)
        doc.parsed_response['graph']['orphanCollections'].should eq([])
      end

      it "can delete a graph again" do
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
        doc = drop_graph(graph_name)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
      end

      it "can not delete a graph twice" do
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
        drop_graph(graph_name)
        doc = drop_graph(graph_name)
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq("graph not found")
        doc.parsed_response['code'].should eq(404)
      end

      it "can not create a graph twice" do
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
        doc = create_graph(graph_name, [definition])
        doc.code.should eq(409)
        doc.parsed_response['error'].should eq("graph already exists")
        doc.parsed_response['code'].should eq(409)
      end

      it "can get a list of vertex collections" do
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
        additional_vertex_collection(graph_name, product_collection)

        doc = list_vertex_collections(graph_name)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['collections'].should eq([product_collection, user_collection])
      end

      it "can get a list of edge collections" do
        definition1 = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        definition2 = { "collection" => bought_collection, "from" => [user_collection], "to" => [product_collection] }
        create_graph(graph_name, [definition1, definition2])

        doc = list_edge_collections(graph_name)
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['collections'].should eq([bought_collection, friend_collection])
      end
    end

    context "check vertex operations" do
      before do
        drop_graph(graph_name)
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
      end

      after do
        drop_graph(graph_name)
      end

      it "can create a vertex" do
        name = "Alice"
        doc = create_vertex(graph_name, user_collection, {"name" => name}) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
      end

      it "can get a vertex" do
        name = "Alice"
        doc = create_vertex(graph_name, user_collection, {"name" => name}) 
        key = doc.parsed_response['vertex']['_key']

        doc = get_vertex(graph_name, user_collection, key) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['name'].should eq(name)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['vertex']['_key'].should eq(key)
      end

      it "can replace a vertex" do
        name = "Alice"
        doc = create_vertex(graph_name, user_collection, {"name" => name}) 
        key = doc.parsed_response['vertex']['_key']
        oldTag = doc.parsed_response['vertex']['_rev']
        oldTag.should eq(doc.headers['etag'])
        name = "Bob"

        doc = replace_vertex(graph_name, user_collection, key, {"name2" => name}) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['vertex']['_key'].should eq(key)

        doc = get_vertex(graph_name, user_collection, key) 
        doc.parsed_response['vertex']['name'].should eq(nil)
        doc.parsed_response['vertex']['name2'].should eq(name)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
        oldTag.should_not eq(doc.headers['etag'])
        doc.parsed_response['vertex']['_key'].should eq(key)
      end

      it "can update a vertex" do
        name = "Alice"
        doc = create_vertex(graph_name, user_collection, {"name" => name}) 
        key = doc.parsed_response['vertex']['_key']
        name2 = "Bob"

        doc = update_vertex(graph_name, user_collection, key, {"name2" => name2}, "") 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['vertex']['_key'].should eq(key)

        doc = get_vertex(graph_name, user_collection, key) 
        doc.parsed_response['vertex']['name'].should eq(name)
        doc.parsed_response['vertex']['name2'].should eq(name2)
        doc.parsed_response['vertex']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['vertex']['_key'].should eq(key)
      end

      it "can delete a vertex" do
        name = "Alice"
        doc = create_vertex(graph_name, user_collection, {"name" => name}) 
        key = doc.parsed_response['vertex']['_key']

        doc = delete_vertex(graph_name, user_collection, key) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        doc = get_vertex(graph_name, user_collection, key) 
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq("document not found")
        doc.parsed_response['code'].should eq(404)
      end

    end

    context "check edge operations" do

      before do
        drop_graph(graph_name)
        definition = { "collection" => friend_collection, "from" => [user_collection], "to" => [user_collection] }
        create_graph(graph_name, [definition])
      end

      after do
        drop_graph(graph_name)
      end

      it "can create an edge" do
        v1 = create_vertex(graph_name, user_collection, {}) 
        v1 = v1.parsed_response['vertex']['_id']
        v2 = create_vertex(graph_name, user_collection, {}) 
        v2 = v2.parsed_response['vertex']['_id']
        doc = create_edge(graph_name, friend_collection, v1, v2, {}) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
      end

      it "can get an edge" do
        v1 = create_vertex(graph_name, user_collection, {}) 
        v1 = v1.parsed_response['vertex']['_id']
        v2 = create_vertex(graph_name, user_collection, {}) 
        v2 = v2.parsed_response['vertex']['_id']
        type = "married"
        doc = create_edge(graph_name, friend_collection, v1, v2, {"type" => type}) 
        key = doc.parsed_response['edge']['_key']

        doc = get_edge(graph_name, friend_collection, key) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['edge']['type'].should eq(type)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['edge']['_key'].should eq(key)
      end

      it "can replace an edge" do
        v1 = create_vertex(graph_name, user_collection, {}) 
        v1 = v1.parsed_response['vertex']['_id']
        v2 = create_vertex(graph_name, user_collection, {}) 
        v2 = v2.parsed_response['vertex']['_id']
        type = "married"
        doc = create_edge(graph_name, friend_collection, v1, v2, {"type" => type}) 
        key = doc.parsed_response['edge']['_key']
        oldTag = doc.parsed_response['edge']['_rev']
        oldTag.should eq(doc.headers['etag'])
        type = "divorced"

        doc = replace_edge(graph_name, friend_collection, key, {"type2" => type}) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['edge']['_key'].should eq(key)

        doc = get_edge(graph_name, friend_collection, key) 
        doc.parsed_response['edge']['type'].should eq(nil)
        doc.parsed_response['edge']['type2'].should eq(type)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
        oldTag.should_not eq(doc.headers['etag'])
        doc.parsed_response['edge']['_key'].should eq(key)
      end

      it "can update an edge" do
        v1 = create_vertex(graph_name, user_collection, {}) 
        v1 = v1.parsed_response['vertex']['_id']
        v2 = create_vertex(graph_name, user_collection, {}) 
        v2 = v2.parsed_response['vertex']['_id']
        type = "married"
        doc = create_edge(graph_name, friend_collection, v1, v2, {"type" => type}) 
        key = doc.parsed_response['edge']['_key']
        type2 = "divorced"

        doc = update_edge(graph_name, friend_collection, key, {"type2" => type2}, "") 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['edge']['_key'].should eq(key)

        doc = get_edge(graph_name, friend_collection, key) 
        doc.parsed_response['edge']['type'].should eq(type)
        doc.parsed_response['edge']['type2'].should eq(type2)
        doc.parsed_response['edge']['_rev'].should eq(doc.headers['etag'])
        doc.parsed_response['edge']['_key'].should eq(key)
      end

      it "can delete an edge" do
        v1 = create_vertex(graph_name, user_collection, {}) 
        v1 = v1.parsed_response['vertex']['_id']
        v2 = create_vertex(graph_name, user_collection, {}) 
        v2 = v2.parsed_response['vertex']['_id']
        type = "married"
        doc = create_edge(graph_name, friend_collection, v1, v2, {"type" => type}) 
        key = doc.parsed_response['edge']['_key']

        doc = delete_edge(graph_name, friend_collection, key) 
        doc.code.should eq(200)
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(200)

        doc = get_edge(graph_name, friend_collection, key) 
        doc.code.should eq(404)
        doc.parsed_response['error'].should eq("document not found")
        doc.parsed_response['code'].should eq(404)
      end

    end

  end

end

# coding: utf-8

require 'rspec'
require './arangodb.rb'

describe ArangoDB do
  api = "/_api/structures"
  prefix = "structures"

  context "dealing with structured documents" do
      
    def create_structure (prefix, body) 
      cmd = "/_api/document?collection=_structures"
      doc = ArangoDB.log_post(prefix, cmd, :body => body)
      return doc
    end

    def insert_structure1 (prefix) 
      # one numeric attribute "number"
      structure = '{ "_key" :"UnitTestsCollectionDocuments", "attributes": { ' +
        '"number": { ' +
        '  "type": "number", ' +
        '  "formatter": { ' +
        '    "default": { "args": { "decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "formatFloat" }, ' +
        '    "de": { "args": { "decPlaces": 4, "decSeparator": ",", "thouSeparator": "." }, "module": "org/arangodb/formatter", "do": "formatFloat" } ' +
        '  }, ' +
        ' "parser": { ' +
        '    "default": { "args": {"decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "parseFloat" }, ' +
        '    "de": { "args": {"decPlaces": 4,"decSeparator": ",","thouSeparator": "." },"module": "org/arangodb/formatter","do": "parseFloat" } ' +
        '  }, ' +
        '  "validators": [ { "module": "org/arangodb/formatter", "do": "validateNotNull" } ]' +
        '  }, ' +
        '"aString": { ' +
        '  "type": "string" ' +
        '}' +
        '}' +
        '}'

      return create_structure(prefix, structure);
    end

    def insert_structure2 (prefix) 
      # one numeric array attribute "numbers"
      structure = '{ "_key" :"UnitTestsCollectionDocuments", "attributes": { ' +
        '"numbers": { ' +
        '  "type": "number_list_type" ' +
        '}},' +
        '"arrayTypes": { ' +
        '  "number_list_type": { ' +
        '    "type": "number", ' +
        '    "formatter": { ' +
        '      "default": { "args": { "decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "formatFloat" }, ' +
        '      "de": { "args": { "decPlaces": 4, "decSeparator": ",", "thouSeparator": "." }, "module": "org/arangodb/formatter", "do": "formatFloat" } ' +
        '     }, ' +
        '   "parser": { ' +
        '      "default": { "args": {"decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "parseFloat" }, ' +
        '      "de": { "args": {"decPlaces": 4,"decSeparator": ",","thouSeparator": "." },"module": "org/arangodb/formatter","do": "parseFloat" } ' +
        '    }, ' +
        '    "validators": [ { "module": "org/arangodb/formatter", "do": "validateNotNull" } ]' +
        '  } ' +
        '}' +
        '}'

      return create_structure(prefix, structure);
    end


    def insert_structure3 (prefix) 
      # one object attribute "myObject"
      structure = '{ "_key" :"UnitTestsCollectionDocuments", "attributes": { ' +
        '"myObject": { ' +
        '  "type": "object_type", ' +
        '  "validators": [ { "module": "org/arangodb/formatter", "do": "validateNotNull" } ]' +
        '}},' +
        '"objectTypes": { ' +
        ' "object_type": { ' +
        '  "attributes": { ' +
        '    "aNumber": { ' +
        '      "type": "number", ' +
        '      "formatter": { ' +
        '        "default": { "args": { "decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "formatFloat" }, ' +
        '        "de": { "args": { "decPlaces": 4, "decSeparator": ",", "thouSeparator": "." }, "module": "org/arangodb/formatter", "do": "formatFloat" } ' +
        '       }, ' +
        '     "parser": { ' +
        '        "default": { "args": {"decPlaces": 4, "decSeparator": ".", "thouSeparator": "," }, "module": "org/arangodb/formatter", "do": "parseFloat" }, ' +
        '        "de": { "args": {"decPlaces": 4,"decSeparator": ",","thouSeparator": "." },"module": "org/arangodb/formatter","do": "parseFloat" } ' +
        '      }, ' +
        '      "validators": [ { "module": "org/arangodb/formatter", "do": "validateNotNull" } ]' +
        '    }, ' +
        '    "aString": { ' +
        '      "type": "string"' +
        '    }' +
        '  }' +
        ' }' +
        '}' +
        '}'

      return create_structure(prefix, structure);
    end
    
    def insert_structured_doc (prefix, api, collection, doc, lang, waitForSync, format)
      cmd = api + "?collection=" + collection + "&lang=" + lang + "&waitForSync=" + waitForSync + "&format=" + format;
      return ArangoDB.log_post(prefix, cmd, :body => doc)
    end

    def replace_structured_doc (prefix, api, id, doc, lang, waitForSync, format, args = {})
      cmd = api + "/" + id + "?lang=" + lang + "&waitForSync=" + waitForSync + "&format=" + format;
      return ArangoDB.log_put(prefix, cmd, :body => doc, :headers => args[:headers])
    end

    def update_structured_doc (prefix, api, id, doc, lang, waitForSync, format, args = {})
      cmd = api + "/" + id + "?lang=" + lang + "&waitForSync=" + waitForSync + "&format=" + format;
      return ArangoDB.log_patch(prefix, cmd, :body => doc, :headers => args[:headers])
    end

    def get_doc (prefix, api, id, lang, format, args = {})
      cmd = api + "/" + id + "?lang=" + lang + "&format=" + format;
      return ArangoDB.log_get(prefix, cmd, args)
    end

    def delete_doc (prefix, api, id, args = {})
      cmd = api + "/" + id;
      return ArangoDB.log_delete(prefix, cmd, args)
    end

    def head_doc (prefix, api, id, args = {})
      cmd = api + "/" + id;
      return ArangoDB.log_head(prefix, cmd, args)
    end

    before do
      @cn = "UnitTestsCollectionDocuments"

      ArangoDB.drop_collection(@cn)
      @cid = ArangoDB.create_collection(@cn, false)

      cmd = "/_api/document/_structures/" + @cn
      ArangoDB.delete(cmd)
    end

    after do
      ArangoDB.drop_collection(@cn)
    end

################################################################################
## creates documents with invalid types
################################################################################

    it "insert a document" do
      p = "#{prefix}-create-1"
      insert_structure1(p);

      body = '{ "number" : "1234.5" }';
      doc =  insert_structured_doc(p, api, @cn, body, "en", "false", "false")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)

      body = '{ "_key" : "a_key", "number" : "99.5" }';
      doc =  insert_structured_doc(p, api, @cn, body, "en", "true", "true")

      doc.code.should eq(201)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      doc.parsed_response['_key'].should eq("a_key")

      # insert same key (error)
      body = '{ "_key" : "a_key", "number" : "99.5" }';
      doc =  insert_structured_doc(p, api, @cn, body, "en", "true", "true")
      doc.code.should eq(400)

    end
    
    it "insert not valid document" do
      p = "#{prefix}-create-2"
      insert_structure1(p);

      body = '{  }';
      doc =  insert_structured_doc(p, api, @cn, body, "en", "true", "true")

      doc.code.should eq(400)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(400)
      doc.parsed_response['errorNum'].should eq(1)
    end

    it "insert document in unknown collection" do
      p = "#{prefix}-create-3"
      insert_structure1(p);

      body = '{  }';

      cmd = api + "?collection=egal&lang=en&waitForSync=true&format=true";
      doc = ArangoDB.log_post(p, cmd, :body => body)

      doc.code.should eq(404)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")

      doc.parsed_response['error'].should eq(true)
      doc.parsed_response['code'].should eq(404)
      doc.parsed_response['errorNum'].should eq(1203)
    end

################################################################################
## create and get objects
################################################################################

    it "insert a document with other language" do
      p = "#{prefix}-get-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      
      doc3 = get_doc(p, api, id, "en", "false") 
      doc3.code.should eq(200)
      doc3.headers['content-type'].should eq("application/json; charset=utf-8")
      doc3.parsed_response['_id'].should eq(id)
      doc3.parsed_response['number'].should eq(1234.5)

      doc2 = get_doc(p, api, id, "en", "true") 
      doc2.code.should eq(200)
      doc2.headers['content-type'].should eq("application/json; charset=utf-8")
      doc2.parsed_response['_id'].should eq(id)
      doc2.parsed_response['number'].should eq("1,234.5000")
    end

    it "insert a document with an array attribute" do
      p = "#{prefix}-get-2"
      insert_structure2(p);

      body = '{ "numbers" : [ "1.234,50", "99,99" ] }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      
      doc3 = get_doc(p, api, id, "en", "false") 
      doc3.code.should eq(200)
      doc3.headers['content-type'].should eq("application/json; charset=utf-8")
      doc3.parsed_response['_id'].should eq(id)
      doc3.parsed_response['numbers'][0].should eq(1234.5)
      doc3.parsed_response['numbers'][1].should eq(99.99)

      doc2 = get_doc(p, api, id, "en", "true") 
      doc2.code.should eq(200)
      doc2.headers['content-type'].should eq("application/json; charset=utf-8")
      doc2.parsed_response['_id'].should eq(id)
      doc2.parsed_response['numbers'][0].should eq("1,234.5000")
      doc2.parsed_response['numbers'][1].should eq("99.9900")
    end

    it "insert a document with an object attribute" do
      p = "#{prefix}-get-3"
      insert_structure3(p);

      body = '{ "myObject" : { "aNumber":"1.234,50", "aString":"str" } }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      
      doc3 = get_doc(p, api, id, "en", "false") 
      doc3.code.should eq(200)
      doc3.headers['content-type'].should eq("application/json; charset=utf-8")
      doc3.parsed_response['_id'].should eq(id)
      doc3.parsed_response['myObject']['aNumber'].should eq(1234.5)
      doc3.parsed_response['myObject']['aString'].should eq("str")

      doc2 = get_doc(p, api, id, "en", "true") 
      doc2.code.should eq(200)
      doc2.headers['content-type'].should eq("application/json; charset=utf-8")
      doc2.parsed_response['_id'].should eq(id)
      doc2.parsed_response['myObject']['aNumber'].should eq("1,234.5000")
      doc2.parsed_response['myObject']['aString'].should eq("str")
    end

################################################################################
## get objects with If-None-Match and If-Match
################################################################################

    it "get a document with If-None-Match" do
      p = "#{prefix}-get2-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']
      
      match = {}
      match['If-None-Match'] = '007';

      doc = get_doc(p, api, id, "en", "false", :headers => match) 
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['_id'].should eq(id)
      doc.parsed_response['number'].should eq(1234.5)

      match = {}
      match['If-None-Match'] = rev;

      doc = get_doc(p, api, id, "en", "false", :headers => match) 
      doc.code.should eq(304)
    end

    it "get a document with If-Match" do
      p = "#{prefix}-get2-2"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']
      
      match = {}
      match['If-Match'] = '007';

      doc = get_doc(p, api, id, "en", "false", :headers => match) 
      doc.code.should eq(412)

      match = {}
      match['If-Match'] = rev;

      doc = get_doc(p, api, id, "en", "false", :headers => match) 
      doc.code.should eq(200)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['_id'].should eq(id)
      doc.parsed_response['number'].should eq(1234.5)
    end

################################################################################
## get objects header with If-None-Match and If-Match
################################################################################

    it "get a document header with If-None-Match" do
      p = "#{prefix}-head-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']
      
      match = {}
      match['If-None-Match'] = '007';

      doc = head_doc(p, api, id, :headers => match) 
      doc.code.should eq(200)

      match = {}
      match['If-None-Match'] = rev;

      doc = head_doc(p, api, id, :headers => match) 
      doc.code.should eq(304)
    end

    it "get a document header with If-Match" do
      p = "#{prefix}-head-2"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']
      
      match = {}
      match['If-Match'] = '007';

      doc = head_doc(p, api, id, :headers => match) 
      doc.code.should eq(412)

      match = {}
      match['If-Match'] = rev;

      doc = head_doc(p, api, id, :headers => match) 
      doc.code.should eq(200)
    end

################################################################################
## replace documents
################################################################################

    it "replace a document" do
      p = "#{prefix}-put-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      key = doc.parsed_response['_key']

      # replace
      doc =  replace_structured_doc(p, api, id, body, "de", "false", "true")
      doc.code.should eq(202)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']

      # replace with wrong _rev
      body = '{ "_key":"' + key + '",  "_id":"' + id + '", "_rev":"error", "number" : "234,50" }';
      doc =  replace_structured_doc(p, api, id, body, "de", "false", "true&policy=error")
      doc.code.should eq(400)

      # replace with last _rev
      body = '{ "_key":"' + key + '",  "_id":"' + id + '", "_rev":"' + rev + '", "number" : "234,50" }';
      doc =  replace_structured_doc(p, api, id, body, "de", "true", "true&policy=error")
      doc.code.should eq(201)

    end

################################################################################
## patch documents
################################################################################

    it "patch a document" do
      p = "#{prefix}-patch-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50", "aString":"str" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      key = doc.parsed_response['_key']

      # patch string
      body = '{ "aString":"new string" }';
      doc =  update_structured_doc(p, api, id, body, "de", "false", "true")
      doc.code.should eq(202)
      id = doc.parsed_response['_id']
      rev = doc.parsed_response['_rev']

      doc3 = get_doc(p, api, id, "en", "true") 
      doc3.code.should eq(200)
      doc3.headers['content-type'].should eq("application/json; charset=utf-8")
      doc3.parsed_response['_id'].should eq(id)
      doc3.parsed_response['number'].should eq("1,234.5000")
      doc3.parsed_response['aString'].should eq("new string")

      # patch number to null (error)
      body = '{ "number" : null }';
      doc =  update_structured_doc(p, api, id, body, "de", "false", "true")
      doc.code.should eq(400)

    end

################################################################################
## delete documents
################################################################################

    it "delete a document" do
      p = "#{prefix}-delete-1"
      insert_structure1(p);

      body = '{ "number" : "1.234,50", "aString":"str" }';
      doc =  insert_structured_doc(p, api, @cn, body, "de", "false", "true")

      doc.code.should eq(202)
      doc.headers['content-type'].should eq("application/json; charset=utf-8")
      doc.parsed_response['error'].should eq(false)
      id = doc.parsed_response['_id']
      key = doc.parsed_response['_key']

      # delete
      doc = delete_doc(p, api, id)
      doc.code.should eq(202)
    end

  end
end

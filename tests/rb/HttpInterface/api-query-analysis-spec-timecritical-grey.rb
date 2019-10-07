# coding: utf-8

require 'rspec'
require 'arangodb.rb'
require 'json'

describe ArangoDB do

  before do
    @api = "/_api/query"
    @prefix = "api-query"
    @current = "#{@api}/current"
    @slow = "#{@api}/slow"
    @properties = "#{@api}/properties"
    @query = "FOR x IN 1..5 LET y = SLEEP(2) RETURN x"
    @queryBody = JSON.dump({query: @query})
    @fastQuery = "FOR x IN 1..1 LET y = SLEEP(0.2) RETURN x"
    @fastQueryBody = JSON.dump({query: @fastQuery})
    @longQuery = "FOR x IN 1..1 LET y = SLEEP(0.2) LET a = y LET b = a LET c = b LET d = c LET e = d LET f = e  RETURN x"
    @longQueryBody = JSON.dump({query: @longQuery})
    @queryWithBind = "FOR x IN 1..5 LET y = SLEEP(@value) RETURN x"
    @queryWithBindBody = JSON.dump({query: @queryWithBind, bindVars: {value: 2}})
    @queryEndpoint ="/_api/cursor"
    @queryPrefix = "api-cursor"
  end

  def send_queries (count = 1, async = "true") 
    count.times do
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @queryBody, :headers => {"X-Arango-Async" => async })
    end
  end

  def send_fast_queries (count = 1, async = "true") 
    count.times do
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @fastQueryBody, :headers => {"X-Arango-Async" => async })
    end
  end
  
  def send_queries_with_bind (count = 1, async = "true") 
    count.times do
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @queryWithBindBody, :headers => {"X-Arango-Async" => async })
    end
  end

  def contains_query (body, testq = @query)
      res = JSON.parse body
      res.each do |q|
        if q["query"] === testq
          return q
        end
      end
      return false
  end

  it "should activate tracking" do
    doc = ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({enable: true}))
    doc.code.should eq(200)
  end

  describe "tracking" do

    before do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({enable: true, slowQueryThreshold: 20, trackSlowQueries: true}))
    end

    before(:each) do
      ArangoDB.log_delete("#{@prefix}-delete", @slow)
    end

    after(:each) do
      # Let the queries finish
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      res = JSON.parse doc.body
      count = 0
      while res.length > 0 && count < 10 do
        sleep 1
        doc = ArangoDB.log_get("#{@prefix}-current", @current)
        res = JSON.parse doc.body
        count += 1
      end
    end

    it "should track running queries" do
      send_queries
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      doc.code.should eq(200)
      found = contains_query doc.body, @query
      found.should_not eq(false)
      found.should have_key("id")
      found["id"].should match(/^\d+$/)
      found.should have_key("query")
      found["query"].should eq(@query)
      found.should have_key("bindVars")
      found["bindVars"].should eq({})
      found.should have_key("runTime")
      found["runTime"].should be_kind_of(Numeric)
      found.should have_key("started")
      found.should have_key("state")
      found["state"].should be_kind_of(String)
    end
    
    it "should track running queries, with bind parameters" do
      send_queries_with_bind
      doc = ArangoDB.log_get("#{@prefix}-current-bind", @current)
      doc.code.should eq(200)
      found = contains_query doc.body, @queryWithBind
      found.should_not eq(false)
      found.should have_key("id")
      found["id"].should match(/^\d+$/)
      found.should have_key("query")
      found["query"].should eq(@queryWithBind)
      found.should have_key("bindVars")
      found["bindVars"].should eq({"value" => 2})
      found.should have_key("runTime")
      found["runTime"].should be_kind_of(Numeric)
      found.should have_key("started")
      found.should have_key("state")
      found["state"].should be_kind_of(String)
    end

    it "should track slow queries by threshold" do
      send_fast_queries 1,"false"
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      found = contains_query doc.body, @fastQuery
      found.should eq(false)

      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 0.1}))

      send_fast_queries 1,"false"
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      doc.code.should eq(200)
      found = contains_query doc.body, @fastQuery
      found.should eq(false)
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      found = contains_query doc.body, @fastQuery
      found.should_not eq(false)
      found.should have_key("query")
      found["query"].should eq(@fastQuery)
      found["bindVars"].should eq({})
      found.should have_key("runTime")
      found["runTime"].should be_kind_of(Numeric)
      found.should have_key("started")
      found["state"].should eq("finished")
    end

    it "should track at most n slow queries" do 
      max = 3
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 0.1, maxSlowQueries: max}))
      send_fast_queries 6, "false"

      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(max)
    end

    it "should not track slow queries if turned off" do
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      found = contains_query doc.body, @fastQuery
      found.should eq(false)

      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 0.1, trackSlowQueries: false}))
      send_fast_queries 1, "false"

      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      found = contains_query doc.body, @fastQuery
      found.should eq(false)
    end

    it "should truncate the query string to at least 64 bytes" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 2, maxQueryStringLength: 12}))
      doc = ArangoDB.log_get("#{@prefix}-properties", @properties)
      res = JSON.parse doc.body
      res["maxQueryStringLength"].should eq(64)
    end

    it "should truncate the query string" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 0.1, maxQueryStringLength: 64}))
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => @longQueryBody)
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
      # This string is exactly 64 bytes long
      shortened = "FOR x IN 1..1 LET y = SLEEP(0.2) LET a = y LET b = a LET c = b L"
      found = contains_query doc.body, shortened + "..."
      found.should_not eq(false)
      found.should have_key("query")
      found["query"].should eq(shortened + "...")
      found["bindVars"].should eq({})
    end

    it "should properly truncate UTF8 symbols" do
      ArangoDB.log_put("#{@prefix}-properties", @properties, :body => JSON.dump({slowQueryThreshold: 0.1, maxQueryStringLength: 64}))
      ArangoDB.log_post(@queryPrefix, @queryEndpoint, :body => JSON.dump({query: 'FOR x IN 1..1 LET y = SLEEP(0.2) LET a= y LET b= a LET c= "ööööööö" RETURN c'}))
      doc = ArangoDB.log_get("#{@prefix}-slow", @slow)
      doc.code.should eq(200)
      res = JSON.parse doc.body
      res.length.should eq(1)
      # This string is exactly 64 bytes long
      shortened = "FOR x IN 1..1 LET y = SLEEP(0.2) LET a= y LET b= a LET c= \"öö"
      found = contains_query doc.body, shortened + "..."
      found.should_not eq(false)
      found.should have_key("query")
      found["query"].should eq(shortened + "...")
      found["bindVars"].should eq({})
    end

    it "should be able to kill a running query" do
      send_queries
      sleep 3
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      found = false
      res = JSON.parse doc.body
      id = ""
      res.each do |q|
        if q["query"] === @query
          found = true
          id = q["id"]
        end
      end
      found.should eq(true)
      doc = ArangoDB.log_delete(@prefix, "#{@api}/" + id)
      doc.code.should eq(200)
      sleep 5
      doc = ArangoDB.log_get("#{@prefix}-current", @current)
      found = contains_query doc.body, @query
      found.should eq(false)
    end

  end

end

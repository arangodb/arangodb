#! ruby -rubygems
# coding: utf-8

require 'arangodb'

# configuration
$number_reader = 30
$number_writer = 20
$number_deleter = 10
$maximal_size = 1000
$verbose = false


################################################################################
## list of all active documents
################################################################################

$documents = []
$doc_mutex = Mutex.new
$doc_cv = ConditionVariable.new

################################################################################
## print version number of the server
################################################################################

doc = ArangoDB.get("/_admin/version")

puts "starting stress test, ArangoDB #{doc.parsed_response['version']}"

################################################################################
## create a collection for testing
################################################################################

cn = "StressTest#{Time.now.to_i}"

ArangoDB.delete("/_api/collection/#{cn}")

body = "{ \"name\" : \"#{cn}\" }"
doc = ArangoDB.post("/_api/collection", :body => body)
$cid = doc.parsed_response['id']

puts "create collection \"#{cn}\": #{$cid}"

################################################################################
## function to a read a document
################################################################################

def read_document (i, pos)
  did = nil

  $doc_mutex.synchronize {
    if $documents.length == 0
      return pos
    end

    pos = (pos * 13 + 1) % $documents.length

    did = $documents[pos]

    if pos + 1 == $documents.length
      $documents.pop()
    else
      $documents[pos] = $documents.pop()
    end
  }

  res = ArangoDB.get("/_api/document/#{did}")

  if res.code == 200
    if $verbose
      puts "read document #{pos} in thread #{i}: #{did}"
    end
  else
    puts "FAILED READ:"
    puts "thread: #{i}"
    puts "document identifier: #{did}"
    puts JSON.pretty_generate(res.parsed_response)
    puts "--------------"
  end

  $doc_mutex.synchronize {
    $documents << did
  }

  return pos
end

################################################################################
## function to a create a document
################################################################################

def create_document (i)
  len = 0
  body = "{ \"Hallo\" : #{i} }"
  res = ArangoDB.post("/_api/document?collection=#{$cid}", :body => body)

  if res.code == 201 or res.code == 202
    did = res.parsed_response['_id']

    $doc_mutex.synchronize {
      $documents << did
      len = $documents.length
    }

    if $verbose
      puts "created document #{len} in thread #{i}: #{did}"
    end
  else
    puts "FAILED CREATE:"
    puts JSON.pretty_generate(res.parsed_response)
    puts "--------------"
  end
end

################################################################################
## function to a delete a document
################################################################################

def delete_document (i, pos)
  did = nil

  $doc_mutex.synchronize {
    if $documents.length == 0
      return pos
    end

    pos = (pos * 13 + 1) % $documents.length

    did = $documents[pos]

    if pos + 1 == $documents.length
      $documents.pop()
    else
      $documents[pos] = $documents.pop()
    end
  }

  res = ArangoDB.delete("/_api/document/#{did}")

  if res.code == 200
    if $verbose
      puts "deleted document #{pos} in thread #{i}: #{did}"
    end
  else
    puts "FAILED DELETE:"
    puts "thread: #{i}"
    puts "document identifier: #{did}"
    puts JSON.pretty_generate(res.parsed_response)
    puts "--------------"
  end

  return pos
end

################################################################################
## reader
################################################################################

def reader (i) 
  pos = i % $maximal_size

  while true
    $doc_mutex.synchronize {
      while $documents.length <= 10
  $doc_cv.wait($doc_mutex)
      end
    }

    pos = read_document(i, pos)

    $doc_mutex.synchronize {
      $doc_cv.broadcast
    }
  end
end

################################################################################
## writer
################################################################################

def writer (i) 
  while true
    $doc_mutex.synchronize {
      while $maximal_size <= $documents.length 
  $doc_cv.wait($doc_mutex)
      end
    }

    create_document(i)

    $doc_mutex.synchronize {
      $doc_cv.broadcast
    }
  end
end

################################################################################
## deleter
################################################################################

def deleter (i) 
  pos = i % $maximal_size

  while true
    $doc_mutex.synchronize {
      while $documents.length < $maximal_size
  $doc_cv.wait($doc_mutex)
      end
    }

    pos = delete_document(i, pos)

    $doc_mutex.synchronize {
      $doc_cv.broadcast
    }
  end
end

################################################################################
## main
################################################################################

threads = []

$number_reader.times { |i|
  threads << Thread.new(i) {
    reader(i)
  }
}

$number_writer.times { |i|
  threads << Thread.new(i) {
    writer(i)
  }
}

$number_deleter.times { |i|
  threads << Thread.new(i) {
    deleter(i)
  }
}

if $verbose
  sleep
else
  while true
    $doc_mutex.synchronize {
      puts "number of documents: #{$documents.length}"
    }

    sleep(10)
  end
end

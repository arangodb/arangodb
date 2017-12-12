#Exporting Data for Offline Processing

In this recipe we will learn how to use the [export API][1] to extract data and process it with PHP. At the end of the recipe you can download the complete PHP script.

**Note**: The following recipe is written using an ArangoDB server with version 2.6 or higher. You can also use the `devel` branch, since version 2.6 hasn't been an official release yet.

##Howto
###Importing example data

First of all we need some data in an ArangoDB collection. For this example we will use a collection named `users` which we will populate with 100.000 [example documents][2]. This way you can get the data into ArangoDB:

```bash
# download data file
wget https://jsteemann.github.io/downloads/code/users-100000.json.tar.gz
# uncompress it
tar xvfz users-100000.json.tar.gz
# import into ArangoDB 
arangoimp --file users-100000.json --collection users --create-collection true
```

###Setting up ArangoDB-PHP
For this recipe we will use the [ArangoDB PHP driver][3]:

```bash
git clone -b devel "https://github.com/arangodb/arangodb-php.git"
```

We will now write a simple PHP script that establishes a connection to ArangoDB on localhost:

```php
<?php

namespace triagens\ArangoDb;

// use the driver's autoloader to load classes
require 'arangodb-php/autoload.php';
Autoloader::init();

// set up connection options
$connectionOptions = array(
    // endpoint to connect to
    ConnectionOptions::OPTION_ENDPOINT     => 'tcp://localhost:8529',
    // can use Keep-Alive connection
    ConnectionOptions::OPTION_CONNECTION   => 'Keep-Alive',
    // use basic authorization
    ConnectionOptions::OPTION_AUTH_TYPE    => 'Basic',
    // user for basic authorization
    ConnectionOptions::OPTION_AUTH_USER    => 'root',
    // password for basic authorization
    ConnectionOptions::OPTION_AUTH_PASSWD  => '',
    // timeout in seconds
    ConnectionOptions::OPTION_TIMEOUT      => 30,
    // database name 
    ConnectionOptions::OPTION_DATABASE     => '_system'
    );

try {
  // establish connection
  $connection = new Connection($connectionOptions);

  echo 'Connected!' . PHP_EOL;

  // TODO: now do something useful with the connection!

} catch (ConnectException $e) {
  print $e . PHP_EOL;
} catch (ServerException $e) {
  print $e . PHP_EOL;
} catch (ClientException $e) {
  print $e . PHP_EOL;
}
```

After running the script you should see `Connected!` in the bash if successful.

###Extracting the data

Now we can run an export of the data in the collection `users`. Place the following code into the `TODO` part of the first code:

```php
function export($collection, Connection $connection) {
  $fp = fopen('output.json', 'w');

  if (! $fp) {
    throw new Exception('could not open output file!');
  }

  // settings to use for the export
  $settings = array(
      'batchSize' => 5000,  // export in chunks of 5K documents
      '_flat' => true       // use simple PHP arrays
      );

  $export = new Export($connection, $collection, $settings);

  // execute the export. this will return an export cursor
  $cursor = $export->execute();

  // statistics
  $count   = 0;
  $batches = 0;
  $bytes   = 0;

  // now we can fetch the documents from the collection in batches
  while ($docs = $cursor->getNextBatch()) {
    $output = '';
    foreach ($docs as $doc) {
      $output .= json_encode($doc) . PHP_EOL;
    }

    // write out chunk
    fwrite($fp, $output);

    // update statistics
    $count += count($docs);
    $bytes += strlen($output);
    ++$batches;
  }

  fclose($fp);

  echo sprintf('written %d documents in %d batches with %d total bytes',
      $count,
      $batches,
      $bytes) . PHP_EOL;
}

// run the export
export('users', $connection);
```

The function extracts all documents from the collection and writes them into an output file `output.json`. In addition it will print some statistics about the number of documents and the total data size:

```json
written 100000 documents in 20 batches with 40890013 total bytes
```

###Applying some transformations
We now will use PHP to transform data as we extract it:

```php
function transformDate($value) {
  return preg_replace('/^(\\d+)-(\\d+)-(\\d+)$/', '\\2/\\3/\\1', $value);
}

function transform(array $document) {
  static $genders = array('male' => 'm', 'female' => 'f');

  $transformed = array(
      'gender'      => $genders[$document['gender']],
      'dob'         => transformDate($document['birthday']),
      'memberSince' => transformDate($document['memberSince']),
      'fullName'    => $document['name']['first'] . ' ' . $document['name']['last'],
      'email'       => $document['contact']['email'][0]
      );

  return $transformed;
}

function export($collection, Connection $connection) {
  $fp = fopen('output-transformed.json', 'w');

  if (! $fp) {
    throw new Exception('could not open output file!');
  }

  // settings to use for the export
  $settings = array(
      'batchSize' => 5000,  // export in chunks of 5K documents
      '_flat' => true       // use simple PHP arrays
      );

  $export = new Export($connection, $collection, $settings);

  // execute the export. this will return an export cursor
  $cursor = $export->execute();

  // now we can fetch the documents from the collection in batches
  while ($docs = $cursor->getNextBatch()) {
    $output = '';
    foreach ($docs as $doc) {
      $output .= json_encode(transform($doc)) . PHP_EOL;
    }

    // write out chunk
    fwrite($fp, $output);
  }

  fclose($fp);
}

// run the export
export('users', $connection);
```

With this script the following changes will be made on the data:
- rewrite the contents of the `gender`attribute. `female` becomes `f` and `male` becomes `m`
- `birthday` now becomes `dob`
- the date formations will be changed from YYYY-MM-DD to MM/DD/YYYY
- concatenate the contents of `name.first` and `name.last`
- `contact.email` will be transformed from an array to a flat string
- every other attribute will be removed

**Note**: The output will be in a file named `output-transformed.json`.

###Filtering attributes
####Exclude certain attributes
Instead of filtering out as done in the previous example we can easily configure the export to exclude these attributes server-side:

```php
// settings to use for the export
$settings = array(
  'batchSize' => 5000,  // export in chunks of 5K documents
  '_flat' => true,      // use simple PHP arrays
  'restrict' => array(
    'type' => 'exclude',
    'fields' => array('_id', '_rev', '_key', 'likes')
  )
);
```

This script will exclude the attributes `_id`, `_rev`. `_key` and `likes`.

####Include certain attributes
We can also include attributes with the following script:

```php
function export($collection, Connection $connection) {
  // settings to use for the export
  $settings = array(
      'batchSize' => 5000,  // export in chunks of 5K documents
      '_flat' => true,      // use simple PHP arrays
      'restrict' => array(
        'type' => 'include',
        'fields' => array('_key', 'name')
        )
      );

  $export = new Export($connection, $collection, $settings);

  // execute the export. this will return an export cursor
  $cursor = $export->execute();

  // now we can fetch the documents from the collection in batches
  while ($docs = $cursor->getNextBatch()) {
    $output = '';

    foreach ($docs as $doc) {
      $values = array(
          $doc['_key'],
          $doc['name']['first'] . ' ' . $doc['name']['last']
          );

      $output .= '"' . implode('","', $values) . '"' . PHP_EOL;
    }

    // print out the data directly 
    print $output;
  }
}

// run the export
export('users', $connection);
```

In this script only the `_key` and `name` attributes are extracted. In the prints the `_key`/`name` pairs are in CSV format.

**Note**: The whole script [can be downloaded][4].

###Using the API without PHP

The export API REST interface can be used with any client that can speak HTTP like curl. With the following command you can fetch the documents from the `users` collection:

```bash
curl
  -X POST
  http://localhost:8529/_api/export?collection=users
--data '{"batchSize":5000}'
```

The HTTP response will contatin a `result` attribute that contains the actual documents. The attribute `hasMore` will indicate if there are more documents for the client to fetch.
The HTTP will also contain an attribute `id` if set to _true_.

With the `id` you can send follow-up requests like this:

```bash
curl
  -X PUT
  http://localhost:8529/_api/export/13979338067709
```


**Authors:** [Thomas Schmidts](https://github.com/13abylon) and [Jan Steemann](https://github.com/jsteemann)

**Tags**: #howto #php


[1]: https://jsteemann.github.io/blog/2015/04/04/more-efficient-data-exports/
[2]: https://jsteemann.github.io/downloads/code/users-100000.json.tar.gz
[3]: https://github.com/arangodb/arangodb-php
[4]: https://jsteemann.github.io/downloads/code/export-csv.php

Collections
===========

The collections section displays all available collections. From here you can
create new collections and jump into a collection for details (click on a
collection tile).

![Collections](images/collectionsView.png)

Functions:

 - A: Toggle filter properties
 - B: Search collection by name
 - D: Create collection
 - C: Filter properties
 - H: Show collection details (click tile)

Information:

 - E: Collection type
 - F: Collection state(unloaded, loaded, ...)
 - G: Collection name

### Collection

![Collection](images/collectionView.png)

There are four view categories: 

1. Content:
 - Create a document
 - Delete a document
 - Filter documents
 - Download documents
 - Upload documents

2. Indices:
 - Create indices
 - Delete indices

3. Info:
 - Detailed collection information and statistics 

3. Settings:
 - Configure name, journal size, index buckets, wait for sync 
 - Delete collection 
 - Truncate collection 
 - Unload/Load collection 
 - Save modifed properties (name, journal size, index buckets, wait for sync) 

Additional information:

Upload format:

I. Line-wise
```js
{ "_key": "key1", ... }
{ "_key": "key2", ... }
```

II. JSON documents in a list
```js
[
  { "_key": "key1", ... },
  { "_key": "key2", ... }
]
```


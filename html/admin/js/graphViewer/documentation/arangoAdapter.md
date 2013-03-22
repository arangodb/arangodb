## Configuration

(arangoPath (localhost:8529))
For our frontend this can be omitted


#AQL to select graph in correct format




    FOR u IN users
      LET friends = (
        FOR f IN likes 
          FILTER u._id == f._from
          for t in users
            filter t._id == f._to
            RETURN {"id" : t._key}
      )
      RETURN {
          "id" : u._key,
          "name": u.name,
          "children" : friends
      }
      
      

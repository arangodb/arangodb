'use strict'
//const joi = require('joi');

const createRouter = require('@arangodb/foxx/router');
const router = createRouter();
const db = require('@arangodb').db;
const aql = require('@arangodb').aql;
const fs = require('fs');


router.get('/pagerank', function (req, res) {
   console._log("DEBUG", "JULIA");
    const collections = db._collections().map((collection) => {
        print("inside POST ", req.body);
        return {
          print("RETURN");
          name: collection.name(),
          type: collection.type() === 2 ? 'document' : 'edge',
          isSystem: collection.properties().isSystem,
        };
      });
    print("AFTER");
    const filePath = module.context.fileName("queries/init_pagerank.aql");
    var queryText = fs.read(filePath);

    const init_val = 1.0/collections.length;

    print('INIT_VAL: ', init_val)
    for (let c in collections) {
        if (!c.includes("_")) {
            print(c)
            var obj = {
                '@collection': c.name,
                'init_val': init_val,
                'gss': 100
            }
            const query = db._query(queryText, obj)
        }
    }
  res.status(302);
  return res;
})
module.context.use(router);

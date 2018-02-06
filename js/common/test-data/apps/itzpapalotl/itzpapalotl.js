/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief An example Foxx-Application for ArangoDB
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  "use strict";

  const createRouter = require('@arangodb/foxx/router');
  const router = createRouter();

  module.context.use(router);

  // include console module so we can log something (in the server's log)
  var console = require("console");
  var ArangoError = require("@arangodb").ArangoError;

  // we also need this module for custom responses
  var actions = require("@arangodb/actions");

  // use joi for validation
  var joi = require("joi");

  // our app is about the following Aztec deities:
  var deities = [
    "CentzonTotochtin",
    "Chalchihuitlicue",
    "Chantico",
    "Chicomecoatl",
    "Cihuacoatl",
    "Cinteotl",
    "Coatlicue",
    "Coyolxauhqui",
    "Huehuecoyotl",
    "Huitzilopochtli",
    "Ilamatecuhtli",
    "Itzcoliuhqui",
    "Itzpaplotl",
    "Mayauel",
    "Meztli",
    "Mictlantecuhtli",
    "Mixcoatl",
    "Quetzalcoatl",
    "Tezcatlipoca",
    "Tialoc",
    "Tlauixcalpantecuhtli",
    "Tlazolteotl",
    "Tonatiuh",
    "Tozi",
    "XipeTotec",
    "Xochiquetzal",
    "Xolotl",
    "Yacatecuhtli"
  ];

  // install index route (this is the default route mentioned in manifest.json)
  // this route will create an HTML overview page
  router.get('/index', function (req, res) {
    res.set("content-type", "text/html");

    var body = "<h1>" + module.context.service.manifest.name + " (" + module.context.service.manifest.version + ")</h1>";
    body += "<h2>an example application demoing a few Foxx features</h2>";

    deities.forEach(function(deity) {
      body += "summon <a href=\"" + encodeURIComponent(deity) + "/summon\">" + deity + "</a><br />";
    });

    body += "<hr />pick a <a href=\"random\">random</a> Aztec deity";

    res.body = body;
  })
  .summary("prints an overview page");

  // install route to return a random deity name in JSON
  router.get('/random', function (req, res) {
    var idx = Math.round(Math.random() * (deities.length - 1));
    res.json({ name: deities[idx] });
  })
  .summary("returns a random deity name");

  // install deity-specific route for summoning
  // deity name is passed as part of the URL
  router.get('/:deity/summon', function (req, res) {
    var deity = req.pathParams.deity;

    console.log("request to summon %s", deity);

    if (deities.indexOf(deity) === -1) {
      // unknown deity
      res.throw(404, "The requested deity could not be found");
    }

    console.log("summoning %s", deity);

    res.json({ name: deity, summoned: true });
  })
  .summary("summons the requested deity")
  .pathParam("deity",
    joi.string().required()
  );

}());

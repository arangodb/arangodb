'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph Data for Example
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var Graph = require("org/arangodb/general-graph");

var createTraversalExample = function() {
  var g = Graph._create("knows_graph",
    [Graph._relation("knows", "persons", "persons")]
  );
  var a = g.persons.save({name: "Alice", _key: "alice"})._id;
  var b = g.persons.save({name: "Bob", _key: "bob"})._id;
  var c = g.persons.save({name: "Charlie", _key: "charlie"})._id;
  var d = g.persons.save({name: "Dave", _key: "dave"})._id;
  var e = g.persons.save({name: "Eve", _key: "eve"})._id;
  g.knows.save(a, b, {});
  g.knows.save(b, c, {});
  g.knows.save(b, d, {});
  g.knows.save(e, a, {});
  g.knows.save(e, b, {});
  return g;
};

var createSocialGraph = function() {
  var edgeDefinition = [];
  edgeDefinition.push(Graph._relation("relation", ["female", "male"], ["female", "male"]));
  var g = Graph._create("social", edgeDefinition);
  var a = g.female.save({name: "Alice", _key: "alice"});
  var b = g.male.save({name: "Bob", _key: "bob"});
  var c = g.male.save({name: "Charly", _key: "charly"});
  var d = g.female.save({name: "Diana", _key: "diana"});
  g.relation.save(a._id, b._id, {type: "married", _key: "aliceAndBob"});
  g.relation.save(a._id, c._id, {type: "friend", _key: "aliceAndCharly"});
  g.relation.save(c._id, d._id, {type: "married", _key: "charlyAndDiana"});
  g.relation.save(b._id, d._id, {type: "friend", _key: "bobAndDiana"});
  return g;
};

var createRoutePlannerGraph = function() {
  var edgeDefinition = [];
  edgeDefinition.push(Graph._relation(
    "germanHighway", ["germanCity"], ["germanCity"])
  );
  edgeDefinition.push(
    Graph._relation("frenchHighway", ["frenchCity"], ["frenchCity"])
  );
  edgeDefinition.push(Graph._relation(
    "internationalHighway", ["frenchCity", "germanCity"], ["frenchCity", "germanCity"])
  );
  var g = Graph._create("routeplanner", edgeDefinition);
  var berlin = g.germanCity.save({_key: "Berlin", population : 3000000, isCapital : true});
  var cologne = g.germanCity.save({_key: "Cologne", population : 1000000, isCapital : false});
  var hamburg = g.germanCity.save({_key: "Hamburg", population : 1000000, isCapital : false});
  var lyon = g.frenchCity.save({_key: "Lyon", population : 80000, isCapital : false});
  var paris = g.frenchCity.save({_key: "Paris", population : 4000000, isCapital : true});
  g.germanHighway.save(berlin._id, cologne._id, {distance: 850});
  g.germanHighway.save(berlin._id, hamburg._id, {distance: 400});
  g.germanHighway.save(hamburg._id, cologne._id, {distance: 500});
  g.frenchHighway.save(paris._id, lyon._id, {distance: 550});
  g.internationalHighway.save(berlin._id, lyon._id, {distance: 1100});
  g.internationalHighway.save(berlin._id, paris._id, {distance: 1200});
  g.internationalHighway.save(hamburg._id, paris._id, {distance: 900});
  g.internationalHighway.save(hamburg._id, lyon._id, {distance: 1300});
  g.internationalHighway.save(cologne._id, lyon._id, {distance: 700});
  g.internationalHighway.save(cologne._id, paris._id, {distance: 550});
  return g;
};

var createMoviesGraph = function (prefixed) {
  let Person = "Person";
  let Movie = "Movie";
  let ACTED_IN = "ACTED_IN";
  let DIRECTED = "DIRECTED";
  let PRODUCED = "PRODUCED";
  let WROTE = "WROTE";
  let REVIEWED = "REVIEWED";
  let FOLLOWS = "FOLLOWS";
  let graph = "movies";
  if (prefixed) {
    Person = "UnitTest" + Person;
    Movie = "UnitTest" + Movie;
    ACTED_IN = "UnitTest" + ACTED_IN;
    DIRECTED = "UnitTest" + DIRECTED;
    PRODUCED = "UnitTest" + PRODUCED;
    WROTE = "UnitTest" + WROTE;
    REVIEWED = "UnitTest" + REVIEWED;
    FOLLOWS = "UnitTest" + FOLLOWS;
    graph = "UnitTest" + graph;
  }
  var edgeDefinition = [];
  edgeDefinition.push(Graph._relation(
    ACTED_IN, Person, Movie
  ));
  edgeDefinition.push(Graph._relation(
    DIRECTED, Person, Movie
  ));
  edgeDefinition.push(Graph._relation(
    PRODUCED, Person, Movie
  ));
  edgeDefinition.push(Graph._relation(
    WROTE, Person, Movie
  ));
  edgeDefinition.push(Graph._relation(
    REVIEWED, Person, Movie
  ));
  edgeDefinition.push(Graph._relation(
    FOLLOWS, Person, Person
  ));
  var g = Graph._create(graph, edgeDefinition);

  var TheMatrix = g[Movie].save(
    {_key: "TheMatrix", title:'The Matrix', released:1999, tagline:'Welcome to the Real World'}
  )._id;
  var Keanu = g[Person].save({_key: "Keanu", name:'Keanu Reeves', born:1964})._id;
  var Carrie = g[Person].save({_key: "Carrie", name:'Carrie-Anne Moss', born:1967})._id;
  var Laurence = g[Person].save({_key: "Laurence", name:'Laurence Fishburne', born:1961})._id;
  var Hugo = g[Person].save({_key: "Hugo", name:'Hugo Weaving', born:1960})._id;
  var Emil = g[Person].save({_key: "Emil", name:"Emil Eifrem", born: 1978})._id;
  var AndyW = g[Person].save({_key: "AndyW", name:'Andy Wachowski', born:1967})._id;
  var LanaW = g[Person].save({_key: "LanaW", name:'Lana Wachowski', born:1965})._id;
  var JoelS = g[Person].save({_key: "JoelS", name:'Joel Silver', born:1952})._id;

  g[ACTED_IN].save(Keanu, TheMatrix, {roles: ["Neo"]});
  g[ACTED_IN].save(Carrie, TheMatrix, {roles: ["Trinity"]});
  g[ACTED_IN].save(Laurence, TheMatrix, {roles: ["Morpheus"]});
  g[ACTED_IN].save(Hugo, TheMatrix, {roles: ["Agent Smith"]});
  g[ACTED_IN].save(Emil, TheMatrix, {roles: ["Emil"]}); 
  g[DIRECTED].save(AndyW, TheMatrix, {});
  g[DIRECTED].save(LanaW, TheMatrix, {});
  g[PRODUCED].save(JoelS, TheMatrix, {});

  var TheMatrixReloaded = g[Movie].save(
    {_key: "TheMatrixReloaded", title: "The Matrix Reloaded", released: 2003, tagline: "Free your mind"}
  )._id;
  g[ACTED_IN].save(Keanu, TheMatrixReloaded, {roles: ["Neo"]});
  g[ACTED_IN].save(Carrie, TheMatrixReloaded, {roles: ["Trinity"]});
  g[ACTED_IN].save(Laurence, TheMatrixReloaded, {roles: ["Morpheus"]});
  g[ACTED_IN].save(Hugo, TheMatrixReloaded, {roles: ["Agent Smith"]});
  g[DIRECTED].save(AndyW, TheMatrixReloaded, {});
  g[DIRECTED].save(LanaW, TheMatrixReloaded, {});
  g[PRODUCED].save(JoelS, TheMatrixReloaded, {});

  var TheMatrixRevolutions = g[Movie].save(
    {_key: "TheMatrixRevolutions", title: "The Matrix Revolutions",
      released: 2003, tagline: "Everything that has a beginning has an end"}
  )._id;
  g[ACTED_IN].save(Keanu, TheMatrixRevolutions, {roles: ["Neo"]});
  g[ACTED_IN].save(Carrie, TheMatrixRevolutions, {roles: ["Trinity"]});
  g[ACTED_IN].save(Laurence, TheMatrixRevolutions, {roles: ["Morpheus"]});
  g[ACTED_IN].save(Hugo, TheMatrixRevolutions, {roles: ["Agent Smith"]});
  g[DIRECTED].save(AndyW, TheMatrixRevolutions, {});
  g[DIRECTED].save(LanaW, TheMatrixRevolutions, {});
  g[PRODUCED].save(JoelS, TheMatrixRevolutions, {});


  var TheDevilsAdvocate = g[Movie].save(
    {_key: "TheDevilsAdvocate", title:"The Devil's Advocate", released:1997, tagline:'Evil has its winning ways'}
  )._id;
  var Charlize = g[Person].save({_key: "Charlize", name:'Charlize Theron', born:1975})._id;
  var Al = g[Person].save({_key: "Al", name:'Al Pacino', born:1940})._id;
  var Taylor = g[Person].save({_key: "Taylor", name:'Taylor Hackford', born:1944})._id;
  g[ACTED_IN].save(Keanu, TheDevilsAdvocate, {roles: ["Kevin Lomax"]});
  g[ACTED_IN].save(Charlize, TheDevilsAdvocate, {roles: ["Mary Ann Lomax"]});
  g[ACTED_IN].save(Al, TheDevilsAdvocate, {roles: ["John Milton"]});
  g[DIRECTED].save(Taylor, TheDevilsAdvocate, {});

  var AFewGoodMen = g[Movie].save(
    {_key: "AFewGoodMen", title:"A Few Good Men", released:1992,
      tagline:"In the heart of the nation's capital, in a courthouse of the U.S. government, "
      + "one man will stop at nothing to keep his honor, and one will stop at nothing to find the truth."}
  )._id;
  var TomC = g[Person].save({_key: "TomC", name:'Tom Cruise', born:1962})._id;
  var JackN = g[Person].save({_key: "JackN", name:'Jack Nicholson', born:1937})._id;
  var DemiM = g[Person].save({_key: "DemiM", name:'Demi Moore', born:1962})._id;
  var KevinB = g[Person].save({_key:"KevinB", name:'Kevin Bacon', born:1958})._id;
  var KieferS = g[Person].save({_key:"KieferS", name:'Kiefer Sutherland', born:1966})._id;
  var NoahW = g[Person].save({_key:"NoahW", name:'Noah Wyle', born:1971})._id;
  var CubaG = g[Person].save({_key:"CubaG", name:'Cuba Gooding Jr.', born:1968})._id;
  var KevinP = g[Person].save({_key:"KevinP", name:'Kevin Pollak', born:1957})._id;
  var JTW = g[Person].save({_key:"JTW", name:'J.T. Walsh', born:1943})._id;
  var JamesM = g[Person].save({_key:"JamesM", name:'James Marshall', born:1967})._id;
  var ChristopherG = g[Person].save({_key:"ChristopherG", name:'Christopher Guest', born:1948})._id;
  var RobR = g[Person].save({_key:"RobR", name:'Rob Reiner', born:1947})._id;
  var AaronS = g[Person].save({_key:"AaronS", name:'Aaron Sorkin', born:1961})._id;
  g[ACTED_IN].save(TomC,AFewGoodMen,{roles:['Lt. Daniel Kaffee']});
  g[ACTED_IN].save(JackN,AFewGoodMen,{roles:['Col. Nathan R. Jessup']});
  g[ACTED_IN].save(DemiM,AFewGoodMen,{roles:['Lt. Cdr. JoAnne Galloway']});
  g[ACTED_IN].save(KevinB,AFewGoodMen,{roles:['Capt. Jack Ross']});
  g[ACTED_IN].save(KieferS,AFewGoodMen,{roles:['Lt. Jonathan Kendrick']});
  g[ACTED_IN].save(NoahW,AFewGoodMen,{roles:['Cpl. Jeffrey Barnes']});
  g[ACTED_IN].save(CubaG,AFewGoodMen,{roles:['Cpl. Carl Hammaker']});
  g[ACTED_IN].save(KevinP,AFewGoodMen,{roles:['Lt. Sam Weinberg']});
  g[ACTED_IN].save(JTW,AFewGoodMen,{roles:['Lt. Col. Matthew Andrew Markinson']});
  g[ACTED_IN].save(JamesM,AFewGoodMen,{roles:['Pfc. Louden Downey']});
  g[ACTED_IN].save(ChristopherG,AFewGoodMen,{roles:['Dr. Stone']});
  g[ACTED_IN].save(AaronS,AFewGoodMen,{roles:['Man in Bar']});
  g[DIRECTED].save(RobR, AFewGoodMen, {});
  g[WROTE].save(AaronS, AFewGoodMen, {});

  var TopGun = g[Movie].save(
    {_key:"TopGun", title:"Top Gun", released:1986, tagline:'I feel the need, the need for speed.'}
  )._id;
  var KellyM = g[Person].save({_key:"KellyM", name:'Kelly McGillis', born:1957})._id;
  var ValK = g[Person].save({_key:"ValK", name:'Val Kilmer', born:1959})._id;
  var AnthonyE = g[Person].save({_key:"AnthonyE", name:'Anthony Edwards', born:1962})._id;
  var TomS = g[Person].save({_key:"TomS", name:'Tom Skerritt', born:1933})._id;
  var MegR = g[Person].save({_key:"MegR", name:'Meg Ryan', born:1961})._id;
  var TonyS = g[Person].save({_key:"TonyS", name:'Tony Scott', born:1944})._id;
  var JimC = g[Person].save({_key:"JimC", name:'Jim Cash', born:1941})._id;
  g[ACTED_IN].save(TomC,TopGun,{roles:['Maverick']});
  g[ACTED_IN].save(KellyM,TopGun,{roles:['Charlie']});
  g[ACTED_IN].save(ValK,TopGun,{roles:['Iceman']});
  g[ACTED_IN].save(AnthonyE,TopGun,{roles:['Goose']});
  g[ACTED_IN].save(TomS,TopGun,{roles:['Viper']});
  g[ACTED_IN].save(MegR,TopGun,{roles:['Carole']});
  g[DIRECTED].save(TonyS, TopGun, {});
  g[WROTE].save(JimC, TopGun, {});

  var JerryMaguire = g[Movie].save(
    {_key:"JerryMaguire", title:'Jerry Maguire', released:2000, tagline:'The rest of his life begins now.'}
  )._id;
  var ReneeZ = g[Person].save({_key:"ReneeZ", name:'Renee Zellweger', born:1969})._id;
  var KellyP = g[Person].save({_key:"KellyP", name:'Kelly Preston', born:1962})._id;
  var JerryO = g[Person].save({_key:"JerryO", name:"Jerry O'Connell", born:1974})._id;
  var JayM = g[Person].save({_key:"JayM", name:'Jay Mohr', born:1970})._id;
  var BonnieH = g[Person].save({_key:"BonnieH", name:'Bonnie Hunt', born:1961})._id;
  var ReginaK = g[Person].save({_key:"ReginaK", name:'Regina King', born:1971})._id;
  var JonathanL = g[Person].save({_key:"JonathanL", name:'Jonathan Lipnicki', born:1996})._id;
  var CameronC = g[Person].save({_key:"CameronC", name:'Cameron Crowe', born:1957})._id;
  g[ACTED_IN].save(TomC,JerryMaguire,{roles:['Jerry Maguire']});
  g[ACTED_IN].save(CubaG,JerryMaguire,{roles:['Rod Tidwell']});
  g[ACTED_IN].save(ReneeZ,JerryMaguire,{roles:['Dorothy Boyd']});
  g[ACTED_IN].save(KellyP,JerryMaguire,{roles:['Avery Bishop']});
  g[ACTED_IN].save(JerryO,JerryMaguire,{roles:['Frank Cushman']});
  g[ACTED_IN].save(JayM,JerryMaguire,{roles:['Bob Sugar']});
  g[ACTED_IN].save(BonnieH,JerryMaguire,{roles:['Laurel Boyd']});
  g[ACTED_IN].save(ReginaK,JerryMaguire,{roles:['Marcee Tidwell']});
  g[ACTED_IN].save(JonathanL,JerryMaguire,{roles:['Ray Boyd']});
  g[DIRECTED].save(CameronC,JerryMaguire,{});
  g[PRODUCED].save(CameronC,JerryMaguire,{});
  g[WROTE].save(CameronC,JerryMaguire,{});

  var StandByMe = g[Movie].save(
    {_key:"StandByMe", title:"Stand By Me", released:1986,
      tagline:"For some, it's the last real taste of innocence, and the first real taste of life. "
      + "But for everyone, it's the time that memories are made of."}
  )._id;
  var RiverP = g[Person].save({_key:"RiverP", name:'River Phoenix', born:1970})._id;
  var CoreyF = g[Person].save({_key:"CoreyF", name:'Corey Feldman', born:1971})._id;
  var WilW = g[Person].save({_key:"WilW", name:'Wil Wheaton', born:1972})._id;
  var JohnC = g[Person].save({_key:"JohnC", name:'John Cusack', born:1966})._id;
  var MarshallB = g[Person].save({_key:"MarshallB", name:'Marshall Bell', born:1942})._id;
  g[ACTED_IN].save(WilW,StandByMe,{roles:['Gordie Lachance']});
  g[ACTED_IN].save(RiverP,StandByMe,{roles:['Chris Chambers']});
  g[ACTED_IN].save(JerryO,StandByMe,{roles:['Vern Tessio']});
  g[ACTED_IN].save(CoreyF,StandByMe,{roles:['Teddy Duchamp']});
  g[ACTED_IN].save(JohnC,StandByMe,{roles:['Denny Lachance']});
  g[ACTED_IN].save(KieferS,StandByMe,{roles:['Ace Merrill']});
  g[ACTED_IN].save(MarshallB,StandByMe,{roles:['Mr. Lachance']});
  g[DIRECTED].save(RobR,StandByMe,{});

  var AsGoodAsItGets = g[Movie].save(
    {_key:"AsGoodAsItGets", title:'As Good as It Gets', released:1997,
      tagline:'A comedy from the heart that goes for the throat.'}
  )._id;
  var HelenH = g[Person].save({_key:"HelenH", name:'Helen Hunt', born:1963})._id;
  var GregK = g[Person].save({_key:"GregK", name:'Greg Kinnear', born:1963})._id;
  var JamesB = g[Person].save({_key:"JamesB", name:'James L. Brooks', born:1940})._id;
  g[ACTED_IN].save(JackN,AsGoodAsItGets,{roles:['Melvin Udall']});
  g[ACTED_IN].save(HelenH,AsGoodAsItGets,{roles:['Carol Connelly']});
  g[ACTED_IN].save(GregK,AsGoodAsItGets,{roles:['Simon Bishop']});
  g[ACTED_IN].save(CubaG,AsGoodAsItGets,{roles:['Frank Sachs']});
  g[DIRECTED].save(JamesB,AsGoodAsItGets,{});

  var WhatDreamsMayCome = g[Movie].save(
    {_key:"WhatDreamsMayCome", title:'What Dreams May Come',
      released:1998, tagline:'After life there is more. The end is just the beginning.'}
  )._id;
  var AnnabellaS = g[Person].save({_key:"AnnabellaS", name:'Annabella Sciorra', born:1960})._id;
  var MaxS = g[Person].save({_key:"MaxS", name:'Max von Sydow', born:1929})._id;
  var WernerH = g[Person].save({_key:"WernerH", name:'Werner Herzog', born:1942})._id;
  var Robin = g[Person].save({_key:"Robin", name:'Robin Williams', born:1951})._id;
  var VincentW = g[Person].save({_key:"VincentW", name:'Vincent Ward', born:1956})._id;
  g[ACTED_IN].save(Robin,WhatDreamsMayCome,{roles:['Chris Nielsen']});
  g[ACTED_IN].save(CubaG,WhatDreamsMayCome,{roles:['Albert Lewis']});
  g[ACTED_IN].save(AnnabellaS,WhatDreamsMayCome,{roles:['Annie Collins-Nielsen']});
  g[ACTED_IN].save(MaxS,WhatDreamsMayCome,{roles:['The Tracker']});
  g[ACTED_IN].save(WernerH,WhatDreamsMayCome,{roles:['The Face']});
  g[DIRECTED].save(VincentW,WhatDreamsMayCome,{});

  var SnowFallingonCedars = g[Movie].save(
    {_key:"SnowFallingonCedars", title:'Snow Falling on Cedars', released:1999, tagline:'First loves last. Forever.'}
  )._id;
  var EthanH = g[Person].save({_key:"EthanH", name:'Ethan Hawke', born:1970})._id;
  var RickY = g[Person].save({_key:"RickY", name:'Rick Yune', born:1971})._id;
  var JamesC = g[Person].save({_key:"JamesC", name:'James Cromwell', born:1940})._id;
  var ScottH = g[Person].save({_key:"ScottH", name:'Scott Hicks', born:1953})._id;
  g[ACTED_IN].save(EthanH,SnowFallingonCedars,{roles:['Ishmael Chambers']});
  g[ACTED_IN].save(RickY,SnowFallingonCedars,{roles:['Kazuo Miyamoto']});
  g[ACTED_IN].save(MaxS,SnowFallingonCedars,{roles:['Nels Gudmundsson']});
  g[ACTED_IN].save(JamesC,SnowFallingonCedars,{roles:['Judge Fielding']});
  g[DIRECTED].save(ScottH,SnowFallingonCedars,{});

  var YouveGotMail = g[Movie].save(
    {_key:"YouveGotMail", title:"You've Got Mail", released:1998, tagline:'At odds in life... in love on-line.'}
  )._id;
  var ParkerP = g[Person].save({_key:"ParkerP", name:'Parker Posey', born:1968})._id;
  var DaveC = g[Person].save({_key:"DaveC", name:'Dave Chappelle', born:1973})._id;
  var SteveZ = g[Person].save({_key:"SteveZ", name:'Steve Zahn', born:1967})._id;
  var TomH = g[Person].save({_key:"TomH", name:'Tom Hanks', born:1956})._id;
  var NoraE = g[Person].save({_key:"NoraE", name:'Nora Ephron', born:1941})._id;
  g[ACTED_IN].save(TomH,YouveGotMail,{roles:['Joe Fox']});
  g[ACTED_IN].save(MegR,YouveGotMail,{roles:['Kathleen Kelly']});
  g[ACTED_IN].save(GregK,YouveGotMail,{roles:['Frank Navasky']});
  g[ACTED_IN].save(ParkerP,YouveGotMail,{roles:['Patricia Eden']});
  g[ACTED_IN].save(DaveC,YouveGotMail,{roles:['Kevin Jackson']});
  g[ACTED_IN].save(SteveZ,YouveGotMail,{roles:['George Pappas']});
  g[DIRECTED].save(NoraE,YouveGotMail,{});

  var SleeplessInSeattle = g[Movie].save(
    {_key:"SleeplessInSeattle", title:'Sleepless in Seattle', released:1993,
      tagline:'What if someone you never met, someone you never saw, '
      + 'someone you never knew was the only someone for you?'}
  )._id;
  var RitaW = g[Person].save({_key:"RitaW", name:'Rita Wilson', born:1956})._id;
  var BillPull = g[Person].save({_key:"BillPull", name:'Bill Pullman', born:1953})._id;
  var VictorG = g[Person].save({_key:"VictorG", name:'Victor Garber', born:1949})._id;
  var RosieO = g[Person].save({_key:"RosieO", name:"Rosie O'Donnell", born:1962})._id;
  g[ACTED_IN].save(TomH,SleeplessInSeattle,{roles:['Sam Baldwin']});
  g[ACTED_IN].save(MegR,SleeplessInSeattle,{roles:['Annie Reed']});
  g[ACTED_IN].save(RitaW,SleeplessInSeattle,{roles:['Suzy']});
  g[ACTED_IN].save(BillPull,SleeplessInSeattle,{roles:['Walter']});
  g[ACTED_IN].save(VictorG,SleeplessInSeattle,{roles:['Greg']});
  g[ACTED_IN].save(RosieO,SleeplessInSeattle,{roles:['Becky']});
  g[DIRECTED].save(NoraE,SleeplessInSeattle,{});

  var JoeVersustheVolcano = g[Movie].save(
    {_key:"JoeVersustheVolcano", title:'Joe Versus the Volcano',
      released:1990, tagline:'A story of love, lava and burning desire.'}
  )._id;
  var JohnS = g[Person].save({_key:"JohnS", name:'John Patrick Stanley', born:1950})._id;
  var Nathan = g[Person].save({_key:"Nathan", name:'Nathan Lane', born:1956})._id;
  g[ACTED_IN].save(TomH,JoeVersustheVolcano,{roles:['Joe Banks']});
  g[ACTED_IN].save(MegR,JoeVersustheVolcano,{roles:['DeDe', 'Angelica Graynamore', 'Patricia Graynamore']});
  g[ACTED_IN].save(Nathan,JoeVersustheVolcano,{roles:['Baw']});
  g[DIRECTED].save(JohnS,JoeVersustheVolcano,{});

  var WhenHarryMetSally = g[Movie].save(
    {_key:"WhenHarryMetSally", title:'When Harry Met Sally',
      released:1998, tagline:'At odds in life... in love on-line.'}
  )._id;
  var BillyC = g[Person].save({_key:"BillyC", name:'Billy Crystal', born:1948})._id;
  var CarrieF = g[Person].save({_key:"CarrieF", name:'Carrie Fisher', born:1956})._id;
  var BrunoK = g[Person].save({_key:"BrunoK", name:'Bruno Kirby', born:1949})._id;
  g[ACTED_IN].save(BillyC,WhenHarryMetSally,{roles:['Harry Burns']});
  g[ACTED_IN].save(MegR,WhenHarryMetSally,{roles:['Sally Albright']});
  g[ACTED_IN].save(CarrieF,WhenHarryMetSally,{roles:['Marie']});
  g[ACTED_IN].save(BrunoK,WhenHarryMetSally,{roles:['Jess']});
  g[DIRECTED].save(RobR,WhenHarryMetSally,{});
  g[PRODUCED].save(RobR,WhenHarryMetSally,{});
  g[PRODUCED].save(NoraE,WhenHarryMetSally,{});
  g[WROTE].save(NoraE,WhenHarryMetSally,{});

  var ThatThingYouDo = g[Movie].save(
    {_key:"ThatThingYouDo", title:'That Thing You Do', released:1996,
      tagline:'In every life there comes a time when that thing you dream becomes that thing you do'}
  )._id;
  var LivT = g[Person].save({_key:"LivT", name:'Liv Tyler', born:1977})._id;
  g[ACTED_IN].save(TomH,ThatThingYouDo,{roles:['Mr. White']});
  g[ACTED_IN].save(LivT,ThatThingYouDo,{roles:['Faye Dolan']});
  g[ACTED_IN].save(Charlize,ThatThingYouDo,{roles:['Tina']});
  g[DIRECTED].save(TomH,ThatThingYouDo,{});

  var TheReplacements = g[Movie].save(
    {_key:"TheReplacements", title:'The Replacements', released:2000,
      tagline:'Pain heals, Chicks dig scars... Glory lasts forever'}
  )._id;
  var Brooke = g[Person].save({_key:"Brooke", name:'Brooke Langton', born:1970})._id;
  var Gene = g[Person].save({_key:"Gene", name:'Gene Hackman', born:1930})._id;
  var Orlando = g[Person].save({_key:"Orlando", name:'Orlando Jones', born:1968})._id;
  var Howard = g[Person].save({_key:"Howard", name:'Howard Deutch', born:1950})._id;
  g[ACTED_IN].save(Keanu,TheReplacements,{roles:['Shane Falco']});
  g[ACTED_IN].save(Brooke,TheReplacements,{roles:['Annabelle Farrell']});
  g[ACTED_IN].save(Gene,TheReplacements,{roles:['Jimmy McGinty']});
  g[ACTED_IN].save(Orlando,TheReplacements,{roles:['Clifford Franklin']});
  g[DIRECTED].save(Howard,TheReplacements,{});

  var RescueDawn = g[Movie].save(
    {_key:"RescueDawn", title:'RescueDawn', released:2006,
      tagline:"Based on the extraordinary true story of one man's fight for freedom"}
  )._id;
  var ChristianB = g[Person].save({_key:"ChristianB", name:'Christian Bale', born:1974})._id;
  var ZachG = g[Person].save({_key:"ZachG", name:'Zach Grenier', born:1954})._id;
  g[ACTED_IN].save(MarshallB,RescueDawn,{roles:['Admiral']});
  g[ACTED_IN].save(ChristianB,RescueDawn,{roles:['Dieter Dengler']});
  g[ACTED_IN].save(ZachG,RescueDawn,{roles:['Squad Leader']});
  g[ACTED_IN].save(SteveZ,RescueDawn,{roles:['Duane']});
  g[DIRECTED].save(WernerH,RescueDawn,{});

  var TheBirdcage = g[Movie].save(
    {_key:"TheBirdcage", title:'The Birdcage', released:1996, tagline:'Come as you are'}
  )._id;
  var MikeN = g[Person].save({_key:"MikeN", name:'Mike Nichols', born:1931})._id;
  g[ACTED_IN].save(Robin,TheBirdcage,{roles:['Armand Goldman']});
  g[ACTED_IN].save(Nathan,TheBirdcage,{roles:['Albert Goldman']});
  g[ACTED_IN].save(Gene,TheBirdcage,{roles:['Sen. Kevin Keeley']});
  g[DIRECTED].save(MikeN,TheBirdcage,{});

  var Unforgiven = g[Movie].save(
    {_key:"Unforgiven", title:'Unforgiven', released:1992, tagline:"It's a hell of a thing, killing a man"}
  )._id;
  var RichardH = g[Person].save({_key:"RichardH", name:'Richard Harris', born:1930})._id;
  var ClintE = g[Person].save({_key:"ClintE", name:'Clint Eastwood', born:1930})._id;
  g[ACTED_IN].save(RichardH,Unforgiven,{roles:['English Bob']});
  g[ACTED_IN].save(ClintE,Unforgiven,{roles:['Bill Munny']});
  g[ACTED_IN].save(Gene,Unforgiven,{roles:['Little Bill Daggett']});
  g[DIRECTED].save(ClintE,Unforgiven,{});

  var JohnnyMnemonic = g[Movie].save(
    {_key:"JohnnyMnemonic", title:'Johnny Mnemonic', released:1995,
      tagline:'The hottest data on earth. In the coolest head in town'}
  )._id;
  var Takeshi = g[Person].save({_key:"Takeshi", name:'Takeshi Kitano', born:1947})._id;
  var Dina = g[Person].save({_key:"Dina", name:'Dina Meyer', born:1968})._id;
  var IceT = g[Person].save({_key:"IceT", name:'Ice-T', born:1958})._id;
  var RobertL = g[Person].save({_key:"RobertL", name:'Robert Longo', born:1953})._id;
  g[ACTED_IN].save(Keanu,JohnnyMnemonic,{roles:['Johnny Mnemonic']});
  g[ACTED_IN].save(Takeshi,JohnnyMnemonic,{roles:['Takahashi']});
  g[ACTED_IN].save(Dina,JohnnyMnemonic,{roles:['Jane']});
  g[ACTED_IN].save(IceT,JohnnyMnemonic,{roles:['J-Bone']});
  g[DIRECTED].save(RobertL,JohnnyMnemonic,{});

  var CloudAtlas = g[Movie].save(
    {_key:"CloudAtlas", title:'Cloud Atlas', released:2012, tagline:'Everything is connected'}
  )._id;
  var HalleB = g[Person].save({_key:"HalleB", name:'Halle Berry', born:1966})._id;
  var JimB = g[Person].save({_key:"JimB", name:'Jim Broadbent', born:1949})._id;
  var TomT = g[Person].save({_key:"TomT", name:'Tom Tykwer', born:1965})._id;
  var DavidMitchell = g[Person].save({_key:"DavidMitchell", name:'David Mitchell', born:1969})._id;
  var StefanArndt = g[Person].save({_key:"StefanArndt", name:'Stefan Arndt', born:1961})._id;
  g[ACTED_IN].save(TomH,CloudAtlas,{roles:['Zachry', 'Dr. Henry Goose', 'Isaac Sachs', 'Dermot Hoggins']});
  g[ACTED_IN].save(Hugo,CloudAtlas,
    {roles:['Bill Smoke', 'Haskell Moore', 'Tadeusz Kesselring', 'Nurse Noakes', 'Boardman Mephi', 'Old Georgie']}
  );
  g[ACTED_IN].save(HalleB,CloudAtlas,{roles:['Luisa Rey', 'Jocasta Ayrs', 'Ovid', 'Meronym']});
  g[ACTED_IN].save(JimB,CloudAtlas,{roles:['Vyvyan Ayrs', 'Captain Molyneux', 'Timothy Cavendish']});
  g[DIRECTED].save(TomT,CloudAtlas,{});
  g[DIRECTED].save(AndyW,CloudAtlas,{});
  g[DIRECTED].save(LanaW,CloudAtlas,{});
  g[WROTE].save(DavidMitchell,CloudAtlas,{});
  g[PRODUCED].save(StefanArndt,CloudAtlas,{});

  var TheDaVinciCode = g[Movie].save(
    {_key:"TheDaVinciCode", title:'The Da Vinci Code', released:2006, tagline:'Break The Codes'}
  )._id;
  var IanM = g[Person].save({_key:"IanM", name:'Ian McKellen', born:1939})._id;
  var AudreyT = g[Person].save({_key:"AudreyT", name:'Audrey Tautou', born:1976})._id;
  var PaulB = g[Person].save({_key:"PaulB", name:'Paul Bettany', born:1971})._id;
  var RonH = g[Person].save({_key:"RonH", name:'Ron Howard', born:1954})._id;
  g[ACTED_IN].save(TomH,TheDaVinciCode,{roles:['Dr. Robert Langdon']});
  g[ACTED_IN].save(IanM,TheDaVinciCode,{roles:['Sir Leight Teabing']});
  g[ACTED_IN].save(AudreyT,TheDaVinciCode,{roles:['Sophie Neveu']});
  g[ACTED_IN].save(PaulB,TheDaVinciCode,{roles:['Silas']});
  g[DIRECTED].save(RonH,TheDaVinciCode,{});

  var VforVendetta = g[Movie].save(
    {_key:"VforVendetta", title:'V for Vendetta', released:2006, tagline:'Freedom! Forever!'}
  )._id;
  var NatalieP = g[Person].save({_key:"NatalieP", name:'Natalie Portman', born:1981})._id;
  var StephenR = g[Person].save({_key:"StephenR", name:'Stephen Rea', born:1946})._id;
  var JohnH = g[Person].save({_key:"JohnH", name:'John Hurt', born:1940})._id;
  var BenM = g[Person].save({_key:"BenM", name: 'Ben Miles', born:1967})._id;
  g[ACTED_IN].save(Hugo,VforVendetta,{roles:['V']});
  g[ACTED_IN].save(NatalieP,VforVendetta,{roles:['Evey Hammond']});
  g[ACTED_IN].save(StephenR,VforVendetta,{roles:['Eric Finch']});
  g[ACTED_IN].save(JohnH,VforVendetta,{roles:['High Chancellor Adam Sutler']});
  g[ACTED_IN].save(BenM,VforVendetta,{roles:['Dascomb']});
  g[DIRECTED].save(JamesM,VforVendetta,{});
  g[PRODUCED].save(AndyW,VforVendetta,{});
  g[PRODUCED].save(LanaW,VforVendetta,{});
  g[PRODUCED].save(JoelS,VforVendetta,{});
  g[WROTE].save(AndyW,VforVendetta,{});
  g[WROTE].save(LanaW,VforVendetta,{});

  var SpeedRacer = g[Movie].save(
    {_key:"SpeedRacer", title:'Speed Racer', released:2008, tagline:'Speed has no limits'}
  )._id;
  var EmileH = g[Person].save({_key:"EmileH", name:'Emile Hirsch', born:1985})._id;
  var JohnG = g[Person].save({_key:"JohnG", name:'John Goodman', born:1960})._id;
  var SusanS = g[Person].save({_key:"SusanS", name:'Susan Sarandon', born:1946})._id;
  var MatthewF = g[Person].save({_key:"MatthewF", name:'Matthew Fox', born:1966})._id;
  var ChristinaR = g[Person].save({_key:"ChristinaR", name:'Christina Ricci', born:1980})._id;
  var Rain = g[Person].save({_key:"Rain", name:'Rain', born:1982})._id;
  g[ACTED_IN].save(EmileH,SpeedRacer,{roles:['Speed Racer']});
  g[ACTED_IN].save(JohnG,SpeedRacer,{roles:['Pops']});
  g[ACTED_IN].save(SusanS,SpeedRacer,{roles:['Mom']});
  g[ACTED_IN].save(MatthewF,SpeedRacer,{roles:['Racer X']});
  g[ACTED_IN].save(ChristinaR,SpeedRacer,{roles:['Trixie']});
  g[ACTED_IN].save(Rain,SpeedRacer,{roles:['Taejo Togokahn']});
  g[ACTED_IN].save(BenM,SpeedRacer,{roles:['Cass Jones']});
  g[DIRECTED].save(AndyW,SpeedRacer,{});
  g[DIRECTED].save(LanaW,SpeedRacer,{});
  g[WROTE].save(AndyW,SpeedRacer,{});
  g[WROTE].save(LanaW,SpeedRacer,{});
  g[PRODUCED].save(JoelS,SpeedRacer,{});

  var NinjaAssassin = g[Movie].save(
    {_key:"NinjaAssassin", title:'Ninja Assassin', released:2009,
      tagline:'Prepare to enter a secret world of assassins'}
  )._id;
  var NaomieH = g[Person].save({_key:"NaomieH", name:'Naomie Harris'})._id;
  g[ACTED_IN].save(Rain,NinjaAssassin,{roles:['Raizo']});
  g[ACTED_IN].save(NaomieH,NinjaAssassin,{roles:['Mika Coretti']});
  g[ACTED_IN].save(RickY,NinjaAssassin,{roles:['Takeshi']});
  g[ACTED_IN].save(BenM,NinjaAssassin,{roles:['Ryan Maslow']});
  g[DIRECTED].save(JamesM,NinjaAssassin,{});
  g[PRODUCED].save(AndyW,NinjaAssassin,{});
  g[PRODUCED].save(LanaW,NinjaAssassin,{});
  g[PRODUCED].save(JoelS,NinjaAssassin,{});

  var TheGreenMile = g[Movie].save(
    {_key:"TheGreenMile", title:'The Green Mile', released:1999, tagline:"Walk a mile you'll never forget."}
  )._id;
  var MichaelD = g[Person].save({_key:"MichaelD", name:'Michael Clarke Duncan', born:1957})._id;
  var DavidM = g[Person].save({_key:"DavidM", name:'David Morse', born:1953})._id;
  var SamR = g[Person].save({_key:"SamR", name:'Sam Rockwell', born:1968})._id;
  var GaryS = g[Person].save({_key:"GaryS", name:'Gary Sinise', born:1955})._id;
  var PatriciaC = g[Person].save({_key:"PatriciaC", name:'Patricia Clarkson', born:1959})._id;
  var FrankD = g[Person].save({_key:"FrankD", name:'Frank Darabont', born:1959})._id;
  g[ACTED_IN].save(TomH,TheGreenMile,{roles:['Paul Edgecomb']});
  g[ACTED_IN].save(MichaelD,TheGreenMile,{roles:['John Coffey']});
  g[ACTED_IN].save(DavidM,TheGreenMile,{roles:['Brutus "Brutal" Howell']});
  g[ACTED_IN].save(BonnieH,TheGreenMile,{roles:['Jan Edgecomb']});
  g[ACTED_IN].save(JamesC,TheGreenMile,{roles:['Warden Hal Moores']});
  g[ACTED_IN].save(SamR,TheGreenMile,{roles:['"Wild Bill" Wharton']});
  g[ACTED_IN].save(GaryS,TheGreenMile,{roles:['Burt Hammersmith']});
  g[ACTED_IN].save(PatriciaC,TheGreenMile,{roles:['Melinda Moores']});
  g[DIRECTED].save(FrankD,TheGreenMile,{});

  var FrostNixon = g[Movie].save(
    {_key:"FrostNixon", title:'Frost/Nixon', released:2008, tagline:'400 million people were waiting for the truth.'}
  )._id;
  var FrankL = g[Person].save({_key:"FrankL", name:'Frank Langella', born:1938})._id;
  var MichaelS = g[Person].save({_key:"MichaelS", name:'Michael Sheen', born:1969})._id;
  var OliverP = g[Person].save({_key:"OliverP", name:'Oliver Platt', born:1960})._id;
  g[ACTED_IN].save(FrankL,FrostNixon,{roles:['Richard Nixon']});
  g[ACTED_IN].save(MichaelS,FrostNixon,{roles:['David Frost']});
  g[ACTED_IN].save(KevinB,FrostNixon,{roles:['Jack Brennan']});
  g[ACTED_IN].save(OliverP,FrostNixon,{roles:['Bob Zelnick']});
  g[ACTED_IN].save(SamR,FrostNixon,{roles:['James Reston, Jr.']});
  g[DIRECTED].save(RonH,FrostNixon,{});

  var Hoffa = g[Movie].save(
    {_key:"Hoffa", title:'Hoffa', released:1992, tagline:"He didn't want law. He wanted justice."}
  )._id;
  var DannyD = g[Person].save({_key:"DannyD", name:'Danny DeVito', born:1944})._id;
  var JohnR = g[Person].save({_key:"JohnR", name:'John C. Reilly', born:1965})._id;
  g[ACTED_IN].save(JackN,Hoffa,{roles:['Hoffa']});
  g[ACTED_IN].save(DannyD,Hoffa,{roles:['Robert "Bobby" Ciaro']});
  g[ACTED_IN].save(JTW,Hoffa,{roles:['Frank Fitzsimmons']});
  g[ACTED_IN].save(JohnR,Hoffa,{roles:['Peter "Pete" Connelly']});
  g[DIRECTED].save(DannyD,Hoffa,{});

  var Apollo13 = g[Movie].save(
    {_key:"Apollo13", title:'Apollo 13', released:1995, tagline:'Houston, we have a problem.'}
  )._id;
  var EdH = g[Person].save({_key:"EdH", name:'Ed Harris', born:1950})._id;
  var BillPax = g[Person].save({_key:"BillPax", name:'Bill Paxton', born:1955})._id;
  g[ACTED_IN].save(TomH,Apollo13,{roles:['Jim Lovell']});
  g[ACTED_IN].save(KevinB,Apollo13,{roles:['Jack Swigert']});
  g[ACTED_IN].save(EdH,Apollo13,{roles:['Gene Kranz']});
  g[ACTED_IN].save(BillPax,Apollo13,{roles:['Fred Haise']});
  g[ACTED_IN].save(GaryS,Apollo13,{roles:['Ken Mattingly']});
  g[DIRECTED].save(RonH,Apollo13,{});

  var Twister = g[Movie].save(
    {_key:"Twister", title:'Twister', released:1996, tagline:"Don't Breathe. Don't Look Back."}
  )._id;
  var PhilipH = g[Person].save({_key:"PhilipH", name:'Philip Seymour Hoffman', born:1967})._id;
  var JanB = g[Person].save({_key:"JanB", name:'Jan de Bont', born:1943})._id;
  g[ACTED_IN].save(BillPax,Twister,{roles:['Bill Harding']});
  g[ACTED_IN].save(HelenH,Twister,{roles:['Dr. Jo Harding']});
  g[ACTED_IN].save(ZachG,Twister,{roles:['Eddie']});
  g[ACTED_IN].save(PhilipH,Twister,{roles:['Dustin "Dusty" Davis']});
  g[DIRECTED].save(JanB,Twister,{});

  var CastAway = g[Movie].save(
    {_key:"CastAway", title:'Cast Away', released:2000, tagline:'At the edge of the world, his journey begins.'}
  )._id;
  var RobertZ = g[Person].save({_key:"RobertZ", name:'Robert Zemeckis', born:1951})._id;
  g[ACTED_IN].save(TomH,CastAway,{roles:['Chuck Noland']});
  g[ACTED_IN].save(HelenH,CastAway,{roles:['Kelly Frears']});
  g[DIRECTED].save(RobertZ,CastAway,{});

  var OneFlewOvertheCuckoosNest = g[Movie].save(
    {_key:"OneFlewOvertheCuckoosNest", title:"One Flew Over the Cuckoo's Nest",
      released:1975, tagline:"If he's crazy, what does that make you?"}
  )._id;
  var MilosF = g[Person].save({_key:"MilosF", name:'Milos Forman', born:1932})._id;
  g[ACTED_IN].save(JackN,OneFlewOvertheCuckoosNest,{roles:['Randle McMurphy']});
  g[ACTED_IN].save(DannyD,OneFlewOvertheCuckoosNest,{roles:['Martini']});
  g[DIRECTED].save(MilosF,OneFlewOvertheCuckoosNest,{});

  var SomethingsGottaGive = g[Movie].save(
    {_key:"SomethingsGottaGive", title:"Something's Gotta Give", released:2003}
  )._id;
  var DianeK = g[Person].save({_key:"DianeK", name:'Diane Keaton', born:1946})._id;
  var NancyM = g[Person].save({_key:"NancyM", name:'Nancy Meyers', born:1949})._id;
  g[ACTED_IN].save(JackN,SomethingsGottaGive,{roles:['Harry Sanborn']});
  g[ACTED_IN].save(DianeK,SomethingsGottaGive,{roles:['Erica Barry']});
  g[ACTED_IN].save(Keanu,SomethingsGottaGive,{roles:['Julian Mercer']});
  g[DIRECTED].save(NancyM,SomethingsGottaGive,{});
  g[PRODUCED].save(NancyM,SomethingsGottaGive,{});
  g[WROTE].save(NancyM,SomethingsGottaGive,{});

  var BicentennialMan = g[Movie].save(
    {_key:"BicentennialMan", title:'Bicentennial Man', released:1999,
      tagline:"One robot's 200 year journey to become an ordinary man."}
  )._id;
  var ChrisC = g[Person].save({_key:"ChrisC", name:'Chris Columbus', born:1958})._id;
  g[ACTED_IN].save(Robin,BicentennialMan,{roles:['Andrew Marin']});
  g[ACTED_IN].save(OliverP,BicentennialMan,{roles:['Rupert Burns']});
  g[DIRECTED].save(ChrisC,BicentennialMan,{});

  var CharlieWilsonsWar = g[Movie].save(
    {_key:"CharlieWilsonsWar", title:"Charlie Wilson's War", released:2007,
      tagline:"A stiff drink. A little mascara. A lot of nerve. Who said they couldn't bring down the Soviet empire."}
  )._id;
  var JuliaR = g[Person].save({_key:"JuliaR", name:'Julia Roberts', born:1967})._id;
  g[ACTED_IN].save(TomH,CharlieWilsonsWar,{roles:['Rep. Charlie Wilson']});
  g[ACTED_IN].save(JuliaR,CharlieWilsonsWar,{roles:['Joanne Herring']});
  g[ACTED_IN].save(PhilipH,CharlieWilsonsWar,{roles:['Gust Avrakotos']});
  g[DIRECTED].save(MikeN,CharlieWilsonsWar,{});

  var ThePolarExpress = g[Movie].save(
    {_key:"ThePolarExpress", title:'The Polar Express', released:2004, tagline:'This Holiday Season… Believe'}
  )._id;
  g[ACTED_IN].save(TomH,ThePolarExpress,
    {roles:['Hero Boy', 'Father', 'Conductor', 'Hobo', 'Scrooge', 'Santa Claus']}
  );
  g[DIRECTED].save(RobertZ,ThePolarExpress,{});

  var ALeagueofTheirOwn = g[Movie].save(
    {_key:"ALeagueofTheirOwn", title:'A League of Their Own', released:1992,
      tagline:'Once in a lifetime you get a chance to do something different.'}
  )._id;
  var Madonna = g[Person].save({_key:"Madonna", name:'Madonna', born:1954})._id;
  var GeenaD = g[Person].save({_key:"GeenaD", name:'Geena Davis', born:1956})._id;
  var LoriP = g[Person].save({_key:"LoriP", name:'Lori Petty', born:1963})._id;
  var PennyM = g[Person].save({_key:"PennyM", name:'Penny Marshall', born:1943})._id;
  g[ACTED_IN].save(TomH,ALeagueofTheirOwn,{roles:['Jimmy Dugan']});
  g[ACTED_IN].save(GeenaD,ALeagueofTheirOwn,{roles:['Dottie Hinson']});
  g[ACTED_IN].save(LoriP,ALeagueofTheirOwn,{roles:['Kit Keller']});
  g[ACTED_IN].save(RosieO,ALeagueofTheirOwn,{roles:['Doris Murphy']});
  g[ACTED_IN].save(Madonna,ALeagueofTheirOwn,{roles:['"All the Way" Mae Mordabito']});
  g[ACTED_IN].save(BillPax,ALeagueofTheirOwn,{roles:['Bob Hinson']});
  g[DIRECTED].save(PennyM,ALeagueofTheirOwn,{});

  var PaulBlythe = g[Person].save({_key:"PaulBlythe", name:'Paul Blythe'})._id;
  var AngelaScope = g[Person].save({_key:"AngelaScope", name:'Angela Scope'})._id;
  var JessicaThompson = g[Person].save({_key:"JessicaThompson", name:'Jessica Thompson'})._id;
  var JamesThompson = g[Person].save({_key:"JamesThompson", name:'James Thompson'})._id;
  g[FOLLOWS].save(JamesThompson,JessicaThompson,{});
  g[FOLLOWS].save(AngelaScope,JessicaThompson,{});
  g[FOLLOWS].save(PaulBlythe,AngelaScope,{});

  g[REVIEWED].save(JessicaThompson,CloudAtlas,{summary:'An amazing journey', rating:95});
  g[REVIEWED].save(JessicaThompson,TheReplacements,{summary:'Silly, but fun', rating:65});
  g[REVIEWED].save(JamesThompson,TheReplacements,{summary:'The coolest football movie ever', rating:100});
  g[REVIEWED].save(AngelaScope,TheReplacements,{summary:'Pretty funny at times', rating:62});
  g[REVIEWED].save(JessicaThompson,Unforgiven,{summary:'Dark, but compelling', rating:85});
  g[REVIEWED].save(JessicaThompson,TheBirdcage,
    {summary:"Slapstick redeemed only by the Robin Williams and Gene Hackman's stellar performances", rating:45}
  );
  g[REVIEWED].save(JessicaThompson,TheDaVinciCode,{summary:'A solid romp', rating:68});
  g[REVIEWED].save(JamesThompson,TheDaVinciCode,{summary:'Fun, but a little far fetched', rating:65});
  g[REVIEWED].save(JessicaThompson,JerryMaguire, {summary:'You had me at Jerry', rating:92});

  return g;
};



var dropGraph = function(name, prefixed) {
  if (prefixed) {
    name = "UnitTest" + name;
  }
  if (Graph._exists(name)) {
    return Graph._drop(name, true);
  }
};


var loadGraph = function(name, prefixed) {
  dropGraph(name);
  switch (name) {
    case "knows_graph":
      return createTraversalExample(prefixed);
    case "routeplanner":
      return createRoutePlannerGraph(prefixed);
    case "social":
      return createSocialGraph(prefixed);
    case "movies":
      return createMoviesGraph(prefixed);
  }
};

exports.loadGraph = loadGraph;
exports.dropGraph = dropGraph;

# AQL Example Queries on an Actors and Movies Database

## Acknowledgments

On [Stackoverflow][1] the user [Vincz][2] asked for some example queries based on graphs.
So credits for this questions go to him. The datasets and queries have been taken from the guys of [neo4j](http://neo4j.com/docs/stable/cypherdoc-movie-database.html). Credits and thanks to them.
As I also think this examples are yet missing I decided to write this recipe.

## Problem

(Copy from Stackoverflow)
Given a collection of **actors** and a collection of **movies**. And a **actIn** edges collection (with a **year** property) to connect the vertex.

\[Actor\] <- act in -> \[Movie\]

 How could I get:

* All actors who acted in "movie1" OR "movie2"
* All actors who acted in both "movie1" AND "movie2" ?
* All common movies between "actor1" and "actor2" ?
* All actors who acted in 3 or more movies ?
* All movies where exactly 6 actors acted in ?
* The number of actors by movie ?
* The number of movies by actor ?
* The number of movies acted in between 2005 and 2010 by actor ?

## Solution

During this solution we will be using arangosh to create and query the data.
All the AQL queries are strings and can simply be copied over to your favorite driver instead of arangosh.

Create a Test Dataset in arangosh:

```js
var actors = db._create("actors");
var movies = db._create("movies");
var actsIn = db._createEdgeCollection("actsIn");

var TheMatrix = movies.save({_key: "TheMatrix", title:'The Matrix', released:1999, tagline:'Welcome to the Real World'})._id;
var Keanu = actors.save({_key: "Keanu", name:'Keanu Reeves', born:1964})._id;
var Carrie = actors.save({_key: "Carrie", name:'Carrie-Anne Moss', born:1967})._id;
var Laurence = actors.save({_key: "Laurence", name:'Laurence Fishburne', born:1961})._id;
var Hugo = actors.save({_key: "Hugo", name:'Hugo Weaving', born:1960})._id;
var Emil = actors.save({_key: "Emil", name:"Emil Eifrem", born: 1978});

actsIn.save(Keanu, TheMatrix, {roles: ["Neo"], year: 1999});
actsIn.save(Carrie, TheMatrix, {roles: ["Trinity"], year: 1999});
actsIn.save(Laurence, TheMatrix, {roles: ["Morpheus"], year: 1999});
actsIn.save(Hugo, TheMatrix, {roles: ["Agent Smith"], year: 1999});
actsIn.save(Emil, TheMatrix, {roles: ["Emil"], year: 1999}); 

var TheMatrixReloaded = movies.save({_key: "TheMatrixReloaded", title: "The Matrix Reloaded", released: 2003, tagline: "Free your mind"});
actsIn.save(Keanu, TheMatrixReloaded, {roles: ["Neo"], year: 2003});
actsIn.save(Carrie, TheMatrixReloaded, {roles: ["Trinity"], year: 2003});
actsIn.save(Laurence, TheMatrixReloaded, {roles: ["Morpheus"], year: 2003});
actsIn.save(Hugo, TheMatrixReloaded, {roles: ["Agent Smith"], year: 2003});

var TheMatrixRevolutions = movies.save({_key: "TheMatrixRevolutions", title: "The Matrix Revolutions", released: 2003, tagline: "Everything that has a beginning has an end"});
actsIn.save(Keanu, TheMatrixRevolutions, {roles: ["Neo"], year: 2003});
actsIn.save(Carrie, TheMatrixRevolutions, {roles: ["Trinity"], year: 2003});
actsIn.save(Laurence, TheMatrixRevolutions, {roles: ["Morpheus"], year: 2003});
actsIn.save(Hugo, TheMatrixRevolutions, {roles: ["Agent Smith"], year: 2003});

var TheDevilsAdvocate = movies.save({_key: "TheDevilsAdvocate", title:"The Devil's Advocate", released:1997, tagline:'Evil has its winning ways'})._id;
var Charlize = actors.save({_key: "Charlize", name:'Charlize Theron', born:1975})._id;
var Al = actors.save({_key: "Al", name:'Al Pacino', born:1940})._id;
actsIn.save(Keanu, TheDevilsAdvocate, {roles: ["Kevin Lomax"], year: 1997});
actsIn.save(Charlize, TheDevilsAdvocate, {roles: ["Mary Ann Lomax"], year: 1997});
actsIn.save(Al, TheDevilsAdvocate, {roles: ["John Milton"], year: 1997});

var AFewGoodMen = movies.save({_key: "AFewGoodMen", title:"A Few Good Men", released:1992, tagline:"In the heart of the nation's capital, in a courthouse of the U.S. government, one man will stop at nothing to keep his honor, and one will stop at nothing to find the truth."})._id;
var TomC = actors.save({_key: "TomC", name:'Tom Cruise', born:1962})._id;
var JackN = actors.save({_key: "JackN", name:'Jack Nicholson', born:1937})._id;
var DemiM = actors.save({_key: "DemiM", name:'Demi Moore', born:1962})._id;
var KevinB = actors.save({_key:"KevinB", name:'Kevin Bacon', born:1958})._id;
var KieferS = actors.save({_key:"KieferS", name:'Kiefer Sutherland', born:1966})._id;
var NoahW = actors.save({_key:"NoahW", name:'Noah Wyle', born:1971})._id;
var CubaG = actors.save({_key:"CubaG", name:'Cuba Gooding Jr.', born:1968})._id;
var KevinP = actors.save({_key:"KevinP", name:'Kevin Pollak', born:1957})._id;
var JTW = actors.save({_key:"JTW", name:'J.T. Walsh', born:1943})._id;
var JamesM = actors.save({_key:"JamesM", name:'James Marshall', born:1967})._id;
var ChristopherG = actors.save({_key:"ChristopherG", name:'Christopher Guest', born:1948})._id;
actsIn.save(TomC,AFewGoodMen,{roles:['Lt. Daniel Kaffee'], year: 1992});
actsIn.save(JackN,AFewGoodMen,{roles:['Col. Nathan R. Jessup'], year: 1992});
actsIn.save(DemiM,AFewGoodMen,{roles:['Lt. Cdr. JoAnne Galloway'], year: 1992});
actsIn.save(KevinB,AFewGoodMen,{roles:['Capt. Jack Ross'], year: 1992});
actsIn.save(KieferS,AFewGoodMen,{ roles:['Lt. Jonathan Kendrick'], year: 1992});
actsIn.save(NoahW,AFewGoodMen,{roles:['Cpl. Jeffrey Barnes'], year: 1992});
actsIn.save(CubaG,AFewGoodMen,{ roles:['Cpl. Carl Hammaker'], year: 1992});
actsIn.save(KevinP,AFewGoodMen,{roles:['Lt. Sam Weinberg'], year: 1992});
actsIn.save(JTW,AFewGoodMen,{roles:['Lt. Col. Matthew Andrew Markinson'], year: 1992});
actsIn.save(JamesM,AFewGoodMen,{roles:['Pfc. Louden Downey'], year: 1992});
actsIn.save(ChristopherG,AFewGoodMen,{ roles:['Dr. Stone'], year: 1992});

var TopGun = movies.save({_key:"TopGun", title:"Top Gun", released:1986, tagline:'I feel the need, the need for speed.'})._id;
var KellyM = actors.save({_key:"KellyM", name:'Kelly McGillis', born:1957})._id;
var ValK = actors.save({_key:"ValK", name:'Val Kilmer', born:1959})._id;
var AnthonyE = actors.save({_key:"AnthonyE", name:'Anthony Edwards', born:1962})._id;
var TomS = actors.save({_key:"TomS", name:'Tom Skerritt', born:1933})._id;
var MegR = actors.save({_key:"MegR", name:'Meg Ryan', born:1961})._id;
actsIn.save(TomC,TopGun,{roles:['Maverick'], year: 1986});
actsIn.save(KellyM,TopGun,{roles:['Charlie'], year: 1986});
actsIn.save(ValK,TopGun,{roles:['Iceman'], year: 1986});
actsIn.save(AnthonyE,TopGun,{roles:['Goose'], year: 1986});
actsIn.save(TomS,TopGun,{roles:['Viper'], year: 1986});
actsIn.save(MegR,TopGun,{roles:['Carole'], year: 1986});

var JerryMaguire = movies.save({_key:"JerryMaguire", title:'Jerry Maguire', released:2000, tagline:'The rest of his life begins now.'})._id;
var ReneeZ = actors.save({_key:"ReneeZ", name:'Renee Zellweger', born:1969})._id;
var KellyP = actors.save({_key:"KellyP", name:'Kelly Preston', born:1962})._id;
var JerryO = actors.save({_key:"JerryO", name:"Jerry O'Connell", born:1974})._id;
var JayM = actors.save({_key:"JayM", name:'Jay Mohr', born:1970})._id;
var BonnieH = actors.save({_key:"BonnieH", name:'Bonnie Hunt', born:1961})._id;
var ReginaK = actors.save({_key:"ReginaK", name:'Regina King', born:1971})._id;
var JonathanL = actors.save({_key:"JonathanL", name:'Jonathan Lipnicki', born:1996})._id;
actsIn.save(TomC,JerryMaguire,{roles:['Jerry Maguire'], year: 2000});
actsIn.save(CubaG,JerryMaguire,{roles:['Rod Tidwell'], year: 2000});
actsIn.save(ReneeZ,JerryMaguire,{roles:['Dorothy Boyd'], year: 2000});
actsIn.save(KellyP,JerryMaguire,{roles:['Avery Bishop'], year: 2000});
actsIn.save(JerryO,JerryMaguire,{roles:['Frank Cushman'], year: 2000});
actsIn.save(JayM,JerryMaguire,{roles:['Bob Sugar'], year: 2000});
actsIn.save(BonnieH,JerryMaguire,{roles:['Laurel Boyd'], year: 2000});
actsIn.save(ReginaK,JerryMaguire,{roles:['Marcee Tidwell'], year: 2000});
actsIn.save(JonathanL,JerryMaguire,{roles:['Ray Boyd'], year: 2000});

var StandByMe = movies.save({_key:"StandByMe", title:"Stand By Me", released:1986, tagline:"For some, it's the last real taste of innocence, and the first real taste of life. But for everyone, it's the time that memories are made of."})._id;
var RiverP = actors.save({_key:"RiverP", name:'River Phoenix', born:1970})._id;
var CoreyF = actors.save({_key:"CoreyF", name:'Corey Feldman', born:1971})._id;
var WilW = actors.save({_key:"WilW", name:'Wil Wheaton', born:1972})._id;
var JohnC = actors.save({_key:"JohnC", name:'John Cusack', born:1966})._id;
var MarshallB = actors.save({_key:"MarshallB", name:'Marshall Bell', born:1942})._id;
actsIn.save(WilW,StandByMe,{roles:['Gordie Lachance'], year: 1986});
actsIn.save(RiverP,StandByMe,{roles:['Chris Chambers'], year: 1986});
actsIn.save(JerryO,StandByMe,{roles:['Vern Tessio'], year: 1986});
actsIn.save(CoreyF,StandByMe,{roles:['Teddy Duchamp'], year: 1986});
actsIn.save(JohnC,StandByMe,{roles:['Denny Lachance'], year: 1986});
actsIn.save(KieferS,StandByMe,{roles:['Ace Merrill'], year: 1986});
actsIn.save(MarshallB,StandByMe,{roles:['Mr. Lachance'], year: 1986});

var AsGoodAsItGets = movies.save({_key:"AsGoodAsItGets", title:'As Good as It Gets', released:1997, tagline:'A comedy from the heart that goes for the throat.'})._id;
var HelenH = actors.save({_key:"HelenH", name:'Helen Hunt', born:1963})._id;
var GregK = actors.save({_key:"GregK", name:'Greg Kinnear', born:1963})._id;
actsIn.save(JackN,AsGoodAsItGets,{roles:['Melvin Udall'], year: 1997});
actsIn.save(HelenH,AsGoodAsItGets,{roles:['Carol Connelly'], year: 1997});
actsIn.save(GregK,AsGoodAsItGets,{roles:['Simon Bishop'], year: 1997});
actsIn.save(CubaG,AsGoodAsItGets,{roles:['Frank Sachs'], year: 1997});

var WhatDreamsMayCome = movies.save({_key:"WhatDreamsMayCome", title:'What Dreams May Come', released:1998, tagline:'After life there is more. The end is just the beginning.'})._id;
var AnnabellaS = actors.save({_key:"AnnabellaS", name:'Annabella Sciorra', born:1960})._id;
var MaxS = actors.save({_key:"MaxS", name:'Max von Sydow', born:1929})._id;
var WernerH = actors.save({_key:"WernerH", name:'Werner Herzog', born:1942})._id;
var Robin = actors.save({_key:"Robin", name:'Robin Williams', born:1951})._id;
actsIn.save(Robin,WhatDreamsMayCome,{roles:['Chris Nielsen'], year: 1998});
actsIn.save(CubaG,WhatDreamsMayCome,{roles:['Albert Lewis'], year: 1998});
actsIn.save(AnnabellaS,WhatDreamsMayCome,{roles:['Annie Collins-Nielsen'], year: 1998});
actsIn.save(MaxS,WhatDreamsMayCome,{roles:['The Tracker'], year: 1998});
actsIn.save(WernerH,WhatDreamsMayCome,{roles:['The Face'], year: 1998});

var SnowFallingonCedars = movies.save({_key:"SnowFallingonCedars", title:'Snow Falling on Cedars', released:1999, tagline:'First loves last. Forever.'})._id;
var EthanH = actors.save({_key:"EthanH", name:'Ethan Hawke', born:1970})._id;
var RickY = actors.save({_key:"RickY", name:'Rick Yune', born:1971})._id;
var JamesC = actors.save({_key:"JamesC", name:'James Cromwell', born:1940})._id;
actsIn.save(EthanH,SnowFallingonCedars,{roles:['Ishmael Chambers'], year: 1999});
actsIn.save(RickY,SnowFallingonCedars,{roles:['Kazuo Miyamoto'], year: 1999});
actsIn.save(MaxS,SnowFallingonCedars,{roles:['Nels Gudmundsson'], year: 1999});
actsIn.save(JamesC,SnowFallingonCedars,{roles:['Judge Fielding'], year: 1999});

var YouveGotMail = movies.save({_key:"YouveGotMail", title:"You've Got Mail", released:1998, tagline:'At odds in life... in love on-line.'})._id;
var ParkerP = actors.save({_key:"ParkerP", name:'Parker Posey', born:1968})._id;
var DaveC = actors.save({_key:"DaveC", name:'Dave Chappelle', born:1973})._id;
var SteveZ = actors.save({_key:"SteveZ", name:'Steve Zahn', born:1967})._id;
var TomH = actors.save({_key:"TomH", name:'Tom Hanks', born:1956})._id;
actsIn.save(TomH,YouveGotMail,{roles:['Joe Fox'], year: 1998});
actsIn.save(MegR,YouveGotMail,{roles:['Kathleen Kelly'], year: 1998});
actsIn.save(GregK,YouveGotMail,{roles:['Frank Navasky'], year: 1998});
actsIn.save(ParkerP,YouveGotMail,{roles:['Patricia Eden'], year: 1998});
actsIn.save(DaveC,YouveGotMail,{roles:['Kevin Jackson'], year: 1998});
actsIn.save(SteveZ,YouveGotMail,{roles:['George Pappas'], year: 1998});

var SleeplessInSeattle = movies.save({_key:"SleeplessInSeattle", title:'Sleepless in Seattle', released:1993, tagline:'What if someone you never met, someone you never saw, someone you never knew was the only someone for you?'})._id;
var RitaW = actors.save({_key:"RitaW", name:'Rita Wilson', born:1956})._id;
var BillPull = actors.save({_key:"BillPull", name:'Bill Pullman', born:1953})._id;
var VictorG = actors.save({_key:"VictorG", name:'Victor Garber', born:1949})._id;
var RosieO = actors.save({_key:"RosieO", name:"Rosie O'Donnell", born:1962})._id;
actsIn.save(TomH,SleeplessInSeattle,{roles:['Sam Baldwin'], year: 1993});
actsIn.save(MegR,SleeplessInSeattle,{roles:['Annie Reed'], year: 1993});
actsIn.save(RitaW,SleeplessInSeattle,{roles:['Suzy'], year: 1993});
actsIn.save(BillPull,SleeplessInSeattle,{roles:['Walter'], year: 1993});
actsIn.save(VictorG,SleeplessInSeattle,{roles:['Greg'], year: 1993});
actsIn.save(RosieO,SleeplessInSeattle,{roles:['Becky'], year: 1993});

var JoeVersustheVolcano = movies.save({_key:"JoeVersustheVolcano", title:'Joe Versus the Volcano', released:1990, tagline:'A story of love, lava and burning desire.'})._id;
var Nathan = actors.save({_key:"Nathan", name:'Nathan Lane', born:1956})._id;
actsIn.save(TomH,JoeVersustheVolcano,{roles:['Joe Banks'], year: 1990});
actsIn.save(MegR,JoeVersustheVolcano,{roles:['DeDe', 'Angelica Graynamore', 'Patricia Graynamore'], year: 1990});
actsIn.save(Nathan,JoeVersustheVolcano,{roles:['Baw'], year: 1990});

var WhenHarryMetSally = movies.save({_key:"WhenHarryMetSally", title:'When Harry Met Sally', released:1998, tagline:'At odds in life... in love on-line.'})._id;
var BillyC = actors.save({_key:"BillyC", name:'Billy Crystal', born:1948})._id;
var CarrieF = actors.save({_key:"CarrieF", name:'Carrie Fisher', born:1956})._id;
var BrunoK = actors.save({_key:"BrunoK", name:'Bruno Kirby', born:1949})._id;
actsIn.save(BillyC,WhenHarryMetSally,{roles:['Harry Burns'], year: 1998});
actsIn.save(MegR,WhenHarryMetSally,{roles:['Sally Albright'], year: 1998});
actsIn.save(CarrieF,WhenHarryMetSally,{roles:['Marie'], year: 1998});
actsIn.save(BrunoK,WhenHarryMetSally,{roles:['Jess'], year: 1998});
```

##  All actors who acted in "movie1" OR "movie2"

Say we want to find all actors who acted in "TheMatrix" OR "TheDevilsAdvocate":

First lets try to get all actors for one movie:

```js
db._query("FOR x IN ANY 'movies/TheMatrix' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN x._id").toArray();
```

Result:
```json
[
  [
    "actors/Keanu",
    "actors/Hugo",
    "actors/Emil",
    "actors/Carrie",
    "actors/Laurence"
  ]
]
```

Now we continue to form a UNION_DISTINCT of two NEIGHBORS queries which will be the solution:

```js
db._query("FOR x IN UNION_DISTINCT ((FOR y IN ANY 'movies/TheMatrix' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id), (FOR y IN ANY 'movies/TheDevilsAdvocate' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id)) RETURN x").toArray();
```

```json
[
  [
    "actors/Emil",
    "actors/Hugo",
    "actors/Carrie",
    "actors/Laurence",
    "actors/Keanu",
    "actors/Al",
    "actors/Charlize"
  ]
]
```

## All actors who acted in both "movie1" AND "movie2" ?

This is almost identical to the question above.
But this time we are not intrested in a UNION but in a INTERSECTION:

```js
db._query("FOR x IN INTERSECTION ((FOR y IN ANY 'movies/TheMatrix' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id), (FOR y IN ANY 'movies/TheDevilsAdvocate' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id)) RETURN x").toArray();
```

```json
[
  [
    "actors/Keanu"
  ]
]
```

## All common movies between "actor1" and "actor2" ?

This is actually identical to the question about common actors in movie1 and movie2.
We just have to change the starting vertices.
As an example let's find all movies where Hugo Weaving ("Hugo") and Keanu Reeves are co-starring:

```js
db._query("FOR x IN INTERSECTION ((FOR y IN ANY 'actors/Hugo' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id), (FOR y IN ANY 'actors/Keanu' actsIn OPTIONS {bfs: true, uniqueVertices: 'global'} RETURN y._id)) RETURN x").toArray();
```

```json
[
  [
    "movies/TheMatrixRevolutions",
    "movies/TheMatrixReloaded",
    "movies/TheMatrix"
  ]
]
```

## All actors who acted in 3 or more movies ?

This question is different, we cannot make use of the neighbors function here.
Instead we will make use of the edge-index and the COLLECT statement of AQL for grouping.
The basic idea is to group all edges by their startVertex (which in this dataset is always the actor).
Then we remove all actors with less than 3 movies from the result.
As I am also interested in the number of movies an actor has acted in, I included the value in the result as well:

```js
db._query("FOR x IN actsIn COLLECT actor = x._from WITH COUNT INTO counter FILTER counter >= 3 RETURN {actor: actor, movies: counter}").toArray()
```

```json
[
  {
    "actor" : "actors/Carrie",
    "movies" : 3
  },
  {
    "actor" : "actors/CubaG",
    "movies" : 4
  },
  {
    "actor" : "actors/Hugo",
    "movies" : 3
  },
  {
    "actor" : "actors/Keanu",
    "movies" : 4
  },
  {
    "actor" : "actors/Laurence",
    "movies" : 3
  },
  {
    "actor" : "actors/MegR",
    "movies" : 5
  },
  {
    "actor" : "actors/TomC",
    "movies" : 3
  },
  {
    "actor" : "actors/TomH",
    "movies" : 3
  }
]
```

## All movies where exactly 6 actors acted in ?

The same idea as in the query before, but with equality filter, however now we need the movie instead of the actor, so we return the _to attribute:

```js
db._query("FOR x IN actsIn COLLECT movie = x._to WITH COUNT INTO counter FILTER counter == 6 RETURN movie").toArray()
```

```json
[
  "movies/SleeplessInSeattle",
  "movies/TopGun",
  "movies/YouveGotMail"
]
```

## The number of actors by movie ?

We remember in our dataset _to on the edge corresponds to the movie, so we count how often the same _to appears.
This is the number of actors.
The query is almost identical to the ones before but without the FILTER after COLLECT:

```js
db._query("FOR x IN actsIn COLLECT movie = x._to WITH COUNT INTO counter RETURN {movie: movie, actors: counter}").toArray()
```

```json
[
  {
    "movie" : "movies/AFewGoodMen",
    "actors" : 11
  },
  {
    "movie" : "movies/AsGoodAsItGets",
    "actors" : 4
  },
  {
    "movie" : "movies/JerryMaguire",
    "actors" : 9
  },
  {
    "movie" : "movies/JoeVersustheVolcano",
    "actors" : 3
  },
  {
    "movie" : "movies/SleeplessInSeattle",
    "actors" : 6
  },
  {
    "movie" : "movies/SnowFallingonCedars",
    "actors" : 4
  },
  {
    "movie" : "movies/StandByMe",
    "actors" : 7
  },
  {
    "movie" : "movies/TheDevilsAdvocate",
    "actors" : 3
  },
  {
    "movie" : "movies/TheMatrix",
    "actors" : 5
  },
  {
    "movie" : "movies/TheMatrixReloaded",
    "actors" : 4
  },
  {
    "movie" : "movies/TheMatrixRevolutions",
    "actors" : 4
  },
  {
    "movie" : "movies/TopGun",
    "actors" : 6
  },
  {
    "movie" : "movies/WhatDreamsMayCome",
    "actors" : 5
  },
  {
    "movie" : "movies/WhenHarryMetSally",
    "actors" : 4
  },
  {
    "movie" : "movies/YouveGotMail",
    "actors" : 6
  }
]
```

## The number of movies by actor ?

I think you get the picture by now ;)

```js
db._query("FOR x IN actsIn COLLECT actor = x._from WITH COUNT INTO counter RETURN {actor: actor, movies: counter}").toArray()
```

```json
[
  {
    "actor" : "actors/Al",
    "movies" : 1
  },
  {
    "actor" : "actors/AnnabellaS",
    "movies" : 1
  },
  {
    "actor" : "actors/AnthonyE",
    "movies" : 1
  },
  {
    "actor" : "actors/BillPull",
    "movies" : 1
  },
  {
    "actor" : "actors/BillyC",
    "movies" : 1
  },
  {
    "actor" : "actors/BonnieH",
    "movies" : 1
  },
  {
    "actor" : "actors/BrunoK",
    "movies" : 1
  },
  {
    "actor" : "actors/Carrie",
    "movies" : 3
  },
  {
    "actor" : "actors/CarrieF",
    "movies" : 1
  },
  {
    "actor" : "actors/Charlize",
    "movies" : 1
  },
  {
    "actor" : "actors/ChristopherG",
    "movies" : 1
  },
  {
    "actor" : "actors/CoreyF",
    "movies" : 1
  },
  {
    "actor" : "actors/CubaG",
    "movies" : 4
  },
  {
    "actor" : "actors/DaveC",
    "movies" : 1
  },
  {
    "actor" : "actors/DemiM",
    "movies" : 1
  },
  {
    "actor" : "actors/Emil",
    "movies" : 1
  },
  {
    "actor" : "actors/EthanH",
    "movies" : 1
  },
  {
    "actor" : "actors/GregK",
    "movies" : 2
  },
  {
    "actor" : "actors/HelenH",
    "movies" : 1
  },
  {
    "actor" : "actors/Hugo",
    "movies" : 3
  },
  {
    "actor" : "actors/JackN",
    "movies" : 2
  },
  {
    "actor" : "actors/JamesC",
    "movies" : 1
  },
  {
    "actor" : "actors/JamesM",
    "movies" : 1
  },
  {
    "actor" : "actors/JayM",
    "movies" : 1
  },
  {
    "actor" : "actors/JerryO",
    "movies" : 2
  },
  {
    "actor" : "actors/JohnC",
    "movies" : 1
  },
  {
    "actor" : "actors/JonathanL",
    "movies" : 1
  },
  {
    "actor" : "actors/JTW",
    "movies" : 1
  },
  {
    "actor" : "actors/Keanu",
    "movies" : 4
  },
  {
    "actor" : "actors/KellyM",
    "movies" : 1
  },
  {
    "actor" : "actors/KellyP",
    "movies" : 1
  },
  {
    "actor" : "actors/KevinB",
    "movies" : 1
  },
  {
    "actor" : "actors/KevinP",
    "movies" : 1
  },
  {
    "actor" : "actors/KieferS",
    "movies" : 2
  },
  {
    "actor" : "actors/Laurence",
    "movies" : 3
  },
  {
    "actor" : "actors/MarshallB",
    "movies" : 1
  },
  {
    "actor" : "actors/MaxS",
    "movies" : 2
  },
  {
    "actor" : "actors/MegR",
    "movies" : 5
  },
  {
    "actor" : "actors/Nathan",
    "movies" : 1
  },
  {
    "actor" : "actors/NoahW",
    "movies" : 1
  },
  {
    "actor" : "actors/ParkerP",
    "movies" : 1
  },
  {
    "actor" : "actors/ReginaK",
    "movies" : 1
  },
  {
    "actor" : "actors/ReneeZ",
    "movies" : 1
  },
  {
    "actor" : "actors/RickY",
    "movies" : 1
  },
  {
    "actor" : "actors/RitaW",
    "movies" : 1
  },
  {
    "actor" : "actors/RiverP",
    "movies" : 1
  },
  {
    "actor" : "actors/Robin",
    "movies" : 1
  },
  {
    "actor" : "actors/RosieO",
    "movies" : 1
  },
  {
    "actor" : "actors/SteveZ",
    "movies" : 1
  },
  {
    "actor" : "actors/TomC",
    "movies" : 3
  },
  {
    "actor" : "actors/TomH",
    "movies" : 3
  },
  {
    "actor" : "actors/TomS",
    "movies" : 1
  },
  {
    "actor" : "actors/ValK",
    "movies" : 1
  },
  {
    "actor" : "actors/VictorG",
    "movies" : 1
  },
  {
    "actor" : "actors/WernerH",
    "movies" : 1
  },
  {
    "actor" : "actors/WilW",
    "movies" : 1
  }
]
```

## The number of movies acted in between 2005 and 2010 by actor ?

This query is where a Multi Model database actually shines.
First of all we want to use it in production, so we set a skiplistindex on year.
This allows as to execute fast range queries like between 2005 and 2010.

```js
db.actsIn.ensureSkiplist("year")
```

Now we slightly modify our movies by actor query.
However my dataset contains only older movies, so I changed the year range from 1990 - 1995:

```js
db._query("FOR x IN actsIn FILTER x.year >= 1990 && x.year <= 1995 COLLECT actor = x._from WITH COUNT INTO counter RETURN {actor: actor, movies: counter}").toArray()
```

```json
[
  {
    "actor" : "actors/BillPull",
    "movies" : 1
  },
  {
    "actor" : "actors/ChristopherG",
    "movies" : 1
  },
  {
    "actor" : "actors/CubaG",
    "movies" : 1
  },
  {
    "actor" : "actors/DemiM",
    "movies" : 1
  },
  {
    "actor" : "actors/JackN",
    "movies" : 1
  },
  {
    "actor" : "actors/JamesM",
    "movies" : 1
  },
  {
    "actor" : "actors/JTW",
    "movies" : 1
  },
  {
    "actor" : "actors/KevinB",
    "movies" : 1
  },
  {
    "actor" : "actors/KevinP",
    "movies" : 1
  },
  {
    "actor" : "actors/KieferS",
    "movies" : 1
  },
  {
    "actor" : "actors/MegR",
    "movies" : 2
  },
  {
    "actor" : "actors/Nathan",
    "movies" : 1
  },
  {
    "actor" : "actors/NoahW",
    "movies" : 1
  },
  {
    "actor" : "actors/RitaW",
    "movies" : 1
  },
  {
    "actor" : "actors/RosieO",
    "movies" : 1
  },
  {
    "actor" : "actors/TomC",
    "movies" : 1
  },
  {
    "actor" : "actors/TomH",
    "movies" : 2
  },
  {
    "actor" : "actors/VictorG",
    "movies" : 1
  }
]
```

## Comment

**Author:** [Michael Hackstein](https://github.com/mchacki)

**Tags:** #graph #examples

[1]: http://stackoverflow.com/questions/32729314/aql-graph-queries-examples
[2]: http://stackoverflow.com/users/1126414/vincz

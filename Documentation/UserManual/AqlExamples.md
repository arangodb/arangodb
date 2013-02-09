AQL Examples {#AqlExamples}
===========================

@NAVIGATE_AqlExamples
@EMBEDTOC{AqlExamplesTOC}

Simple queries {#AqlExamplesSimple}
===================================

This page contains some examples for how to write queries in AQL. For better
understandability, the query results are also included.

A query that returns a string value. the result string is contained in a list
because the result of every valid query is a list:

    RETURN "this will be returned"

    ["this will be returned"]


A query that creates the cross products of two lists, and runs a projection on it:

    FOR year in [ 2011, 2012, 2013 ]
      FOR quarter IN [ 1, 2, 3, 4 ]
        RETURN { "y" : "year", "q" : quarter, "nice" : CONCAT(TO_STRING(quarter), "/", TO_STRING(year)) }

    [ { "y" : "year", "q" : 1, "nice" : "1/2011" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2011" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2011" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2011" }, 
      { "y" : "year", "q" : 1, "nice" : "1/2012" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2012" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2012" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2012" }, 
      { "y" : "year", "q" : 1, "nice" : "1/2013" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2013" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2013" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2013" } ]

Collection-based queries {#AqlExamplesCollection}
=================================================

Return 5 documents from a users collection, unaltered:

    FOR u IN users 
      LIMIT 0, 5
      RETURN u

    [ { "_id" : "9259836/10505020", "_rev" : "10505020", "active" : true, "id" : 100, "age" : 37, "name" : "John", "gender" : "m" }, 
      { "_id" : "9259836/11553596", "_rev" : "11553596", "active" : true, "id" : 107, "age" : 30, "gender" : "m", "name" : "Anthony" }, 
      { "_id" : "9259836/11094844", "_rev" : "11094844", "active" : true, "id" : 101, "age" : 36, "name" : "Fred", "gender" : "m" }, 
      { "_id" : "9259836/11619132", "_rev" : "11619132", "active" : true, "id" : 108, "age" : 29, "name" : "Jim", "gender" : "m" }, 
      { "_id" : "9259836/11160380", "_rev" : "11160380", "active" : false, "id" : 102, "age" : 35, "name" : "Jacob", "gender" : "m" } ]

Return a projection from a users collection:

    FOR u IN users 
      LIMIT 0, 5
      RETURN { "user" : { "isActive": u.active ? "yes" : "no", "name" : u.name } }

    [ { "user" : { "isActive" : "yes", "name" : "John" } }, 
      { "user" : { "isActive" : "yes", "name" : "Anthony" } }, 
      { "user" : { "isActive" : "yes", "name" : "Fred" } }, 
      { "user" : { "isActive" : "yes", "name" : "Jim" } }, 
      { "user" : { "isActive" : "no", "name" : "Jacob" } } ]

Return a filtered projection from a users collection:

    FOR u IN users 
      FILTER u.active == true && u.age >= 30
      SORT u.age DESC
      RETURN { "age" : u.age, "name" : u.name }

    [ { "age" : 37, "name" : "John" }, 
      { "age" : 37, "name" : "Sophia" }, 
      { "age" : 36, "name" : "Fred" }, 
      { "age" : 36, "name" : "Emma" }, 
      { "age" : 34, "name" : "Madison" }, 
      { "age" : 33, "name" : "Michael" }, 
      { "age" : 33, "name" : "Chloe" }, 
      { "age" : 32, "name" : "Alexander" }, 
      { "age" : 31, "name" : "Daniel" }, 
      { "age" : 31, "name" : "Abigail" }, 
      { "age" : 30, "name" : "Anthony" }, 
      { "age" : 30, "name" : "Isabella" } ]

Joins {#AqlExamplesJoins}
=========================

Getting the names of friends (also users) for users. this is achieved by "joining" a relations table:

    FOR u IN users
      FILTER u.active == true 
      LET friends = (FOR f IN userRelations 
	FILTER f.from == u.id 
	FOR u2 IN users 
	  FILTER f.to == u2.id 
	  RETURN u2.name
      ) 
      RETURN { "user" : u.name, "friends" : friends }

    [ { "user" : "John", "friends" : ["Diego", "Mary", "Abigail"] }, 
      { "user" : "Anthony", "friends" : ["Madison"] }, 
      { "user" : "Fred", "friends" : ["Mariah"] }, 
      { "user" : "Jim", "friends" : ["Mariah"] }, 
      { "user" : "Diego", "friends" : ["Mary"] }, 
      { "user" : "Sophia", "friends" : ["Madison", "John"] }, 
      { "user" : "Michael", "friends" : ["John", "Jim"] }, 
      { "user" : "Emma", "friends" : ["Jacob", "Madison"] }, 
      { "user" : "Alexander", "friends" : ["Michael", "John"] }, 
      { "user" : "Daniel", "friends" : ["Eva"] }, 
      { "user" : "Madison", "friends" : ["Anthony", "Daniel"] }, 
      { "user" : "Chloe", "friends" : ["Alexander"] }, 
      { "user" : "Abigail", "friends" : ["Daniel", "John", "Jacob", "Jim"] }, 
      { "user" : "Isabella", "friends" : ["Madison", "Diego"] }, 
      { "user" : "Mary", "friends" : ["Isabella", "Diego", "Michael"] }, 
      { "user" : "Mariah", "friends" : ["Madison", "Ethan", "Eva"] } ]

Getting users favorite song names from a joined "songs" collection:

    FOR u IN users 
      LET likes = (
	FOR s IN songs
	  FILTER s._id IN u.likes
	  RETURN CONCAT(s.artist, " - ", s.song)
      )
      SORT RAND()
      LIMIT 0, 8
      RETURN { "user" : u.name, "likes" : likes }

    [ { "user" : "Eva", "likes" : ["Chocolate - Ritmo De La Noche", "4 The Cause - Stand By Me", "Tony Carey - Room with a view"] }, 
      { "user" : "Mary", "likes" : ["Hall and Oates - Maneater", "Elton John - Candle In The Wind", "A-Ha - Crying In The Rain", "Laid Back - Sunshine Reggae", "Cock Robin - The promise you made"] }, 
      { "user" : "Alexander", "likes" : ["Moby - Feel so real", "Rednex - Old pop in an oak", "2 Unlimited - No Limit"] }, 
      { "user" : "Michael", "likes" : ["The Kelly Family - David's Song"] }, 
      { "user" : "Ethan", "likes" : ["Technotronic - Megamix", "Gipsy Kings - Baila me", "Goombay Dance Band - Seven Tears", "Sandra - Hiroshima"] }, { "user" : "Isabella", "likes" : ["Milli Vanilli - Girl, I'm Gonna Miss You", "Technotronic - Get Up", "Right Said Fred - Don't Talk Just Kiss", "Peter Schilling - Major Tom (Völlig losgelöst)"] }, 
      { "user" : "Abigail", "likes" : ["Tina Turner - Typical male", "Liquido - Narcotic"] }, 
      { "user" : "Jim", "likes" : ["Berlin - Take my breath away", "Ashford & Simpson - Solid", "Fine Young Cannibals - She drives me cracy", "Cut'N'Move - Give it up", "Cyndi Lauper - Time after time"] }, 
      { "user" : "Jacob", "likes" : ["Kylie Minogue - The Loco-motion", "Eruption - Runaway"] } ]

Grouping {#AqlExamplesGrouping}
===============================

Group users by age:

    FOR u IN users 
      FILTER u.active == true
      COLLECT age = u.age INTO group
      SORT age DESC
      RETURN { "age" : age, "users" : group }

    [ { "age" : 37, "users" : ["Sophia", "John"] }, 
      { "age" : 36, "users" : ["Emma", "Fred"] }, 
      { "age" : 34, "users" : ["Madison"] }, 
      { "age" : 33, "users" : ["Chloe", "Michael"] }, 
      { "age" : 32, "users" : ["Alexander"] }, 
      { "age" : 31, "users" : ["Abigail", "Daniel"] }, 
      { "age" : 30, "users" : ["Isabella", "Anthony"] }, 
      { "age" : 29, "users" : ["Mary", "Jim"] }, 
      { "age" : 28, "users" : ["Mariah", "Diego"] } ]

Group users by agegroup and gender:

    FOR u IN users 
      FILTER u.active == true
      COLLECT ageGroup = FLOOR(u.age/5) * 5, gender = u.gender INTO group
      SORT ageGroup DESC
      RETURN { "ageGroup" : ageGroup, "gender" : gender, "numUsers" : LENGTH(group) }

    [ { "ageGroup" : 35, "gender" : "f", "numUsers" : 2 }, 
      { "ageGroup" : 35, "gender" : "m", "numUsers" : 2 }, 
      { "ageGroup" : 30, "gender" : "f", "numUsers" : 4 }, 
      { "ageGroup" : 30, "gender" : "m", "numUsers" : 4 }, 
      { "ageGroup" : 25, "gender" : "f", "numUsers" : 2 }, 
      { "ageGroup" : 25, "gender" : "m", "numUsers" : 2 } ]

Get the 3 agegroups with most users:

    FOR u IN users 
      FILTER u.active == true
      COLLECT ageGroup = FLOOR(u.age/5) * 5 INTO group
      FILTER LENGTH(group) > 2 /* group must contain at least 3 users */
      SORT LENGTH(group) DESC
      LIMIT 0, 3
      RETURN { "ageGroup" : ageGroup, "numUsers" : LENGTH(group), "users" : group[*].u.name }

    [ { "ageGroup" : 30, "numUsers" : 8, "users" : ["Alexander", "Isabella", "Michael", "Abigail", "Anthony", "Daniel", "Madison", "Chloe"] }, 
      { "ageGroup" : 25, "numUsers" : 4, "users" : ["Mariah", "Mary", "Jim", "Diego"] }, 
      { "ageGroup" : 35, "numUsers" : 4, "users" : ["Emma", "Sophia", "Fred", "John"] } ]

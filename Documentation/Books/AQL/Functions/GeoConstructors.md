GeoJSON Constructors
---------------------

The following helper functions are available to easily create valid GeoJSON output. 
In all cases you can write equivalent JSON yourself, but thes functions will help
you to make all your AQL queries shorter and easier to read.

### GEO_POINT()

`GEO_POINT(latitude, longitude) → GeoJSON Point`

Returns a valid GeoJSON Point.

- **latitude** (number): the latitude portion of the point
- **longitude** (number): the longitude portion of the point

```js
return GEO_POINT(1.0, 2.0)

// {
//   "type": "Point",
//   "coordinates": [1.0, 2,0]
// }
```

### GEO_MULTIPOINT()

`GEO_MULTIPOINT(array) → GeoJSON LineString`

Returns a valid GeoJSON LineString. Needs at least two longitude/latitude pairs.

- **array** (points): array of points e.g. longitude/latitude pairs

```js
return GEO_MULTIPOINT([
  [35, 10], [45, 45]
])

//  {
//    "coordinates": [
//      [
//        35,
//        10
//      ],
//      [
//        45,
//        45
//      ]
//    ],
//    "type": "MultiPoint"
//  }
```

### GEO_POLYGON()

`GEO_POLYGON(array) → GeoJSON Polygon`

Returns a valid GeoJSON Polygon. Needs at least three longitude/latitude pairs.

- **array** (points): array of longitude/latitude pairs

Simple Polygon Builder:

```js
return GEO_POLYGON([
  [1.0, 2.0], [3.0, 4.0], [5.0, 6.0]
])

// {
//   "coordinates": [
//     [
//       [
//         1,
//         2
//       ],
//       [
//         3,
//         4
//       ],
//       [
//         5,
//         6
//       ]
//     ]
//   ],
//   "type": "Polygon"
//  }
```

Advanced Polygon Builder with a hole inside:

```js
return GEO_POLYGON([
  [[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],
  [[20, 30], [35, 35], [30, 20], [20, 30]]
])

//  {
//    "coordinates": [
//      [
//        [
//          35,
//          10
//        ],
//        [
//          45,
//          45
//        ],
//        [
//          15,
//          40
//        ],
//        [
//          10,
//          20
//        ],
//        [
//          35,
//          10
//        ]
//      ],
//      [
//        [
//          20,
//          30
//        ],
//        [
//          35,
//          35
//        ],
//        [
//          30,
//          20
//        ],
//        [
//          20,
//          30
//        ]
//      ]
//    ],
//    "type": "Polygon"
//  }

```

### GEO_LINESTRING()

`GEO_LINESTRING(array) → GeoJSON LineString`

Returns a valid GeoJSON LineString. Needs at least two longitude/latitude pairs.

- **array** (points): array of longitude/latitude pairs

```js
return GEO_LINESTRING([
  [35, 10], [45, 45]
])

//  {
//    "coordinates": [
//      [
//        35,
//        10
//      ],
//      [
//        45,
//        45
//      ]
//    ],
//    "type": "LineString"
//  }
```

### GEO_MULTILINESTRING()

`GEO_MULTILINESTRING(array) → GeoJSON MultiLineString`

Returns a valid GeoJSON MultiLineString. Needs at least two linestrings elements.

- **array** (points): array of linestrings

```js
return GEO_MULTILINESTRING([
 [[100.0, 0.0], [101.0, 1.0]],
 [[102.0, 2.0], [101.0, 2.3]]
])

//  {
//    "coordinates": [
//      [
//        [
//          100,
//          0
//        ],
//        [
//          101,
//          1
//        ]
//      ],
//      [
//        [
//          102,
//          2
//        ],
//        [
//          101,
//          2.3
//        ]
//      ]
//    ],
//    "type": "MultiLineString"
//  }
```
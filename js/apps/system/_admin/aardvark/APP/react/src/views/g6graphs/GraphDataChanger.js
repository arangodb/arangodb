/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback, useRef } from 'react';

export const GraphDataChanger = ({ graphData, onChangeGraphData }) => {
  console.log("GraphDataChanger (graphData): ", graphData);

  const changeGraphData = () => {
    const newGraphData = {
      "nodes": [
          {
              "id": "frenchCity/Paris",
              "label": "Paris",
              "style": {
                  "label": {
                      "value": "Paris"
                  }
              }
          },
          {
              "id": "germanCity/Berlin",
              "label": "Berlin",
              "style": {
                  "label": {
                      "value": "Berlin"
                  }
              }
          },
          {
              "id": "germanCity/Hamburg",
              "label": "Hamburg",
              "style": {
                  "label": {
                      "value": "Hamburg"
                  }
              }
          },
          {
              "id": "germanCity/Cologne",
              "label": "Cologne",
              "style": {
                  "label": {
                      "value": "Cologne"
                  }
              }
          }
      ],
      "edges": [
          {
              "id": "internationalHighway/24436",
              "source": "germanCity/Berlin",
              "label": "",
              "color": "#cccccc",
              "target": "frenchCity/Paris",
              "size": 1,
              "sortColor": "#cccccc"
          },
          {
              "id": "germanHighway/24431",
              "source": "germanCity/Berlin",
              "label": "",
              "color": "#cccccc",
              "target": "germanCity/Cologne",
              "size": 1,
              "sortColor": "#cccccc"
          },
          {
              "id": "germanHighway/24432",
              "source": "germanCity/Berlin",
              "label": "",
              "color": "#cccccc",
              "target": "germanCity/Hamburg",
              "size": 1,
              "sortColor": "#cccccc"
          },
          {
              "id": "internationalHighway/24437",
              "source": "germanCity/Hamburg",
              "label": "",
              "color": "#cccccc",
              "target": "frenchCity/Paris",
              "size": 1,
              "sortColor": "#cccccc"
          },
          {
              "id": "germanHighway/24433",
              "source": "germanCity/Hamburg",
              "label": "",
              "color": "#cccccc",
              "target": "germanCity/Cologne",
              "size": 1,
              "sortColor": "#cccccc"
          },
          {
              "id": "internationalHighway/24440",
              "source": "germanCity/Cologne",
              "label": "",
              "color": "#cccccc",
              "target": "frenchCity/Paris",
              "size": 1,
              "sortColor": "#cccccc"
          }
      ],
      "settings": {
          "vertexCollections": [
              {
                  "name": "frenchCity",
                  "id": "24420"
              },
              {
                  "name": "germanCity",
                  "id": "24419"
              }
          ],
          "startVertex": {
              "_key": "Paris",
              "_id": "frenchCity/Paris",
              "_rev": "_dbfRAI6--A",
              "population": 4000000,
              "isCapital": true,
              "geometry": {
                  "type": "Point",
                  "coordinates": [
                      2.3508,
                      48.8567
                  ]
              }
          }
      }
  };
    onChangeGraphData(newGraphData);
    console.log("changeGraphData() - newGraphData: ", newGraphData);
  }

  return (
    <button onClick={changeGraphData}>
      Change Graph Data
    </button>
  );
}

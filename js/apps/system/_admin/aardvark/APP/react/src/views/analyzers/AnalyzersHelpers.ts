import { AnalyzerTypes, AnalyzerTypeState } from "./Analyzer.types";

export const TYPE_TO_LABEL_MAP: {
  [key in AnalyzerTypes]: string;
} = {
  identity: "Identity",
  delimiter: "Delimiter",
  stem: "Stem",
  norm: "Norm",
  ngram: "N-Gram",
  text: "Text",
  aql: "AQL",
  stopwords: "Stopwords",
  collation: "Collation",
  segmentation: "Segmentation",
  nearest_neighbors: "Nearest Neighbors",
  classification: "Classification",
  pipeline: "Pipeline",
  geojson: "GeoJSON",
  geopoint: "GeoPoint",
  geo_s2: "Geo S2",
  minhash: "Min Hash"
};

export const TYPE_TO_INITIAL_VALUES_MAP: {
  [key in AnalyzerTypes]: AnalyzerTypeState;
} = {
  identity: {
    type: "identity"
  },
  delimiter: {
    type: "delimiter",
    properties: {
      delimiter: ""
    }
  },
  stem: {
    type: "stem",
    properties: {
      locale: ""
    }
  },
  norm: {
    type: "norm",
    properties: {
      locale: "",
      case: "none",
      accent: true
    }
  },
  ngram: {
    type: "ngram",
    properties: {
      min: 2,
      max: 3,
      preserveOriginal: false,
      startMarker: "",
      endMarker: "",
      streamType: "binary"
    }
  },
  text: {
    type: "text",
    properties: {
      locale: "",
      case: "lower",
      accent: false,
      stemming: true
    }
  },
  aql: {
    type: "aql",
    properties: {
      queryString: "",
      collapsePositions: false,
      keepNull: true,
      batchSize: 10,
      memoryLimit: 1048576,
      returnType: "string"
    }
  },
  stopwords: {
    type: "stopwords",
    properties: {
      hex: false,
      stopwords: []
    }
  },
  collation: {
    type: "collation",
    properties: {
      locale: ""
    }
  },
  segmentation: {
    type: "segmentation",
    properties: {
      case: "lower",
      break: "alpha"
    }
  },
  nearest_neighbors: {
    type: "nearest_neighbors",
    properties: {
      model_location: ""
    }
  },
  classification: {
    type: "classification",
    properties: {
      model_location: "",
      threshold: 0
    }
  },
  pipeline: {
    type: "pipeline",
    properties: {
      pipeline: []
    }
  },
  geojson: {
    type: "geojson",
    properties: {
      options: {
        maxCells: 20,
        minLevel: 4,
        maxLevel: 23
      },
      type: "shape",
      legacy: false
    }
  },
  geopoint: {
    type: "geopoint",
    properties: {
      options: {
        maxCells: 20,
        minLevel: 4,
        maxLevel: 23
      },
      latitude: [],
      longitude: []
    }
  },
  geo_s2: {
    type: "geo_s2",
    properties: {
      options: {
        maxCells: 20,
        minLevel: 4,
        maxLevel: 23
      },
      type: "shape",
      format: "latLngDouble"
    }
  },
  minhash: {
    type: "minhash",
    properties: {
      analyzer: {
        type: "identity"
      },
      numHashes: undefined
    }
  }
};

export const ANALYZER_TYPE_OPTIONS = Object.keys(TYPE_TO_LABEL_MAP).map(
  type => {
    return {
      value: type,
      label: TYPE_TO_LABEL_MAP[type as AnalyzerTypes]
    };
  }
);

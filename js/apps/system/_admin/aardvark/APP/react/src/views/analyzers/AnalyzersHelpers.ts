import { AqlConfig } from "./forms/AqlConfig";
import { ClassificationConfig } from "./forms/ClassificationConfig";
import { CollationConfig } from "./forms/CollationConfig";
import { DelimiterConfig } from "./forms/DelimiterConfig";
import { GeoJSONConfig } from "./forms/GeoJSONConfig";
import { GeopointConfig } from "./forms/GeopointConfig";
import { GeoS2Config } from "./forms/GeoS2Config";
import { NearestNeighborsConfig } from "./forms/NearestNeighborsConfig";
import { NgramConfig } from "./forms/NgramConfig";
import { NormConfig } from "./forms/NormConfig";
import { PipelineConfig } from "./forms/PipelineConfig";
import { SegmentationConfig } from "./forms/SegmentationConfig";
import { StemConfig } from "./forms/StemConfig";
import { StopwordsConfig } from "./forms/StopwordsConfig";
import { TextConfig } from "./forms/TextConfig";

export const TYPE_TO_LABEL_MAP = {
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
  geo_s2: "Geo S2"
};

export const ANALYZER_TYPE_TO_CONFIG_MAP = {
  identity: null,
  delimiter: DelimiterConfig,
  stem: StemConfig,
  norm: NormConfig,
  ngram: NgramConfig,
  text: TextConfig,
  aql: AqlConfig,
  stopwords: StopwordsConfig,
  collation: CollationConfig,
  segmentation: SegmentationConfig,
  nearest_neighbors: NearestNeighborsConfig,
  classification: ClassificationConfig,
  pipeline: PipelineConfig,
  geojson: GeoJSONConfig,
  geopoint: GeopointConfig,
  geo_s2: GeoS2Config
};

export const TYPE_TO_INITIAL_VALUES_MAP = {
  identity: {
    type: "identity"
  },
  delimiter: {
    type: "delimiter",
    delimiter: ""
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
      break: "aplha"
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
  }
};

export type AnalyzerTypes =
  | "identity"
  | "delimiter"
  | "stem"
  | "norm"
  | "ngram"
  | "text"
  | "aql"
  | "stopwords"
  | "collation"
  | "segmentation"
  | "nearest_neighbors"
  | "classification"
  | "pipeline"
  | "geojson"
  | "geopoint"
  | "geo_s2";

type Feature = "frequency" | "norm" | "position";
type Features = Feature[];

type BaseAnalyzerState = {
  name: string;
  features: Features;
};
type IdentityState = {
  type: "identity";
};

type DelimiterState = {
  type: "delimiter";
  properties: {
    delimiter: string;
  };
};

type LocaleProperty = {
  properties: {
    locale: string;
  };
};

type StemState = LocaleProperty & {
  type: "stem";
};

type CaseProperty = {
  properties: {
    case?: "lower" | "upper" | "none";
  };
};

type AccentProperty = {
  properties: {
    accent?: boolean;
  };
};

type NormState = LocaleProperty &
  CaseProperty &
  AccentProperty & {
    type: "norm";
  };

type NGramBaseProperty = {
  max?: number;
  min?: number;
  preserveOriginal?: boolean;
};

type NGramState = {
  type: "ngram";
  properties: NGramBaseProperty & {
    startMarker?: string;
    endMarker?: string;
    streamType?: "binary" | "utf8";
  };
};

type StopwordsProperty = {
  properties: {
    stopwords?: string[];
  };
};

type TextState = LocaleProperty &
  CaseProperty &
  AccentProperty &
  StopwordsProperty & {
    type: "text";
    properties: {
      stemming?: boolean;
      edgeNgram?: NGramBaseProperty;
      stopwordsPath?: string;
    };
  };

type AqlState = {
  type: "aql";
  properties: {
    queryString: string;
    collapsePositions?: boolean;
    keepNull?: boolean;
    batchSize?: number;
    memoryLimit?: number;
    returnType?: "string" | "number" | "bool";
  };
};

type StopwordsState = StopwordsProperty & {
  type: "stopwords";
  properties: {
    hex?: boolean;
  };
};

type CollationState = LocaleProperty & {
  type: "collation";
};

type SegmentationState = CaseProperty & {
  type: "segmentation";
  properties: {
    break?: "all" | "alpha" | "graphic";
  };
};

type ModelProperty = {
  properties: {
    model_location: string;
    top_k?: number; // [0, 2147483647]
  };
};

type NearestNeighborsState = ModelProperty & {
  type: "nearest_neighbors";
};

type ClassificationState = ModelProperty & {
  type: "classification";
  properties: {
    threshold?: number; // [0.0, 1.0]
  };
};

type PipelineState =
  | DelimiterState
  | StemState
  | NormState
  | NGramState
  | TextState
  | AqlState
  | StopwordsState
  | CollationState
  | SegmentationState
  | NearestNeighborsState
  | ClassificationState;

export type PipelineStates = {
  type: "pipeline";
  properties: {
    pipeline: PipelineState[];
  };
};

export type GeoOptionsProperty = {
  properties: {
    options?: {
      maxCells?: number;
      minLevel?: number;
      maxLevel?: number;
    };
  };
};

export type GeoJsonState = GeoOptionsProperty & {
  type: "geojson";
  properties: {
    type?: "shape" | "centroid" | "point";
    legacy?: boolean;
  };
};

export type GeoS2State = GeoOptionsProperty & {
  type: "geo_s2";
  properties: {
    format?: "latLngDouble" | "latLngInt" | "s2Point";
    type?: "shape" | "centroid" | "point";
  };
};

export type GeoPointState = GeoOptionsProperty & {
  type: "geopoint";
  properties: {
    latitude?: string[];
    longitude?: string[];
  };
};

export type AnalyzerTypeState =
  | IdentityState
  | DelimiterState
  | StemState
  | NormState
  | NGramState
  | TextState
  | AqlState
  | StopwordsState
  | CollationState
  | SegmentationState
  | NearestNeighborsState
  | ClassificationState
  | PipelineStates
  | GeoJsonState
  | GeoPointState
  | GeoS2State;

export type AnalyzerState = BaseAnalyzerState & AnalyzerTypeState;

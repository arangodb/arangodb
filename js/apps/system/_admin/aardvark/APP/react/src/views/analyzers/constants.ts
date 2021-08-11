export const typeNameMap = {
  identity: 'Identity',
  delimiter: 'Delimiter',
  stem: 'Stem',
  norm: 'Norm',
  ngram: 'N-Gram',
  text: 'Text',
  aql: 'AQL',
  geojson: 'GeoJSON',
  geopoint: 'GeoPoint',
  pipeline: 'Pipeline'
};

export interface FormProps {
  formState: { [key: string]: any };
  updateFormField: (field: string, value: any) => void;
}

type AnalyzerType =
  'identity'
  | 'delimiter'
  | 'stem'
  | 'norm'
  | 'ngram'
  | 'text'
  | 'aql'
  | 'geojson'
  | 'geopoint'
  | 'pipeline';
type AnalyzerNormCase = 'lower' | 'upper' | 'none';
type AnalyzerStreamType = 'binary' | 'utf8';
type AnalyzerFeature = 'frequency' | 'norm' | 'position';
type AnalyzerFeatures = AnalyzerFeature[];
type Int = number & { __int__: void };
type AnalyzerEdgeNgram = {
  max?: Int;
  min?: Int;
  preserveOriginal?: boolean;
}
type AnalyzerReturnType = 'string' | 'number' | 'bool';
type AnalyzerGeoJsonType = 'shape' | 'centroid' | 'point';
type AnalyzerGeoJsonOptions = {
  maxCells?: Int;
  minLevel?: Int;
  maxLevel?: Int;
};
type AnalyzerProperties = {
  delimiter?: string;
  locale?: string;
  case?: AnalyzerNormCase;
  max?: Int;
  min?: Int;
  preserveOriginal?: boolean;
  startMarker?: string;
  endMarker?: string;
  streamType?: AnalyzerStreamType;
  accent?: boolean;
  stemming?: boolean;
  edgeNgram?: AnalyzerEdgeNgram;
  stopwords?: string[];
  stopwordsPath?: string;
  queryString?: string;
  collapsePositions?: boolean;
  keepNull?: boolean;
  batchSize?: Int;
  memoryLimit?: Int;
  returnType?: AnalyzerReturnType;
  type?: AnalyzerGeoJsonType;
  options?: AnalyzerGeoJsonOptions;
}

export interface FormState {
  name: string;
  type: AnalyzerType;
  features?: AnalyzerFeatures;
  properties?: AnalyzerProperties;
}

export const formSchema = {
  type: 'object',
  properties: {
    name: {
      nullable: false,
      type: 'string'
    },
    type: {
      nullable: false,
      type: 'string',
      enum: ['identity', 'delimiter', 'stem', 'norm', 'ngram', 'text', 'aql', 'geojson', 'geopoint', 'pipeline']
    },
    features: {
      type: 'array',
      maxItems: 3,
      uniqueItems: true,
      nullable: false,
      items: {
        type: 'string',
        nullable: false,
        enum: ['frequency', 'norm', 'position']
      }
    },
    properties: {
      type: 'object',
      nullable: false,
      properties: {
        delimiter: {
          type: 'string',
          nullable: false
        },
        locale: {
          type: 'string',
          nullable: false
        },
        case: {
          type: 'string',
          nullable: false,
          enum: ['lower', 'upper', 'none']
        },
        min: {
          type: 'integer',
          nullable: false,
          minimum: 1
        },
        max: {
          type: 'integer',
          nullable: false,
          minimum: 1
        },
        preserveOriginal: {
          type: 'boolean',
          nullable: false
        },
        startMarker: {
          type: 'string',
          nullable: false
        },
        endMarker: {
          type: 'string',
          nullable: false
        },
        streamType: {
          type: 'string',
          nullable: false,
          enum: ['binary', 'utf8']
        },
        accent: {
          type: 'boolean',
          nullable: false
        },
        stemming: {
          type: 'boolean',
          nullable: false
        },
        edgeNgram: {
          type: 'object',
          nullable: false,
          properties: {
            min: {
              type: 'integer',
              nullable: false,
              minimum: 1
            },
            max: {
              type: 'integer',
              nullable: false,
              minimum: 1
            },
            preserveOriginal: {
              type: 'boolean',
              nullable: false
            }
          }
        },
        stopwords: {
          type: 'array',
          nullable: false,
          items: {
            type: 'string'
          }
        },
        stopwordsPath: {
          type: 'string',
          nullable: false
        },
        queryString: {
          type: 'string',
          nullable: false
        },
        collapsePositions: {
          type: 'boolean',
          nullable: false
        },
        keepNull: {
          type: 'boolean',
          nullable: false
        },
        batchSize: {
          type: 'integer',
          nullable: false,
          minimum: 1,
          maximum: 1000
        },
        memoryLimit: {
          type: 'integer',
          nullable: false,
          maximum: 33554432
        },
        returnType: {
          nullable: false,
          type: 'string',
          enum: ['string', 'number', 'bool']
        },
        type: {
          nullable: false,
          type: 'string',
          enum: ['shape', 'centroid', 'point']
        },
        options: {
          type: 'object',
          nullable: false,
          properties: {
            maxCells: {
              type: 'integer',
              nullable: false
            },
            minLevel: {
              type: 'integer',
              nullable: false
            },
            maxLevel: {
              type: 'integer',
              nullable: false
            }
          }
        }
      }
    }
  },
  required: ['name', 'type'],
  additionalProperties: false
};

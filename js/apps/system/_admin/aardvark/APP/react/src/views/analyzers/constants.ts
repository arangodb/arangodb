/* eslint-disable camelcase */
import { JSONSchemaType } from 'ajv';
import _, { merge, partial } from 'lodash';

export const typeNameMap = {
  identity: 'Identity',
  delimiter: 'Delimiter',
  stem: 'Stem',
  norm: 'Norm',
  ngram: 'N-Gram',
  text: 'Text',
  aql: 'AQL',
  stopwords: 'Stopwords',
  collation: 'Collation',
  segmentation: 'Segmentation',
  nearest_neighbors: 'Nearest Neighbors',
  classification: 'Classification',
  pipeline: 'Pipeline',
  geojson: 'GeoJSON',
  geopoint: 'GeoPoint'
};

export type Feature = 'frequency' | 'norm' | 'position';
type Features = Feature[];

export type BaseFormState = {
  name: string;
  features: Features;
};

type IdentityState = {
  type: 'identity'
};

export type DelimiterState = {
  type: 'delimiter';
  properties: {
    delimiter: string;
  };
};

export type LocaleProperty = {
  properties: {
    locale: string;
  };
};

export type StemState = LocaleProperty & {
  type: 'stem';
};

export type CaseProperty = {
  properties: {
    case?: 'lower' | 'upper' | 'none';
  };
};

export type AccentProperty = {
  properties: {
    accent?: boolean;
  };
};

export type NormState = LocaleProperty & CaseProperty & AccentProperty & {
  type: 'norm';
};

export type NGramBaseProperty = {
  max?: number;
  min?: number;
  preserveOriginal?: boolean;
};

export type NGramState = {
  type: 'ngram';
  properties: NGramBaseProperty & {
    startMarker?: string;
    endMarker?: string;
    streamType?: 'binary' | 'utf8';
  };
};

export type StopwordsProperty = {
  properties: {
    stopwords?: string[];
  };
};

export type TextState = LocaleProperty & CaseProperty & AccentProperty & StopwordsProperty & {
  type: 'text';
  properties: {
    stemming?: boolean;
    edgeNgram?: NGramBaseProperty;
    stopwordsPath?: string;
  };
};

export type AqlState = {
  type: 'aql';
  properties: {
    queryString: string;
    collapsePositions?: boolean;
    keepNull?: boolean;
    batchSize?: number;
    memoryLimit?: number;
    returnType?: 'string' | 'number' | 'bool';
  };
};

export type StopwordsState = StopwordsProperty & {
  type: 'stopwords';
  properties: {
    hex?: boolean;
  };
};

export type CollationState = LocaleProperty & {
  type: 'collation';
};

export type SegmentationState = CaseProperty & {
  type: 'segmentation';
  properties: {
    break?: 'all' | 'alpha' | 'graphic';
  };
};

export type ModelProperty = {
  properties: {
    model_location: string;
    top_k?: number; // [0, 2147483647]
  };
};

export type NearestNeighborsState = ModelProperty & {
  type: 'nearest_neighbors';
};

export type ClassificationState = ModelProperty & {
  type: 'classification';
  properties: {
    threshold?: number; // [0.0, 1.0]
  };
};

export type PipelineState = DelimiterState
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
  type: 'pipeline';
  properties: {
    pipeline: PipelineState[]
  };
};

export type GeoOptionsProperty = {
  properties: {
    options?: {
      maxCells?: number;
      minLevel?: number;
      maxLevel?: number;
    }
  }
};

export type GeoJsonState = GeoOptionsProperty & {
  type: 'geojson';
  properties: {
    type?: 'shape' | 'centroid' | 'point';
  };
};

export type GeoPointState = GeoOptionsProperty & {
  type: 'geopoint';
  properties: {
    latitude?: string[];
    longitude?: string[];
  };
};

export type AnalyzerTypeState = IdentityState
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
  | GeoPointState;

export type FormState = BaseFormState & AnalyzerTypeState;

const baseSchema = {
  properties: {
    name: {
      nullable: false,
      type: 'string',
      pattern: '^[a-zA-Z][a-zA-Z0-9-_]*$'
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
    }
  },
  additionalProperties: false
};

const localeSchema = {
  type: 'string',
  nullable: false,
  default: ''
};

const accentSchema = {
  type: 'boolean',
  nullable: false
};

const caseSchema = {
  type: 'string',
  nullable: false,
  enum: ['lower', 'upper', 'none']
};

const geoOptionsSchema = {
  type: 'object',
  nullable: false,
  properties: {
    maxCells: {
      type: 'integer',
      minimum: 0,
      nullable: false
    },
    minLevel: {
      type: 'integer',
      minimum: 0,
      maximum: { $data: '1/maxLevel' },
      nullable: false
    },
    maxLevel: {
      type: 'integer',
      minimum: { $data: '1/minLevel' },
      nullable: false
    }
  },
  additionalProperties: false
};

const mergeBase = partial(merge, _, baseSchema);

const identitySchema = mergeBase({
  properties: {
    'type': {
      const: 'identity'
    }
  },
  required: ['type']
});

const delimiterSchema = mergeBase({
  properties: {
    'type': {
      const: 'delimiter'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        delimiter: {
          type: 'string',
          nullable: false,
          default: ''
        }
      },
      required: ['delimiter'],
      additionalProperties: false,
      default: {
        delimiter: ''
      }
    }
  },
  required: ['type', 'properties']
});

const stemSchema = mergeBase({
  properties: {
    'type': {
      const: 'stem'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        locale: localeSchema
      },
      required: ['locale'],
      additionalProperties: false,
      default: {
        locale: ''
      }
    }
  },
  required: ['type', 'properties']
});

const normSchema = mergeBase({
  properties: {
    'type': {
      const: 'norm'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        locale: localeSchema,
        accent: accentSchema,
        case: caseSchema
      },
      required: ['locale'],
      additionalProperties: false,
      default: {
        locale: ''
      }
    }
  },
  required: ['type', 'properties']
});

const ngramSchema = mergeBase({
  properties: {
    'type': {
      const: 'ngram'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        min: {
          type: 'integer',
          nullable: false,
          minimum: 1,
          maximum: { $data: '1/max' },
          default: 2
        },
        max: {
          type: 'integer',
          nullable: false,
          minimum: { $data: '1/min' },
          default: 3
        },
        preserveOriginal: {
          type: 'boolean',
          nullable: false,
          default: false
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
        }
      },
      required: ['min', 'max', 'preserveOriginal'],
      additionalProperties: false,
      default: {
        min: 2,
        max: 3,
        preserveOriginal: false
      }
    }
  },
  required: ['type', 'properties']
});

const textSchema = mergeBase({
  properties: {
    'type': {
      const: 'text'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        locale: localeSchema,
        accent: accentSchema,
        case: caseSchema,
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
              maximum: { $data: '1/max' },
              minimum: 1
            },
            max: {
              type: 'integer',
              nullable: false,
              minimum: { $data: '1/min' }
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
        }
      },
      required: ['locale'],
      additionalProperties: false,
      default: {
        locale: ''
      }
    }
  },
  required: ['type', 'properties']
});

const aqlSchema = mergeBase({
  properties: {
    'type': {
      const: 'aql'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        queryString: {
          type: 'string',
          nullable: false,
          default: ''
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
        }
      },
      required: ['queryString'],
      additionalProperties: false,
      default: {
        queryString: ''
      }
    }
  },
  required: ['type', 'properties']
});

const stopwordsSchema = mergeBase({
  properties: {
    'type': {
      const: 'stopwords'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        stopwords: {
          type: 'array',
          nullable: false,
          items: {
            type: 'string'
          },
          default: []
        },
        hex: {
          type: 'boolean',
          nullable: false
        }
      },
      required: ['stopwords'],
      additionalProperties: false,
      default: {
        stopwords: []
      }
    }
  },
  required: ['type', 'properties']
});

const collationSchema = mergeBase({
  properties: {
    'type': {
      const: 'collation'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        locale: localeSchema
      },
      required: ['locale'],
      additionalProperties: false,
      default: {
        locale: ''
      }
    }
  },
  required: ['type', 'properties']
});

const segmentationSchema = mergeBase({
  properties: {
    'type': {
      const: 'segmentation'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        break: {
          type: 'string',
          nullable: false,
          enum: ['all', 'alpha', 'graphic']
        },
        case: caseSchema
      },
      additionalProperties: false,
      default: {}
    }
  },
  required: ['type', 'properties']
});

const nearestNeighborsSchema = mergeBase({
  properties: {
    'type': {
      const: 'nearest_neighbors'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        model_location: {
          type: 'string',
          nullable: false
        },
        top_k: {
          type: 'integer',
          minimum: 0,
          maximum: 2147483647,
          nullable: false
        }
      }
    },
    additionalProperties: false,
    default: {}
  },
  required: ['type', 'properties']
});

const classificationSchema = mergeBase({
  properties: {
    'type': {
      const: 'classification'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        model_location: {
          type: 'string',
          nullable: false
        },
        top_k: {
          type: 'integer',
          minimum: 0,
          maximum: 2147483647,
          nullable: false
        },
        threshold: {
          type: 'number',
          nullable: false,
          minimum: 0,
          maximum: 1,
          default: 0
        }
      }
    },
    additionalProperties: false,
    default: {}
  },
  required: ['type', 'properties']
});

const pipelineSchema = mergeBase({
  properties: {
    type: {
      const: 'pipeline'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        pipeline: {
          type: 'array',
          nullable: false,
          items: {
            type: "object",
            discriminator: {
              propertyName: "type"
            },
            oneOf: [
              delimiterSchema,
              stemSchema,
              normSchema,
              ngramSchema,
              textSchema,
              aqlSchema,
              stopwordsSchema,
              collationSchema,
              segmentationSchema,
              nearestNeighborsSchema,
              classificationSchema
            ],
            errorMessage: {
              discriminator: '/type should be one of "delimiter", "stem", "norm", "ngram", "text", "aql",' +
                ' "stopwords", "collation", "segmentation"'
            }
          },
          default: []
        }
      },
      additionalProperties: false,
      required: ['pipeline'],
      default: {
        pipeline: []
      }
    }
  },
  required: ['type', 'properties']
});

const geojsonSchema = mergeBase({
  properties: {
    type: {
      const: 'geojson'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        type: {
          nullable: false,
          type: 'string',
          enum: ['shape', 'centroid', 'point']
        },
        options: geoOptionsSchema
      },
      additionalProperties: false,
      default: {}
    }
  },
  required: ['type', 'properties']
});

const geopointSchema = mergeBase({
  properties: {
    type: {
      const: 'geopoint'
    },
    'properties': {
      type: 'object',
      nullable: false,
      properties: {
        latitude: {
          type: 'array',
          nullable: false,
          items: {
            type: 'string'
          }
        },
        longitude: {
          type: 'array',
          nullable: false,
          items: {
            type: 'string'
          }
        },
        options: geoOptionsSchema
      },
      additionalProperties: false,
      default: {}
    }
  },
  required: ['type', 'properties']
});

export const formSchema: JSONSchemaType<FormState> = {
  type: 'object',
  discriminator: {
    propertyName: 'type'
  },
  oneOf: [
    identitySchema,
    delimiterSchema,
    stemSchema,
    normSchema,
    ngramSchema,
    textSchema,
    aqlSchema,
    stopwordsSchema,
    collationSchema,
    segmentationSchema,
    nearestNeighborsSchema,
    classificationSchema,
    pipelineSchema,
    geojsonSchema,
    geopointSchema
  ],
  errorMessage: {
    discriminator: '/type should be one of "identity", "delimiter", "stem", "norm", "ngram", "text", "aql",' +
      ' "stopwords", "collation", "segmentation", "pipeline", "geojson", "geopoint"'
  },
  required: ['name', 'features']
};

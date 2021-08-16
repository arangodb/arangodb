import Ajv, { JSONSchemaType } from 'ajv';
import { Dispatch } from 'React';
import _, { merge, partial } from 'lodash';

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

type CaseProperty = 'lower' | 'upper' | 'none';
export type Feature = 'frequency' | 'norm' | 'position';
type Features = Feature[];
type Int = number & { __int__: void };
export type GeoOptions = {
  maxCells?: Int;
  minLevel?: Int;
  maxLevel?: Int;
};

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

export type StemState = {
  type: 'stem';
  properties: {
    locale: string;
  };
};

export type CaseState = {
  properties: {
    case?: CaseProperty;
  };
};

export type NormState = CaseState & {
  type: 'norm';
  properties: {
    locale: string;
    accent?: boolean;
  };
};

export type NGramState = {
  type: 'ngram';
  properties: {
    min: Int;
    max: Int;
    preserveOriginal: boolean;
    startMarker?: string;
    endMarker?: string;
    streamType?: 'binary' | 'utf8';
  };
};

export type TextState = CaseState & {
  type: 'text';
  properties: {
    locale: string;
    accent?: boolean;
    stemming?: boolean;
    edgeNgram?: {
      max?: Int;
      min?: Int;
      preserveOriginal?: boolean;
    };
    stopwords?: string[];
    stopwordsPath?: string;
  };
};

export type AqlState = {
  type: 'aql';
  properties: {
    queryString: string;
    collapsePositions?: boolean;
    keepNull?: boolean;
    batchSize?: Int;
    memoryLimit?: Int;
    returnType?: 'string' | 'number' | 'bool';
  };
};

export type GeoOptionsState = {
  properties: {
    options?: GeoOptions;
  };
};

export type GeoJsonState = GeoOptionsState & {
  type: 'geojson';
  properties: {
    type?: 'shape' | 'centroid' | 'point';
  };
};

export type GeoPointState = GeoOptionsState & {
  type: 'geopoint';
  properties: {
    latitude?: string[];
    longitude?: string[];
  };
};

type PipelineState = IdentityState
  | DelimiterState
  | StemState
  | NormState
  | NGramState
  | TextState
  | AqlState;
export type PipelineStates = {
  type: 'pipeline';
  properties: PipelineState[];
};

type AnalyzerTypeState = IdentityState
  | DelimiterState
  | StemState
  | NormState
  | NGramState
  | TextState
  | AqlState
  | GeoJsonState
  | GeoPointState
  | PipelineStates;

export type FormState = BaseFormState & AnalyzerTypeState;

const baseSchema = {
  properties: {
    name: {
      nullable: false,
      type: 'string',
      default: ''
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
      },
      default: []
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
    type: {
      const: 'identity'
    }
  },
  required: ['type']
});

const delimiterSchema = mergeBase({
  properties: {
    type: {
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
    type: {
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
    type: {
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
    type: {
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
    type: {
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
    type: {
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

const pipelineSchema = mergeBase({
  properties: {
    type: {
      const: 'pipeline'
    },
    'properties': {
      type: 'array',
      nullable: false,
      items: {
        discriminator: {
          propertyName: "type"
        },
        oneOf: [
          identitySchema,
          delimiterSchema,
          stemSchema,
          normSchema,
          ngramSchema,
          textSchema,
          aqlSchema
        ]
      },
      default: []
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
    geojsonSchema,
    geopointSchema,
    pipelineSchema
  ],
  required: ['name', 'features']
};

const ajv = new Ajv({
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
export const validateAndFix = ajv.compile(formSchema);

export type State = {
  formState: FormState;
  formCache: object;
  show: boolean;
  showJsonForm: boolean;
  lockJsonForm: boolean;
  renderKey: string;
};

export type DispatchArgs = {
  type: string;
  field?: {
    path: string;
    value?: any;
  };
  formState?: FormState;
};

export type FormProps = {
  state: State;
  dispatch: Dispatch<DispatchArgs>;
};

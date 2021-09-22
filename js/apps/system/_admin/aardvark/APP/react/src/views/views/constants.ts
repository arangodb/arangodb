import { JSONSchemaType } from 'ajv';
import { Dispatch } from 'react';
import _, { merge, partial } from 'lodash';
import { Int } from "../../utils/constants";

type Compression = 'lz4' | 'none';

type StoredValue = {
  fields: string[];
  compression: Compression;
};

type BytesAccumConsolidationPolicy = {
  type: 'bytes_accum';
  threshold?: number;
};

type TierConsolidationPolicy = {
  type: 'tier';
  segmentsMin?: Int;
  segmentsMax?: Int;
  segmentsBytesMax?: Int;
  segmentsBytesFloor?: Int;
};

type ConsolidationPolicy = BytesAccumConsolidationPolicy | TierConsolidationPolicy;

type ViewProperties = {
  primarySort?: string[];
  primarySortCompression?: Compression;
  storedValues?: StoredValue[];
  cleanupIntervalStep?: Int;
  commitIntervalMsec?: Int;
  consolidationIntervalMsec?: Int;
  writebufferIdle?: Int;
  writebufferActive?: Int;
  writebufferSizeMax?: Int;
  consolidationPolicy?: ConsolidationPolicy;
};

type LinkProperties = {
  analyzers?: string[];
  fields?: {
    [attributeName: string]: LinkProperties;
  };
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  storeValues?: 'none' | 'id';
  inBackground?: boolean;
};

type BaseFormState = {
  name: string;
  type: 'arangosearch';
  links?: {
    [key: string]: LinkProperties | null;
  };
};

export type FormState = BaseFormState & ViewProperties;

const baseSchema = {
  properties: {
    name: {
      nullable: false,
      type: 'string'
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

const stopwordsSchema = mergeBase({
  properties: {
    type: {
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
    type: {
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
    type: {
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
              segmentationSchema
            ]
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
    pipelineSchema,
    geojsonSchema,
    geopointSchema
  ],
  required: ['name', 'features']
};

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
  basePath?: string;
  formState?: FormState;
};

export type FormProps = {
  formState: BaseFormState | FormState;
  dispatch: Dispatch<DispatchArgs>;
  disabled?: boolean;
};

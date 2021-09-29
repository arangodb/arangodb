import { JSONSchemaType } from 'ajv';
import { Dispatch } from 'react';

type Compression = 'lz4' | 'none';

type Direction = 'asc' | 'desc';

type StoredValue = {
  fields: string[];
  compression: Compression;
};

type PrimarySort = {
  field: string;
  direction: Direction;
};

type BytesAccumConsolidationPolicy = {
  type: 'bytes_accum';
  threshold?: number;
};

type TierConsolidationPolicy = {
  type: 'tier';
  segmentsMin?: number;
  segmentsMax?: number;
  segmentsBytesMax?: number;
  segmentsBytesFloor?: number;
};

type ConsolidationPolicy = BytesAccumConsolidationPolicy | TierConsolidationPolicy;

type ViewProperties = {
  primarySort?: PrimarySort[];
  primarySortCompression?: Compression;
  storedValues?: StoredValue[];
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
  writebufferIdle?: number;
  writebufferActive?: number;
  writebufferSizeMax?: number;
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
    [collectionName: string]: LinkProperties | null;
  };
};

export type FormState = BaseFormState & ViewProperties;

export const linksSchema = {
  $id: 'https://arangodb.com/schemas/views/linkProperties.json',
  $recursiveAnchor: true,
  type: 'object',
  properties: {
    analyzers: {
      type: 'array',
      nullable: false,
      items: {
        type: 'string',
        pattern: '^([a-zA-Z0-9-_]+::)?[a-zA-Z][a-zA-Z0-9-_]*$'
      }
    },
    fields: {
      type: 'object',
      nullable: false,
      patternProperties: {
        '.+': {
          $recursiveRef: '#'
        }
      }
    },
    includeAllFields: {
      type: 'boolean',
      nullable: false
    },
    trackListPositions: {
      type: 'boolean',
      nullable: false
    },
    storeValues: {
      type: 'string',
      nullable: false,
      enum: ['none', 'id']
    },
    inBackground: {
      type: 'boolean',
      nullable: false
    }
  },
  additionalProperties: false
};

export const formSchema: JSONSchemaType<FormState> = {
  $id: 'https://arangodb.com/schemas/views/views.json',
  type: 'object',
  properties: {
    name: {
      nullable: false,
      type: 'string',
      pattern: '^[a-zA-Z][a-zA-Z0-9-_]*$',
      default: ''
    },
    type: {
      type: 'string',
      const: 'arangosearch'
    },
    links: {
      nullable: true,
      type: 'object',
      patternProperties: {
        '^[a-zA-Z0-9-_]+$': {
          type: 'object',
          $ref: 'linkProperties.json'
        }
      },
      required: []
    },
    primarySort: {
      type: 'array',
      nullable: true,
      items: {
        type: 'object',
        nullable: false,
        properties: {
          field: {
            type: 'string',
            nullable: false
          },
          direction: {
            type: 'string',
            enum: ['asc', 'desc'],
            default: 'asc'
          }
        },
        default: {
          direction: 'asc'
        },
        required: ['field', 'direction'],
        additionalProperties: false
      }
    },
    primarySortCompression: {
      type: 'string',
      nullable: true,
      enum: ['lz4', 'none'],
      default: 'lz4'
    },
    storedValues: {
      type: 'array',
      nullable: true,
      items: {
        type: 'object',
        nullable: false,
        properties: {
          fields: {
            type: 'array',
            nullable: false,
            items: {
              type: 'string'
            }
          },
          compression: {
            type: 'string',
            enum: ['lz4', 'none'],
            default: 'lz4'
          }
        },
        required: ['fields', 'compression'],
        default: {
          compression: 'lz4'
        },
        additionalProperties: false
      }
    },
    cleanupIntervalStep: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 2
    },
    commitIntervalMsec: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 1000
    },
    consolidationIntervalMsec: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 1000
    },
    writebufferIdle: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 64
    },
    writebufferActive: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 0
    },
    writebufferSizeMax: {
      type: 'integer',
      nullable: true,
      minimum: 0,
      default: 33554432
    },
    consolidationPolicy: {
      type: 'object',
      nullable: true,
      discriminator: {
        propertyName: "type"
      },
      oneOf: [
        {
          properties: {
            type: {
              const: 'bytes_accum'
            },
            threshold: {
              type: 'number',
              nullable: false,
              minimum: 0,
              maximum: 1,
              default: 0.1
            }
          },
          additionalProperties: false
        },
        {
          properties: {
            type: {
              const: 'tier'
            },
            segmentsMin: {
              type: 'integer',
              nullable: false,
              minimum: 0,
              maximum: {
                $data: '1/segmentsMax'
              },
              default: 1
            },
            segmentsMax: {
              type: 'integer',
              nullable: false,
              minimum: {
                $data: '1/segmentsMin'
              },
              default: 10
            },
            segmentsBytesMax: {
              type: 'integer',
              nullable: false,
              minimum: 0,
              default: 5368709120
            },
            segmentsBytesFloor: {
              type: 'integer',
              nullable: false,
              minimum: 0,
              default: 2097152
            }
          },
          additionalProperties: false
        }
      ],
      default: {
        type: 'tier',
        segmentsMin: 1,
        segmentsMax: 10,
        segmentsBytesMax: 5368709120,
        segmentsBytesFloor: 2097152
      },
      required: ['type']
    }
  },
  required: ['name', 'type'],
  additionalProperties: false
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
  formState: FormState;
  dispatch: Dispatch<DispatchArgs>;
  disabled?: boolean;
};

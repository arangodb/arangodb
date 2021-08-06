export const typeNameMap = {
  identity: 'Identity',
  delimiter: 'Delimiter',
  stem: 'Stem',
  norm: 'Norm',
  ngram: 'N-Gram',
  text: 'Text'
};

export interface FormProps {
  formState: { [key: string]: any };
  updateFormField: (field: string, value: any) => void;
}

type AnalyzerType = 'identity' | 'delimiter' | 'stem' | 'norm' | 'ngram' | 'text';
type AnalyzerNormCase = 'lower' | 'upper' | 'none';
type AnalyzerFeature = 'frequency' | 'norm' | 'position';
type AnalyzerFeatures = AnalyzerFeature[];
type Int = number & { __int__: void };

interface AnalyzerProperties {
  delimiter?: string;
  locale?: string;
  case?: AnalyzerNormCase;
  max?: Int;
  min?: Int;
  preserveOriginal?: boolean;
  accent?: boolean;
  stemming?: boolean;
  stopwords?: string[];
  stopwordsPath?: string;
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
      enum: ['identity', 'delimiter', 'stem', 'norm', 'ngram', 'text']
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
        accent: {
          type: 'boolean',
          nullable: false
        },
        stemming: {
          type: 'boolean',
          nullable: false
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
      }
    }
  },
  required: ['name', 'type'],
  additionalProperties: false
};

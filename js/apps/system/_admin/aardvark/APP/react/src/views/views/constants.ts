import { JSONSchemaType } from 'ajv';
import { Dispatch } from 'react';
import { Int } from "../../utils/constants";

type Compression = 'lz4' | 'none';

type Direction = 'asc' | 'desc';

type StoredValue = {
  fields: string[];
  compression: Compression;
};

type PrimarySortOrder = {
  field: string;
  direction: Direction;
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
  primarySort?: PrimarySortOrder[];
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
    [collectionName: string]: LinkProperties | null;
  };
};

export type FormState = BaseFormState & ViewProperties;

const linksSchema = {

};

const baseSchema = {
  properties: {
    name: {
      nullable: false,
      type: 'string',
      pattern: '^[a-zA-Z][a-zA-Z0-9-_]*$'
    },
    type: {
      const: 'arangosearch'
    },
    links: {
      nullable: false,
      type: 'object',
      properties: linksSchema
    }
  },
  additionalProperties: false
};

export const formSchema: JSONSchemaType<FormState> = {
  type: 'object',
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

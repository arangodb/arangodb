import { SearchViewType } from "./viewsList/useViewsList";

type Compression = "lz4" | "none";

type PrimarySortType = {
  field: string;
  asc: boolean;
};

type StoredValue = {
  fields: string[];
  compression: Compression;
};

type StoredValues = {
  storedValues?: StoredValue[];
};

type PrimarySort = {
  primarySort?: PrimarySortType[];
};

type BytesAccumConsolidationPolicy = {
  consolidationPolicy?: {
    type: "bytes_accum";
    threshold?: number;
  };
};

type TierConsolidationPolicy = {
  consolidationPolicy?: {
    type: "tier";
    segmentsMin?: number;
    segmentsMax?: number;
    segmentsBytesMax?: number;
    segmentsBytesFloor?: number;
    minScore?: number;
  };
};

type ConsolidationPolicy =
  | BytesAccumConsolidationPolicy
  | TierConsolidationPolicy;

type AdvancedProperties = {
  primarySortCompression?: Compression;
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
  writebufferIdle?: number;
  writebufferActive?: number;
  writebufferSizeMax?: number;
};

type LinkProperties = {
  analyzers?: string[];
  fields?: {
    [attributeName: string]: LinkProperties;
  };
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  storeValues?: "none" | "id";
  inBackground?: boolean;
  cache?: boolean;
};

export type ArangoSearchViewPropertiesType = SearchViewType &
  PrimarySort &
  StoredValues &
  ConsolidationPolicy &
  AdvancedProperties & {
    links?: {
      [collectionName: string]: LinkProperties | null;
    };
  };

export interface SearchAliasViewPropertiesType extends SearchViewType {
  indexes: Array<{ collection: string; index: string }>;
}

export type ViewPropertiesType =
  | ArangoSearchViewPropertiesType
  | SearchAliasViewPropertiesType;

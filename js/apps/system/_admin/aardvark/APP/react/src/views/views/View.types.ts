export interface ViewDescription {
  /**
   * A globally unique identifier for this View.
   */
  globallyUniqueId: string;
  /**
   * An identifier for this View.
   */
  id: string;
  /**
   * Name of the View.
   */
  name: string;
  /**
   * Type of the View.
   */
  type: "arangosearch" | "search-alias";
}

type Compression = "lz4" | "none";

export type PrimarySortType = {
  field: string;
  asc: boolean;
};

export type StoredValueType = {
  fields: string[];
  compression: Compression;
  cache?: boolean;
};

type StoredValues = {
  storedValues?: StoredValueType[];
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
  primaryKeyCache?: boolean;
  primarySortCache?: boolean;
  primarySortCompression?: Compression;
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
  writebufferIdle?: number;
  writebufferActive?: number;
  writebufferSizeMax?: number;
};

export type LinkProperties = {
  analyzers?: string[];
  fields?: {
    [attributeName: string]: LinkProperties;
  };
  nested?: {
    [attributeName: string]: LinkProperties;
  };
  includeAllFields?: boolean;
  trackListPositions?: boolean;
  storeValues?: "none" | "id";
  inBackground?: boolean;
  cache?: boolean;
};

export type LinksType = {
  [collectionName: string]: LinkProperties | null;
};
export type ArangoSearchViewPropertiesType = ViewDescription &
  PrimarySort &
  StoredValues &
  ConsolidationPolicy &
  AdvancedProperties & {
    links?: LinksType;
  };

export interface SearchAliasViewPropertiesType extends ViewDescription {
  indexes: Array<{ collection: string; index: string }>;
}

export type ViewPropertiesType =
  | ArangoSearchViewPropertiesType
  | SearchAliasViewPropertiesType;

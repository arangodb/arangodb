import { ArangojsResponse } from "arangojs/lib/request";
import { useEffect, useState } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export type StoredValue = {
  fields: string[];
  compression: "lz4" | "none";
};
export type IndexType =
  | "primary"
  | "fulltext"
  | "edge"
  | "persistent"
  | "ttl"
  | "geo"
  | "zkd"
  | "hash"
  | "inverted";
export type IndexRowType = {
  fields: string[] | { [key: string]: string }[];
  id: string;
  name: string;
  sparse: boolean;
  type: IndexType;
  unique: boolean;
  selectivityEstimate?: number;
  storedValues?: string[] | StoredValue[];
  minLength?: number;
  cacheEnabled?: boolean;
  deduplicate: boolean;
} & InvertedIndexExtraFields;

type InvertedIndexExtraFields = {
  analyzer?: string;
  cleanupIntervalStep?: number;
  commitIntervalMsec?: number;
  consolidationIntervalMsec?: number;
};

interface IndicesResponse extends ArangojsResponse {
  body: { indexes: Array<IndexRowType> };
}

export const useFetchIndices = ({
  collectionName
}: {
  collectionName: string;
}) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { data, ...rest } = useSWR<IndicesResponse>(
    `/index/?collection=${encodedCollectionName}`,
    () => {
      return getApiRouteForCurrentDB().get(
        `/index/`,
        `collection=${encodedCollectionName}`
      ) as any as Promise<IndicesResponse>;
    }
  );
  const result = data?.body.indexes;
  const [indices, setIndexList] = useState<IndexRowType[] | undefined>(result);

  useEffect(() => {
    setIndexList(result);
  }, [result]);
  return { indices, ...rest };
};

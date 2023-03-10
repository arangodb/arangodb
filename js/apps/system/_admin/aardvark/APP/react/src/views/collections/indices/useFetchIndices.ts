import { ArangojsResponse } from "arangojs/lib/request";
import { useEffect, useState } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export type IndexType = {
  fields: string[];
  id: string;
  name: string;
  sparse: boolean;
  type: "primary" | "fulltext";
  unique: boolean;
  selectivityEstimate?: number;
  storedValues?: string[];
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
  body: { indexes: Array<IndexType> };
}

/**
 * 
 * 
 *  $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/index/?collection=' + encodeURIComponent(this.get('name'))),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data, self.get('id'));
        },
        error: function (data) {
          callback(true, data, self.get('id'));
        }
      });
 */
export const useFetchIndices = ({
  collectionName
}: {
  collectionName: string;
}) => {
  const { data, ...rest } = useSWR<IndicesResponse>(
    `/index/?collection=${collectionName}`,
    () => {
      return (getApiRouteForCurrentDB().get(
        `/index/`,
        `collection=${collectionName}`
      ) as any) as Promise<IndicesResponse>;
    }
  );
  const result = data?.body.indexes;
  const [indices, setIndexList] = useState<IndexType[] | undefined>(result);

  useEffect(() => {
    setIndexList(result);
  }, [result]);
  return { indices, ...rest };
};

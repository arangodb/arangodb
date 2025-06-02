import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { CollectionIndex } from "./CollectionIndex.types";

interface EngineResponse extends ArangojsResponse {
  parsedBody: {
    supports: {
      indexes: string[];
      aliases?: {
        indexes: { [key: string]: string };
      };
    };
  };
}

export const useSupportedIndexTypes = () => {
  const { data, ...rest } = useSWR<EngineResponse>(`/engine`, path => {
    return getApiRouteForCurrentDB().get(
      path
    ) as any as Promise<EngineResponse>;
  });

  const indexes = data?.parsedBody.supports.indexes;
  const aliases = data?.parsedBody.supports.aliases?.indexes || {};
  const supported = indexes?.filter(indexType => {
    return !aliases.hasOwnProperty(indexType);
  });
  
  // Add vector index conditionally based on support
  const allIndexOptions = [
    ...indexTypeOptions,
    ...(supported?.includes("vector") ? [{
      label: "Vector index (beta)",
      value: "vector" as const
    }] : [])
  ];

  const options = allIndexOptions.filter(option =>
    supported?.includes(option.value)
  );
  return { supported, indexTypeOptions: options, ...rest };
};

const indexTypeOptions: {
  label: string;
  value: CollectionIndex["type"] | "fulltext" | "vector";
}[] = [
  {
    label: "Persistent Index",
    value: "persistent"
  },
  {
    label: "Geo Index",
    value: "geo"
  },
  {
    label: "Fulltext Index",
    value: "fulltext"
  },
  {
    label: "TTL Index",
    value: "ttl"
  },
  {
    label: "MDI Index",
    value: "mdi"
  },
  {
    label: "MDI Index (Prefixed)",
    value: "mdi-prefixed"
  },
  {
    label: "Inverted Index",
    value: "inverted"
  }
];

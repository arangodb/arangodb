import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { CollectionIndex } from "./CollectionIndex.types";

interface EngineResponse extends ArangojsResponse {
  body: {
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

  const indexes = data?.body.supports.indexes;
  const aliases = data?.body.supports.aliases?.indexes || {};
  const supported = indexes?.filter(indexType => {
    return !aliases.hasOwnProperty(indexType);
  });
  const options = indexTypeOptions.filter(option =>
    supported?.includes(option.value)
  );
  return { supported, indexTypeOptions: options, ...rest };
};

const indexTypeOptions: {
  label: string;
  value: CollectionIndex["type"];
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

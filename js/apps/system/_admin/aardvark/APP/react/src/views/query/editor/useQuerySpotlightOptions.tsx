import { CollectionMetadata, CollectionType } from "arangojs/collection";
import useSWR from "swr";
import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../../utils/arangoClient";
const AQL_KEYWORDS = [
  "for",
  "return",
  "filter",
  "search",
  "sort",
  "limit",
  "let",
  "collect",
  "asc",
  "desc",
  "in",
  "into",
  "insert",
  "update",
  "remove",
  "replace",
  "upsert",
  "options",
  "with",
  "and",
  "or",
  "not",
  "distinct",
  "graph",
  "shortest_path",
  "k_shortest_paths",
  "all_shortest_paths",
  "k_paths",
  "outbound",
  "inbound",
  "any",
  "all",
  "none",
  "aggregate",
  "like",
  "count"
];

const AQL_KEYWORDS_OPTIONS = AQL_KEYWORDS.map(keyword => ({
  label: keyword.toUpperCase(),
  value: keyword.toUpperCase()
}));

const useBuiltinFunctionOptions = () => {
  const fetchBuiltins = async () => {
    const route = getApiRouteForCurrentDB();
    const response = await route.get("/aql-builtin");
    return response.body;
  };
  const { data, isLoading: isFunctionsLoading } = useSWR<{ functions: any[] }>(
    "/aql-builtin",
    fetchBuiltins
  );
  const builtinFunctions = data?.functions.map(func => ({
    label: func.name,
    value: func.name
  }));
  return { builtinFunctions, isFunctionsLoading };
};

const useCollectionOptions = () => {
  const fetchCollections = async () => {
    const currentDB = getCurrentDB();
    const data = await currentDB.listCollections(false);
    return data;
  };
  const { data, isLoading: isCollectionsLoading } = useSWR<
    CollectionMetadata[]
  >(`/collections`, fetchCollections);
  const edgeCollectionOptions = data
    ?.filter(collection => {
      return (
        collection.type === CollectionType.EDGE_COLLECTION &&
        !collection.isSystem
      );
    })
    .map(collection => ({
      label: collection.name,
      value: collection.name
    }));
  const documentCollectionOptions = data
    ?.filter(collection => {
      return (
        collection.type === CollectionType.DOCUMENT_COLLECTION &&
        !collection.isSystem
      );
    })
    .map(collection => ({
      label: collection.name,
      value: collection.name
    }));
  const systemCollectionOptions = data
    ?.filter(collection => {
      return collection.isSystem;
    })
    .map(collection => ({
      label: collection.name,
      value: collection.name
    }));
  return {
    edgeCollectionOptions,
    documentCollectionOptions,
    systemCollectionOptions,
    isCollectionsLoading
  };
};
export const useQuerySpotlightOptions = () => {
  const { builtinFunctions, isFunctionsLoading } = useBuiltinFunctionOptions();
  const {
    edgeCollectionOptions,
    documentCollectionOptions,
    systemCollectionOptions,
    isCollectionsLoading
  } = useCollectionOptions();
  const groupedOptions = [
    {
      label: "Functions",
      options: builtinFunctions || []
    },
    { label: "Keywords", options: AQL_KEYWORDS_OPTIONS },
    { label: "Edges", options: edgeCollectionOptions || [] },
    { label: "Documents", options: documentCollectionOptions || [] },
    { label: "System", options: systemCollectionOptions || [] }
  ];
  return {
    groupedOptions,
    isLoading: isFunctionsLoading || isCollectionsLoading
  };
};

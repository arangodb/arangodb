import { CollectionMetadata, CollectionType } from "arangojs/collection";
import useSWR from "swr";
import { OptionType } from "../../../components/select/SelectBase";
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
    return response.parsedBody;
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
  const result = data?.reduce(
    (acc, collection) => {
      if (collection.type === CollectionType.EDGE_COLLECTION) {
        acc.edgeCollectionOptions.push({
          label: collection.name,
          value: collection.name
        });
      } else if (collection.type === CollectionType.DOCUMENT_COLLECTION) {
        acc.documentCollectionOptions.push({
          label: collection.name,
          value: collection.name
        });
      } else if (collection.isSystem) {
        acc.systemCollectionOptions.push({
          label: collection.name,
          value: collection.name
        });
      }
      return acc;
    },
    {
      edgeCollectionOptions: [] as OptionType[],
      documentCollectionOptions: [] as OptionType[],
      systemCollectionOptions: [] as OptionType[]
    }
  );
  return {
    edgeCollectionOptions: result?.edgeCollectionOptions,
    documentCollectionOptions: result?.documentCollectionOptions,
    systemCollectionOptions: result?.systemCollectionOptions,
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

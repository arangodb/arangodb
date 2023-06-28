import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../../utils/arangoClient";
import useSWR from "swr";
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
  const { data } = useSWR<{ functions: any[] }>("/aql-builtin", fetchBuiltins);
  const builtinFunctions = data?.functions.map(func => ({
    label: func.name,
    value: func.name
  }));
  return { builtinFunctions };
};
export const useQuerySpotlightOptions = () => {
  const { builtinFunctions } = useBuiltinFunctionOptions();
  const groupedOptions = [
    {
      label: "Functions",
      options: builtinFunctions || []
    },
    { label: "keywords", options: AQL_KEYWORDS_OPTIONS }
  ];
  return groupedOptions;
};

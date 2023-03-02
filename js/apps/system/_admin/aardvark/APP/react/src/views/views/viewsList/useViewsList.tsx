import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export type SearchViewType = {
  globallyUniqueId: string;
  id: string;
  name: string;
  type: "search-alias" | "arangosearch";
};
interface ViewsListResponse extends ArangojsResponse {
  body: { result: Array<SearchViewType> };
}

export const useViewsList = () => {
  const { data } = useSWR<ViewsListResponse>("/view", path => {
    return (getApiRouteForCurrentDB().get(path) as any) as ViewsListResponse;
  });
  console.log({ data, result: data && data.body.result });
  return { viewsList: data && data.body && data.body.result };
};

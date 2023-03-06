import { ArangojsResponse } from "arangojs/lib/request";
import { useEffect, useState } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export type SearchViewType = {
  globallyUniqueId: string;
  id: string;
  name: string;
  type: "search-alias" | "arangosearch";
  isLocked?: boolean;
};
interface ViewsListResponse extends ArangojsResponse {
  body: { result: Array<SearchViewType> };
}

export const useViewsList = () => {
  const { data, ...rest } = useSWR<ViewsListResponse>("/view", path => {
    return (getApiRouteForCurrentDB().get(path) as any) as Promise<
      ViewsListResponse
    >;
  });
  const result = data?.body?.result;

  const [updatedViewsList, setUpdatedViewsList] = useState<
    SearchViewType[] | undefined
  >(result);

  useEffect(() => {
    const checkProgress = () => {
      const callback = (_: any, lockedViews: { collection: string }[]) => {
        const newViewsList = result?.map(view => {
          if (
            lockedViews.find(lockedView => lockedView.collection === view.name)
          ) {
            return { ...view, isLocked: true };
          }
          return { ...view, isLocked: false };
        });
        setUpdatedViewsList(newViewsList);
      };
      if (!window.frontendConfig.ldapEnabled) {
        window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs(
          "view",
          callback
        );
      }
    };
    checkProgress();
    const timeout = window.setTimeout(() => {
      checkProgress();
    }, 10000);
    return () => clearTimeout(timeout);
  }, [result]);
  return { viewsList: updatedViewsList, ...rest };
};

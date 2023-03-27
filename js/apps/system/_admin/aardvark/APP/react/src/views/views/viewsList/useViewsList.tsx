import { ArangojsResponse } from "arangojs/lib/request";
import { useEffect, useState } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export type SearchViewType = {
  globallyUniqueId: string;
  id: string;
  name: string;
  type: "search-alias" | "arangosearch";
};

export interface SearchViewTypeWithLock extends SearchViewType {
  isLocked?: boolean;
}
interface ViewsListResponse extends ArangojsResponse {
  body: { result: Array<SearchViewTypeWithLock> };
}

export const useViewsList = ({
  searchValue,
  sortDescending
}: {
  searchValue: string;
  sortDescending: boolean;
}) => {
  const { data, ...rest } = useSWR<ViewsListResponse>("/view", path => {
    return (getApiRouteForCurrentDB().get(path) as any) as Promise<
      ViewsListResponse
    >;
  });
  const result = data?.body?.result;

  const [viewsList, setViewsList] = useState<SearchViewType[] | undefined>(
    result
  );

  const checkProgress = () => {
    const callback = (_: any, lockedViews?: { collection: string }[]) => {
      const newViewsList = result?.map(view => {
        if (
          lockedViews?.find(lockedView => lockedView.collection === view.name)
        ) {
          return { ...view, isLocked: true };
        }
        return { ...view, isLocked: false };
      });
      if (newViewsList) {
        setViewsList(
          filterAndSortViews({
            viewsList: newViewsList,
            searchValue,
            sortDescending
          })
        );
      }
    };
    if (!window.frontendConfig.ldapEnabled) {
      window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs("view", callback);
    }
  };

  useEffect(() => {
    let interval: number;
    if (result) {
      checkProgress();
      interval = window.setInterval(() => {
        checkProgress();
      }, 10000);
    }
    return () => window.clearInterval(interval);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [result]);

  useEffect(() => {
    setViewsList(
      filterAndSortViews({ viewsList: result, searchValue, sortDescending })
    );
  }, [searchValue, result, sortDescending]);
  return { viewsList, ...rest };
};

const filterAndSortViews = ({
  viewsList,
  searchValue,
  sortDescending
}: {
  viewsList: Array<SearchViewType> | undefined;
  searchValue: string;
  sortDescending: boolean;
}) => {
  return viewsList
    ?.filter(view => {
      return view.name.includes(searchValue);
    })
    .sort((a, b) => {
      if (a.name > b.name) {
        return sortDescending ? -1 : 1;
      }
      return sortDescending ? 1 : -1;
    });
};

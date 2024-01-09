import { ArangojsResponse } from "arangojs/lib/request";
import { useEffect, useState } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { ViewDescription } from "./View.types";

export interface LockableViewDescription extends ViewDescription {
  isLocked?: boolean;
}
interface ListViewsResponse extends ArangojsResponse {
  body: { result: Array<LockableViewDescription> };
}

export const useFetchViews = () => {
  const { data, ...rest } = useSWR<ListViewsResponse>("/view", path => {
    return getApiRouteForCurrentDB().get(
      path
    ) as any as Promise<ListViewsResponse>;
  });
  const result = data?.body?.result;

  const [views, setViews] = useState<LockableViewDescription[] | undefined>(
    result
  );

  const checkProgress = () => {
    const callback = (_: any, lockedViews?: { collection: string }[]) => {
      const newViews = result?.map(view => {
        if (
          lockedViews?.find(lockedView => lockedView.collection === view.name)
        ) {
          return { ...view, isLocked: true };
        }
        return { ...view, isLocked: false };
      });
      if (newViews) {
        setViews(newViews);
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
    setViews(result);
  }, [result]);
  return { views, ...rest };
};

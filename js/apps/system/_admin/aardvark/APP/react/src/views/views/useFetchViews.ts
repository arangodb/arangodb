import { useEffect, useState } from "react";
import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { ViewDescription } from "./View.types";

export interface LockableViewDescription extends ViewDescription {
  isLocked?: boolean;
}

export const useFetchViews = () => {
  const { data: result, ...rest } = useSWR("/view", () =>
    getCurrentDB().listViews()
  );

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
    window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs("view", callback);
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

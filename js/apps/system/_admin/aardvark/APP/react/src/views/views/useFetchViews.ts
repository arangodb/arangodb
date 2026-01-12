import { useEffect, useState } from "react";
import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { syncFrontendJobs } from "../../utils/frontendJobs";
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

  const checkProgress = async () => {
    try {
      const pendingJobs = await syncFrontendJobs("view");
      const lockedViewNames = new Set(pendingJobs.map(job => job.collection));

      const newViews = result?.map(view => ({
        ...view,
        isLocked: lockedViewNames.has(view.name)
      }));

      if (newViews) {
        setViews(newViews);
      }
    } catch (error) {
      console.error("Failed to check view job progress:", error);
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

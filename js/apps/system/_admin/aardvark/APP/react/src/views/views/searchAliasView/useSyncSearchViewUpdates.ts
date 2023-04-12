import { useEffect } from "react";
import { useSWRConfig } from "swr";

export const useSyncSearchViewUpdates = ({ viewName }: { viewName: string }) => {
  const { mutate } = useSWRConfig();

  const checkState = function (
    error: boolean,
    jobs: { id: string; collection: string }[]
  ) {
    if (error) {
      window.arangoHelper.arangoError("Jobs", "Could not read pending jobs.");
    } else {
      const foundJob = jobs.find(locked => locked.collection === viewName);
      if (foundJob) {
        const path = `/view/${viewName}/properties`;
        mutate(path);
        window.arangoHelper.arangoNotification(
          "Success",
          `Updated View: ${viewName}`
        );
        window.arangoHelper.deleteAardvarkJob(foundJob.id);
      }
    }
  };
  useEffect(() => {
    window.arangoHelper.getAardvarkJobs(checkState);

    let interval = window.setInterval(() => {
      window.arangoHelper.getAardvarkJobs(checkState);
    }, 10000);
    return () => window.clearInterval(interval);
    // disabled because function creation can cause re-render, and this only needs to run on mount
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
};

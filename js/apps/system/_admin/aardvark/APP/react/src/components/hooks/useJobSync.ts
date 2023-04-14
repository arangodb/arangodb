import { useEffect } from "react";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";

export const useJobSync = ({
  onError,
  onSuccess,
  onQueue,
  jobCollectionName
}: {
  onError: (error: any) => void;
  onSuccess: () => void;
  onQueue: () => void;
  jobCollectionName: string;
}) => {
  const checkState = function (
    error: boolean,
    jobsList: { id: string; collection: string }[]
  ) {
    if (error) {
      window.arangoHelper.arangoError("Jobs", "Could not read pending jobs.");
    } else {
      const readJob = function (data: any, jobId: string) {
        if (data.statusCode === 204) {
          // job is still in quere or pending
          onQueue();
          return;
        }
        // this means the index is created, so we can delete the job and refetch
        onSuccess();
        window.arangoHelper.deleteAardvarkJob(jobId);
      };

      const onJobError = (error: any, jobId: string) => {
        const statusCode = error?.response?.body?.code;
        onError(error);
        if (statusCode === 404) {
          // delete non existing aardvark job
          window.arangoHelper.deleteAardvarkJob(jobId);
        } else if (statusCode === 400) {
          // index job failed -> print error
          // delete non existing aardvark job
          window.arangoHelper.deleteAardvarkJob(jobId);
        }
      };

      jobsList.forEach(job => {
        if (job.collection === jobCollectionName) {
          getApiRouteForCurrentDB()
            .put(`/job/${job.id}`)
            .then(data => readJob(data, job.id))
            .catch(error => onJobError(error, job.id));
        }
      });
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

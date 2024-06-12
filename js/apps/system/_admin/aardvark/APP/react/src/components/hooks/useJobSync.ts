import { useInterval } from "@chakra-ui/react";
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
        if (data.status === 204) {
          // job is still in queue or pending
          onQueue();
          return;
        }
        // this means the job is complete so we can delete it
        window.arangoHelper.deleteAardvarkJob(jobId);
        onSuccess();
      };

      const onJobError = (error: any, jobId: string) => {
        const statusCode = error?.response?.parsedBody?.code;
        const message = error.message;
        window.arangoHelper.arangoError(
          `Something went wrong while creating the index ${
            message ? `: ${message}` : ""
          }`
        );
        if (statusCode === 404 || statusCode === 400) {
          // delete non existing aardvark job
          window.arangoHelper.deleteAardvarkJob(jobId);
          return;
        }
        onError(error);
      };
      // if a job is in the list, this checks for status
      // by calling 'put /job/:jobId'
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

  useInterval(() => {
    window.arangoHelper.getAardvarkJobs(checkState);
  }, 10000);
};

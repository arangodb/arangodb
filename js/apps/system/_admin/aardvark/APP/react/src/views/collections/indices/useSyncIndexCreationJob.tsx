import { useEffect } from "react";
import { useSWRConfig } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";

export const useSyncIndexCreationJob = () => {
  const { collectionId, collectionName } = useCollectionIndicesContext();
  const { mutate } = useSWRConfig();

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
          window.arangoHelper.arangoMessage(
            "Index",
            "There is at least one new index in the queue or in the process of being created."
          );
          return;
        }
        window.arangoHelper.deleteAardvarkJob(jobId);
        const { encoded: encodedCollectionName } = encodeHelper(collectionName);

        mutate(`/index/?collection=${encodedCollectionName}`);
      };

      const onJobError = (error: any, jobId: string) => {
        const statusCode = error?.response?.body?.code;
        const errorMessage = error?.response?.body?.errorMessage;
        if (statusCode === 404) {
          // delete non existing aardvark job
          window.arangoHelper.deleteAardvarkJob(jobId);
        } else if (statusCode === 400) {
          // index job failed -> print error
          window.arangoHelper.arangoError(
            "Index creation failed",
            errorMessage
          );
          // delete non existing aardvark job
          window.arangoHelper.deleteAardvarkJob(jobId);
        }
      };

      jobsList.forEach(job => {
        if (job.collection === collectionId) {
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

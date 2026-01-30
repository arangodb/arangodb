import { useInterval } from "@chakra-ui/react";
import {
  checkAsyncJobStatus,
  deleteFrontendJob,
  getFrontendJobs
} from "../../utils/frontendJobs";

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
  const checkState = async () => {
    try {
      const jobsList = await getFrontendJobs();

      for (const job of jobsList) {
        if (job.collection !== jobCollectionName) {
          continue;
        }

        try {
          const { complete, error } = await checkAsyncJobStatus(job.id);

          if (!complete) {
            // Job still in queue or pending
            onQueue();
            continue;
          }

          // Job is complete, clean it up
          await deleteFrontendJob(job.id);

          if (error) {
            const statusCode = error?.response?.status;
            const message = error.message;
            window.arangoHelper.arangoError(
              `Something went wrong while updating the view${
                message ? `: ${message}` : ""
              }`
            );
            if (statusCode === 404 || statusCode === 400) {
              // Job already processed, just continue
              continue;
            }
            onError(error);
          } else {
            onSuccess();
          }
        } catch (error: any) {
          onError(error);
        }
      }
    } catch (error) {
      window.arangoHelper.arangoError("Jobs", "Could not read pending jobs.");
    }
  };

  useInterval(() => {
    checkState();
  }, 10000);
};

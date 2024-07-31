import { mutate } from "swr";
import { useJobSync } from "../../../components/hooks/useJobSync";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useSyncSearchViewUpdates = ({
  viewName
}: {
  viewName: string;
}) => {
  useJobSync({
    onQueue: () => {
      window.arangoHelper.arangoMessage(
        "View",
        "There is at least one new view in the queue or in the process of being created."
      );
    },
    onSuccess: () => {
      const { encoded: encodedViewName } = encodeHelper(viewName);
      mutate(`/view/${encodedViewName}/properties`);
      window.arangoHelper.arangoNotification(
        "Success",
        `Updated View: ${viewName}`
      );
    },
    onError: error => {
      const errorMessage = error?.response?.parsedBody?.errorMessage;
      window.arangoHelper.arangoError("View creation failed", errorMessage);
    },
    jobCollectionName: viewName
  });
};

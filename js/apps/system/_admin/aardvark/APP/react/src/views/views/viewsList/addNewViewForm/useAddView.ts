import { useState } from "react";
import { useSWRConfig } from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";

export const useAddView = () => {
  const [isLoading, setIsLoading] = useState(false);
  const { mutate } = useSWRConfig();
  const onAdd = (values: any) => {
    setIsLoading(true);
    return getApiRouteForCurrentDB()
      .post("/view", values)
      .then(() => {
        setIsLoading(false);
        window.arangoHelper.arangoNotification(
          "View",
          "Creation in progress. This may take a while."
        );
        mutate("/view");
      })
      .catch(error => {
        const errorMessage = error?.response?.body?.errorMessage;
        if (errorMessage) {
          window.arangoHelper.arangoError("View", errorMessage);
        }
        setIsLoading(false);
      });
  };
  return { onAdd, isLoading };
};

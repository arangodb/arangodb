import { useState } from "react";
import { useSWRConfig } from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../../utils/notifications";

export const useAddView = () => {
  const [isLoading, setIsLoading] = useState(false);
  const { mutate } = useSWRConfig();
  const onAdd = (values: any) => {
    setIsLoading(true);
    return getApiRouteForCurrentDB()
      .post("/view", values)
      .then(() => {
        setIsLoading(false);
        notifySuccess("View creation in progress, this may take a while.");
        mutate("/view");
      })
      .catch(error => {
        const errorMessage = error?.response?.body?.errorMessage;
        if (errorMessage) {
          notifyError(`View creation failed: ${errorMessage}`);
        }
        setIsLoading(false);
      });
  };
  return { onAdd, isLoading };
};

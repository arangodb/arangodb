import { useState } from "react";
import { useSWRConfig } from "swr";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../../utils/notifications";

export const useAddView = () => {
  const [isLoading, setIsLoading] = useState(false);
  const { mutate } = useSWRConfig();
  const onAdd = ({ name, ...values }: any) => {
    setIsLoading(true);
    return getCurrentDB()
      .view(name)
      .create(values)
      .then(() => {
        setIsLoading(false);
        notifySuccess("View creation in progress, this may take a while.");
        mutate("/view");
      })
      .catch(error => {
        const errorMessage = error?.response?.parsedBody?.errorMessage;
        if (errorMessage) {
          notifyError(`View creation failed: ${errorMessage}`);
        }
        setIsLoading(false);
      });
  };
  return { onAdd, isLoading };
};

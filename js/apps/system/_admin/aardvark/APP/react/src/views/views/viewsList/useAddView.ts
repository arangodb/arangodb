import { useState } from "react";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

export const useAddView = () => {
  const [isLoading, setIsLoading] = useState(false);
  const onAdd = (values: any) => {
    setIsLoading(true);
    return getApiRouteForCurrentDB()
      .post("/view", values)
      .then(() => setIsLoading(false))
      .catch(() => setIsLoading(false));
  };
  return { onAdd, isLoading };
};

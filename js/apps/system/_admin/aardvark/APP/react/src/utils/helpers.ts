import { ChangeEvent, useEffect, useRef } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "./arangoClient";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

export const getChangeHandler = (setter: (value: string) => void) => {
  return (event: ChangeEvent<HTMLInputElement | HTMLSelectElement>) => {
    setter(event.currentTarget.value);
  };
};

export const usePrevious = (value: any) => {
  const ref = useRef();

  useEffect(() => {
    ref.current = value;
  });

  return ref.current;
};


export const usePermissions = () => useSWR(
    `/user/${arangoHelper.getCurrentJwtUsername()}/database/${frontendConfig.db}`,
    (path) => getApiRouteForCurrentDB().get(path)
  );

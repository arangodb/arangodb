import { pick } from "lodash";
import { ArangoSearchViewPropertiesType } from "../searchView.types";
import { useUpdateSearchView } from "./useUpdateSearchView";

export const useUpdateArangoSearchViewProperties = () => {
  const getProperties = ({
    view
  }: {
    view: ArangoSearchViewPropertiesType;
  }) => {
    const properties = pick(
      view,
      "consolidationIntervalMsec",
      "commitIntervalMsec",
      "cleanupIntervalStep",
      "links",
      "consolidationPolicy"
    );
    return properties;
  };
  return useUpdateSearchView({
    getProperties
  });
};

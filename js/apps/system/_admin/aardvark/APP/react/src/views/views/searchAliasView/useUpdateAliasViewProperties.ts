import { differenceWith, isEqual } from "lodash";
import { useUpdateSearchView } from "../editView/useUpdateSearchView";
import { SearchAliasViewPropertiesType } from "../View.types";

export const useUpdateAliasViewProperties = () => {
  const getProperties = (({
    initialView,
    view
  }: {
    initialView: SearchAliasViewPropertiesType;
    view: SearchAliasViewPropertiesType;
  }) => {
    const { addedChanges, deletedChanges } = getUpdatedIndexes({
      view,
      initialView
    });
    let properties = addedChanges;
    if (deletedChanges.length > 0) {
      const deletedIndexes = deletedChanges.map(index => {
        return { ...index, operation: "del" };
      });
      properties = [...addedChanges, ...deletedIndexes];
    }
    return { indexes: properties };
  }) as any;
  return useUpdateSearchView({
    getProperties
  });
};

export const getUpdatedIndexes = ({
  view,
  initialView
}: {
  view: SearchAliasViewPropertiesType;
  initialView: SearchAliasViewPropertiesType;
}) => {
  const addedChanges = differenceWith(
    view.indexes,
    initialView.indexes,
    isEqual
  );
  const deletedChanges = differenceWith(
    initialView.indexes,
    view.indexes,
    isEqual
  );

  return { addedChanges, deletedChanges };
};

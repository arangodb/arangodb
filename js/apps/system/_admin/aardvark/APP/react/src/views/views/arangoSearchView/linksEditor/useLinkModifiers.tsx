import { useFormikContext } from "formik";
import { cloneDeep, get, set } from "lodash";
import { useEditViewContext } from "../../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../../searchView.types";

export const useFieldPath = () => {
  const { currentField } = useEditViewContext();

  const fieldPath =
    currentField.length > 0
      ? currentField.reduce((acc, curr) => {
          acc.push("fields");
          acc.push(curr);
          return acc;
        }, [] as string[])
      : [];
  return {
    fieldPath
  };
};

export const useLinkModifiers = () => {
  const { currentLink = "" } = useEditViewContext();
  const { fieldPath } = useFieldPath();
  const { values, setValues } =
    useFormikContext<ArangoSearchViewPropertiesType>();

  const setCurrentLinkValue = ({
    id,
    value
  }: {
    id: string[];
    value: string | string[] | {} | undefined;
  }) => {
    const newLinks = values.links
      ? set(cloneDeep(values.links), [currentLink, ...fieldPath, ...id], value)
      : {};

    setValues({
      ...values,
      links: newLinks
    });
  };
  const getCurrentLinkValue = (id: string[]) => {
    return get(values.links, [currentLink, ...fieldPath, ...id]);
  };
  return { setCurrentLinkValue, getCurrentLinkValue };
};

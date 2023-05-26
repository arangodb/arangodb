import { useFormikContext } from "formik";
import { cloneDeep, get, set } from "lodash";
import { OptionType } from "../../../../components/select/SelectBase";
import { useEditViewContext } from "../../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../../searchView.types";

export const useLinksSetter = () => {
  const { currentLink = "" } = useEditViewContext();
  const { fieldPath } = useFieldPath();
  const { values, setValues } =
    useFormikContext<ArangoSearchViewPropertiesType>();
  const currentLinkFieldsValue = values?.links?.[currentLink];
  const fieldsValue =
    fieldPath.length > 0
      ? get(currentLinkFieldsValue, fieldPath)?.fields
      : currentLinkFieldsValue?.fields;
  const fields = fieldsValue
    ? (Object.keys(fieldsValue)
        .map(key => {
          if (fieldsValue[key] === undefined) {
            return null;
          }
          return {
            label: key,
            value: key
          };
        })
        .filter(Boolean) as OptionType[])
    : [];
  const addField = (field: string) => {
    const newLinks = values.links
      ? set(
          cloneDeep(values.links),
          [currentLink, ...fieldPath, "fields", field],
          {}
        )
      : {};

    setValues({
      ...values,
      links: newLinks
    });
  };
  const removeField = (field: string) => {
    const newLinks = values.links
      ? set(
          cloneDeep(values.links),
          [currentLink, ...fieldPath, "fields", field],
          undefined
        )
      : {};
    setValues({
      ...values,
      links: newLinks
    });
  };
  const analyzersValue =
    fieldPath.length > 0
      ? get(currentLinkFieldsValue, fieldPath)?.analyzers || []
      : currentLinkFieldsValue?.analyzers || [];
  const analyzers =
    analyzersValue?.map((analyzer: string) => ({
      label: analyzer,
      value: analyzer
    })) || [];
  const addAnalyzer = (analyzer: string) => {
    const newLinks = values.links
      ? set(
          cloneDeep(values.links),
          [currentLink, ...fieldPath, "analyzers"],
          [...analyzersValue, analyzer]
        )
      : {};
    setValues({
      ...values,
      links: newLinks
    });
  };
  const removeAnalyzer = (analyzer: string) => {
    const newLinks = values.links
      ? set(
          cloneDeep(values.links),
          [currentLink, ...fieldPath, "analyzers"],
          analyzersValue.filter((a: string) => a !== analyzer)
        )
      : {};
    setValues({
      ...values,
      links: newLinks
    });
  };
  return {
    fields,
    addField,
    removeField,
    analyzers,
    addAnalyzer,
    removeAnalyzer
  };
};

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

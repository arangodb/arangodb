import { get } from "lodash";
import React, { useContext } from "react";
import { ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import { useLinksContext } from "../LinksContext";

type FieldViewProps = {
  disabled: boolean | undefined;
};

const FieldView = ({ disabled }: FieldViewProps) => {
  const { currentField } = useLinksContext();
  const { formState, dispatch } = useContext(ViewContext);
  const field = currentField && get(formState, currentField.fieldPath);
  if (!currentField || !field) {
    return null;
  }
  return (
    <LinkPropertiesInput
      formState={field}
      disabled={disabled}
      basePath={currentField.fieldPath}
      dispatch={dispatch}
    />
  );
};

export default FieldView;

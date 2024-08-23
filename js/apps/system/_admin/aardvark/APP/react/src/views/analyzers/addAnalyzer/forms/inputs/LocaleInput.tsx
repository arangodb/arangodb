import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const LocaleInput = ({
  basePropertiesPath,
  placeholder
}: {
  basePropertiesPath: string;
  placeholder?: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <InputControl
      isDisabled={isDisabled}
      name={`${basePropertiesPath}.locale`}
      inputProps={{ placeholder }}
      label="Locale"
    />
  );
};

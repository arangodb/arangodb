import { InputControl } from "@arangodb/ui";
import React from "react";
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

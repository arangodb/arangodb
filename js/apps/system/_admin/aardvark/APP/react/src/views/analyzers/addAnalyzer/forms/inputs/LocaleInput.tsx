import React from "react";
import { InputControl } from "../../../../../components/form/InputControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const LocaleInput = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <InputControl
      isDisabled={isDisabled}
      name={`${basePropertiesPath}.locale`}
      inputProps={{
        placeholder: "language[_COUNTRY][.encoding][@variant]"
      }}
      label="Locale"
    />
  );
};

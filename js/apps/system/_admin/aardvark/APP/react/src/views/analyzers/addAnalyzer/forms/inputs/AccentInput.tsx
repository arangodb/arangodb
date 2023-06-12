import React from "react";
import { SwitchControl } from "../../../../../components/form/SwitchControl";
import { useAnalyzersContext } from "../../../AnalyzersContext";

export const AccentInput = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <SwitchControl
      isDisabled={isDisabled}
      name={`${basePropertiesPath}.accent`}
      label="Accent"
    />
  );
};

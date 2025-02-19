import { SwitchControl } from "@arangodb/ui";
import React from "react";
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

import { InfoTooltip } from "@arangodb/ui";
import React from "react";

export const IndexInfoTooltip = ({ label }: { label: string }) => {
  return (
    <InfoTooltip
      label={label}
      boxProps={{
        alignSelf: "start",
        top: "1"
      }}
    />
  );
};

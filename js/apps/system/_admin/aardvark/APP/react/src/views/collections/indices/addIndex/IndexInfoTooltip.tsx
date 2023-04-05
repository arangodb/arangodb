import React from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";

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

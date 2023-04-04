import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";

import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeDirection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeDirection: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeDirection">Show edge direction</FormLabel>
      <Checkbox
        id="edgeDirection"
        isChecked={urlParams.edgeDirection}
        onChange={handleChange}
      />
      <InfoTooltip
        label={"When true, an arrowhead on the 'to' side of the edge is drawn."}
      />
    </>
  );
};

export default ParameterEdgeDirection;

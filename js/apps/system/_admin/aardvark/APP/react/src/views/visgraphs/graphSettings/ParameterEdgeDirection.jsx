import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";

import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeDirection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [edgeDirection, setEdgeDirection] = useState(urlParams.edgeDirection);

  const handleChange = event => {
    setEdgeDirection(event.target.checked);
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
        checked={edgeDirection}
        onChange={handleChange}
      />
      <InfoTooltip
        label={"When true, an arrowhead on the 'to' side of the edge is drawn."}
      />
    </>
  );
};

export default ParameterEdgeDirection;

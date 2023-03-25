import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";

import { UrlParametersContext } from "./url-parameters-context";

const ParameterEdgeDirection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeDirection, setEdgeDirection] = useState(
    urlParameters.edgeDirection
  );

  const handleChange = event => {
    setEdgeDirection(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      edgeDirection: event.target.checked
    };
    setUrlParameters(newUrlParameters);
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

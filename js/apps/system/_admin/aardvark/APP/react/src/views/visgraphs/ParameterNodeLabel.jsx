import { Box, FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterNodeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabel, setNodeLabel] = useState(urlParameters.nodeLabel);

  const handleChange = event => {
    setNodeLabel(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      nodeLabel: event.target.value
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeLabel">Node label</FormLabel>
      <Input
        id="nodeLabel"
        min={1}
        required={true}
        value={nodeLabel}
        background="#ffffff"
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={"Enter a valid node attribute to be used as a node label."}
      />
    </>
  );
};

export default ParameterNodeLabel;

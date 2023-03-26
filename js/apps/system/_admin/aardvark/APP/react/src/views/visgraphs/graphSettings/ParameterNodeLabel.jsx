import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeLabel = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [nodeLabel, setNodeLabel] = useState(urlParams.nodeLabel);

  const handleChange = event => {
    setNodeLabel(event.target.value);
    const newUrlParameters = {
      ...urlParams,
      nodeLabel: event.target.value
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeLabel">Node label</FormLabel>
      <Input
        id="nodeLabel"
        min={1}
        required={true}
        value={nodeLabel}
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

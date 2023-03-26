import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeSize = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const [nodeSize, setNodeSize] = useState(urlParams.nodeSize);

  const handleChange = event => {
    setNodeSize(event.target.value);
    const newUrlParameters = { ...urlParams, nodeSize: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeSize"> Sizing attribute</FormLabel>
      <Input
        id="nodeSize"
        width="200px"
        min={1}
        required={true}
        value={nodeSize}
        size="sm"
        onChange={handleChange}
        disabled={urlParams.nodeSizeByEdges}
      />
      <InfoTooltip
        label={
          "If an attribute is given, nodes will be sized by the attribute."
        }
      />
    </>
  );
};

export default ParameterNodeSize;

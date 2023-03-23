import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const handleChange = event => {
    setNodeSize(event.target.value);
    const newUrlParameters = { ...urlParameters, nodeSize: event.target.value };
    setUrlParameters(newUrlParameters);
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
        background="#ffffff"
        size="sm"
        onChange={handleChange}
        disabled={urlParameters.nodeSizeByEdges}
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

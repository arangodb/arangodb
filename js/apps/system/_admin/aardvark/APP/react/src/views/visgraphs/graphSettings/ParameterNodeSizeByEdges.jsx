import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeSizeByEdges = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(
    urlParams.nodeSizeByEdges
  );

  const handleChange = event => {
    setNodeSizeByEdges(event.target.checked);
    const newUrlParameters = {
      ...urlParams,
      nodeSizeByEdges: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeSizeByEdges">Size by connections</FormLabel>
      <Checkbox
        id="nodeSizeByEdges"
        checked={nodeSizeByEdges}
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "If enabled, nodes are sized according to the number of edges they have. This option overrides the sizing attribute."
        }
      />
    </>
  );
};

export default ParameterNodeSizeByEdges;

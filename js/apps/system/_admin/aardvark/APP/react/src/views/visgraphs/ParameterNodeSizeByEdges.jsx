import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterNodeSizeByEdges = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(
    urlParameters.nodeSizeByEdges
  );

  const handleChange = event => {
    setNodeSizeByEdges(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      nodeSizeByEdges: event.target.checked
    };
    setUrlParameters(newUrlParameters);
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

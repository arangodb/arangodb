import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeSizeByEdges = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
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
        isChecked={urlParams.nodeSizeByEdges}
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

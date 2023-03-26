import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeSize = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
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
        value={urlParams.nodeSize}
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

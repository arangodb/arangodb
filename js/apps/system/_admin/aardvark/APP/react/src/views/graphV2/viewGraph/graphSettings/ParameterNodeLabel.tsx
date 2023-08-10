import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeLabel = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
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
        value={urlParams.nodeLabel}
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

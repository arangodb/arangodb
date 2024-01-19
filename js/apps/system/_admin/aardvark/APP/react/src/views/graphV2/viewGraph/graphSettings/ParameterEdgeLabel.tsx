import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeLabel = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeLabel: event.target.value
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeLabel">Edge label</FormLabel>
      <Input
        id="edgeLabel"
        value={urlParams.edgeLabel}
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={"Enter a valid edge attribute to be used as an edge label."}
      />
    </>
  );
};

export default ParameterEdgeLabel;

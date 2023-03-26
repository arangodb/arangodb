import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeStart = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeStart: event.target?.value
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeStart"> Start node</FormLabel>
      <Input
        id="nodeStart"
        width="200px"
        min={1}
        required={true}
        value={urlParams.nodeStart}
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."
        }
      />
    </>
  );
};

export default ParameterNodeStart;

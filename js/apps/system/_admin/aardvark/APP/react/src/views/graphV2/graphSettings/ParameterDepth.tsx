import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterDepth = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    if (Number(event.target.value) >= 0) {
      const newUrlParameters = {
        ...urlParams,
        depth: Number(event.target.value)
      };
      setUrlParams(newUrlParameters);
    }
  };

  return (
    <>
      <FormLabel htmlFor="depth">Depth</FormLabel>
      <Input
        id="depth"
        width="200px"
        value={urlParams.depth}
        size="sm"
        type="number"
        onChange={handleChange}
      />

      <InfoTooltip label={"Search depth, starting from your start node."} />
    </>
  );
};

export default ParameterDepth;

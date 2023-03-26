import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterDepth = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [depth, setDepth] = useState(urlParams.depth);

  const handleChange = event => {
    setDepth(event.target.value);
    const newUrlParameters = { ...urlParams, depth: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="depth">Depth</FormLabel>
      <Input
        id="depth"
        width="200px"
        min={1}
        required={true}
        value={depth}
        size="sm"
        onChange={handleChange}
      />

      <InfoTooltip label={"Search depth, starting from your start node."} />
    </>
  );
};

export default ParameterDepth;

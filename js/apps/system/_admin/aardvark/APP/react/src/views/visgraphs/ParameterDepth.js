import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from "../../components/arango/tootip";
import { Box, Flex, Spacer, Input, Center, FormLabel } from "@chakra-ui/react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";

const ParameterDepth = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [depth, setDepth] = useState(urlParameters.depth);

  const handleChange = event => {
    setDepth(event.target.value);
    const newUrlParameters = { ...urlParameters, depth: event.target.value };
    setUrlParameters(newUrlParameters);
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
        background="#ffffff"
        size="sm"
        onChange={handleChange}
      />

      <InfoTooltip label={"Search depth, starting from your start node."} />
    </>
  );
};

export default ParameterDepth;

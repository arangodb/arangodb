import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { useState } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [nodeColor, setNodeColor] = useState(urlParams.nodeColor);

  const handleChange = event => {
    setNodeColor(event.target.value);
    const newUrlParameters = {
      ...urlParams,
      nodeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };

  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith("#")) {
    calculatedNodeColor = "#" + nodeColor;
  }

  return (
    <>
      <FormLabel htmlFor="nodeColor">Default node color </FormLabel>
      <Input
        id="nodeColor"
        type="color"
        value={calculatedNodeColor}
        style={{
          width: "60px",
          height: "30px"
        }}
        onChange={handleChange}
        disabled={urlParams.nodeColorByCollection}
      />
      <Spacer />
    </>
  );
};

export default ParameterNodeColor;

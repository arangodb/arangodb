import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { useState } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [edgeColor, setEdgeColor] = useState(urlParams.edgeColor);

  const handleChange = event => {
    setEdgeColor(event.target.value);
    const newUrlParameters = {
      ...urlParams,
      edgeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };
  let calculatedEdgeColor = edgeColor;
  if (!edgeColor.startsWith("#")) {
    calculatedEdgeColor = "#" + edgeColor;
  }

  return (
    <>
      <FormLabel htmlFor="edgeColor">Default edge color</FormLabel>
      <Input
        id="edgeColor"
        type="color"
        value={calculatedEdgeColor}
        style={{
          width: "60px",
          height: "30px"
        }}
        onChange={handleChange}
        disabled={urlParams.edgeColorByCollection}
      />
      <Spacer />
    </>
  );
};

export default ParameterEdgeColor;

import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterEdgeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColor, setEdgeColor] = useState(urlParameters.edgeColor);

  const handleChange = event => {
    setEdgeColor(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      edgeColor: event.target.value.replace("#", "")
    };
    setUrlParameters(newUrlParameters);
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
        disabled={urlParameters.edgeColorByCollection}
      />
      <Spacer />
    </>
  );
};

export default ParameterEdgeColor;

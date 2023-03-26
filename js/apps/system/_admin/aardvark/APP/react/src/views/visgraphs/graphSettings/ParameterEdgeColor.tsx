import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };
  let calculatedEdgeColor = urlParams.edgeColor;
  if (!calculatedEdgeColor.startsWith("#")) {
    calculatedEdgeColor = "#" + calculatedEdgeColor;
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

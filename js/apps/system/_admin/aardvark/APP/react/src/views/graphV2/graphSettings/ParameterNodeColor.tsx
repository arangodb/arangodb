import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };

  let calculatedNodeColor = urlParams.nodeColor;
  if (!calculatedNodeColor.startsWith("#")) {
    calculatedNodeColor = "#" + calculatedNodeColor;
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

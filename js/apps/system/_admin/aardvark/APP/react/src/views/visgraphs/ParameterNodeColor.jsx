import { FormLabel, Spacer } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import Textinput from "./components/pure-css/form/Textinput.tsx";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const handleChange = event => {
    setNodeColor(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      nodeColor: event.target.value.replace("#", "")
    };
    setUrlParameters(newUrlParameters);
  };

  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith("#")) {
    calculatedNodeColor = "#" + nodeColor;
  }

  return (
    <>
      <FormLabel htmlFor="nodeColor">Default node color </FormLabel>
      <Textinput
        id="nodeColor"
        type={"color"}
        value={calculatedNodeColor}
        template={"graphviewer"}
        width={"60px"}
        height={"30px"}
        onChange={handleChange}
        disabled={urlParameters.nodeColorByCollection}
      />
      <Spacer />
    </>
  );
};

export default ParameterNodeColor;

import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterEdgeLabel = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabel, setEdgeLabel] = useState(urlParameters.edgeLabel);

  const handleChange = event => {
    setEdgeLabel(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      edgeLabel: event.target.value
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeLabel">Edge label</FormLabel>
      <Input
        id="edgeLabel"
        min={1}
        required={true}
        value={edgeLabel}
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        title={"Enter a valid edge attribute to be used as an edge label."}
        setArrow={true}
      />
    </>
  );
};

export default ParameterEdgeLabel;

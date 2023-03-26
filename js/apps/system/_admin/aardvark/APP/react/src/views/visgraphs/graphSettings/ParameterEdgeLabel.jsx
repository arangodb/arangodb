import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeLabel = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [edgeLabel, setEdgeLabel] = useState(urlParams.edgeLabel);

  const handleChange = event => {
    setEdgeLabel(event.target.value);
    const newUrlParameters = {
      ...urlParams,
      edgeLabel: event.target.value
    };
    setUrlParams(newUrlParameters);
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

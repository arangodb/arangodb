import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColorAttribute = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const [nodeColorAttribute, setNodeColorAttribute] = useState(
    urlParams.nodeColorAttribute
  );

  const handleChange = event => {
    setNodeColorAttribute(event.target.value);
    const newUrlParameters = {
      ...urlParams,
      nodeColorAttribute: event.target.value
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeColorAttribute">Node color attribute</FormLabel>
      <Input
        id="nodeColorAttribute"
        width="200px"
        min={1}
        required={true}
        value={nodeColorAttribute}
        size="sm"
        onChange={handleChange}
        disabled={urlParams.nodeColorByCollection}
      />
      <InfoTooltip
        label={
          "If an attribute is given, nodes will be colorized by the attribute. This setting ignores default node color if set."
        }
      />
    </>
  );
};

export default ParameterNodeColorAttribute;

import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterNodeColorAttribute = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(
    urlParameters.nodeColorAttribute
  );

  const handleChange = event => {
    setNodeColorAttribute(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      nodeColorAttribute: event.target.value
    };
    setUrlParameters(newUrlParameters);
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
        background="#ffffff"
        size="sm"
        onChange={handleChange}
        disabled={urlParameters.nodeColorByCollection}
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

import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColorAttribute = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
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
        required
        value={urlParams.nodeColorAttribute}
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

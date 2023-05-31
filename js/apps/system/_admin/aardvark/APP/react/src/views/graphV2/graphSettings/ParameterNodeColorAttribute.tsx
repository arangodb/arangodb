import {
  FormControl,
  FormErrorMessage,
  FormLabel,
  Input
} from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useGraph } from "../GraphContext";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColorAttribute = () => {
  const { graphData } = useGraph();
  const { settings } = graphData || {};
  const { nodeColorAttributeMessage } = settings || {};
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
      <FormControl isInvalid={!!nodeColorAttributeMessage}>
        <Input
          id="nodeColorAttribute"
          width="200px"
          value={urlParams.nodeColorAttribute}
          size="sm"
          onChange={handleChange}
          disabled={urlParams.nodeColorByCollection}
        />
        <FormErrorMessage>{nodeColorAttributeMessage}</FormErrorMessage>
      </FormControl>
      <InfoTooltip
        label={
          "If an attribute is given, nodes will be colorized by the attribute. This setting ignores default node color if set."
        }
      />
    </>
  );
};

export default ParameterNodeColorAttribute;

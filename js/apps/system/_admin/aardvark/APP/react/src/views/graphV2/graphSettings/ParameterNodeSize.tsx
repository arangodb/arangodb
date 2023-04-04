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

const ParameterNodeSize = () => {
  const { graphData } = useGraph();
  const { settings } = graphData || {};
  const { nodeSizeAttributeMessage } = settings || {};

  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = { ...urlParams, nodeSize: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeSize">Sizing attribute</FormLabel>
      <FormControl isInvalid={!!nodeSizeAttributeMessage}>
        <Input
          id="nodeSize"
          width="200px"
          value={urlParams.nodeSize}
          size="sm"
          onChange={handleChange}
          disabled={urlParams.nodeSizeByEdges}
        />
        <FormErrorMessage>{nodeSizeAttributeMessage}</FormErrorMessage>
      </FormControl>
      <InfoTooltip
        label={
          "If an attribute is given, nodes will be sized by the attribute."
        }
      />
    </>
  );
};

export default ParameterNodeSize;

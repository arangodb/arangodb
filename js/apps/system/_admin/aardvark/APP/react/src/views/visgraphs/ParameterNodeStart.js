import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from "../../components/arango/tootip";
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center,
  Grid,
  FormLabel
} from "@chakra-ui/react";

const ParameterNodeStart = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);

  const handleChange = event => {
    const newUrlParameters = {
      ...urlParameters,
      nodeStart: event.target.value
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <Grid alignItems="center" templateColumns="150px 1fr 40px">
      <FormLabel htmlFor="startNode"> Start node</FormLabel>
      <Input
        name="startNode"
        width="200px"
        min={1}
        required={true}
        value={urlParameters.nodeStart}
        background="#ffffff"
        size="sm"
        onChange={handleChange}
      />
      <Center>
        <ToolTip
          title={
            "A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."
          }
          setArrow={true}
        >
          <span
            className="arangoicon icon_arangodb_info"
            style={{ fontSize: "16px", color: "#989CA1" }}
          ></span>
        </ToolTip>
      </Center>
    </Grid>
  );
};

export default ParameterNodeStart;

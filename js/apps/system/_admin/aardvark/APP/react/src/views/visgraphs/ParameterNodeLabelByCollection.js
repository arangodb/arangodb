import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Center } from '@chakra-ui/react';

const ParameterNodeLabelByCollection = ({ onAddCollectionNameChange }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <Flex direction='row' mt='12'>
      <Center>
        <Box color='#fff' w='150px'>Show collection name</Box>
      </Center>
      <Checkbox
        inline
        checked={nodeLabelByCollection}
        onChange={() => {
          const newNodeLabelByCollection = !nodeLabelByCollection;
          setNodeLabelByCollection(newNodeLabelByCollection);
          newUrlParameters.nodeLabelByCollection = newNodeLabelByCollection;
          setUrlParameters(newUrlParameters);
        }}
        template={'graphviewer'}
      />
      <Center>
        <ToolTip
          title={"Adds a collection name to the node label."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </Center>

    </Flex>
  );
};

export default ParameterNodeLabelByCollection;

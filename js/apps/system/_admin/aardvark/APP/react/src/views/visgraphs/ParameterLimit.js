import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import {
  Box,
  Flex,
  Spacer,
  Input,
  Center } from '@chakra-ui/react';

const ParameterLimit = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [limit, setLimit] = useState(urlParameters.limit);

  const newUrlParameters = { ...urlParameters };

  return (
      <Flex direction='row' mt='24' mb='12'>
        <Center>
          <Box color='#fff' w='150px'>Limit</Box>
        </Center>
        <Input  
          width='60px'
          min={1}
          required={true}
          value={limit}
          onChange={(e) => {
            setLimit(+e.target.value);
            newUrlParameters.limit = +e.target.value;
            setUrlParameters(newUrlParameters);
          }}
        />
        <Spacer />
        <Center>
          <ToolTip
            title={"Limit nodes count. If empty or zero, no limit is set."}
            setArrow={true}
          >
            <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
          </ToolTip>
        </Center>
      </Flex>
  );
};

export default ParameterLimit;

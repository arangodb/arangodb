import { ArrowBackIcon, SettingsIcon } from "@chakra-ui/icons";
import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  Button,
  Flex,
  Grid,
  HStack,
  Icon,
  IconButton,
  Menu,
  MenuButton,
  Portal,
  Progress,
  Tooltip,
  useMenuContext,
  useMenuPositioner,
  useMultiStyleConfig
} from "@chakra-ui/react";
import React, { ReactNode, useMemo } from "react";
import { useHistory } from "react-router";
import { AddAPhoto } from "styled-icons/material";
import { downloadCanvas } from "../../graphV2/graphHeader/DownloadGraphButton";
import { FullscreenGraphButton } from "../../graphV2/graphHeader/FullscreenGraphButton";
import { GraphLayoutSelectorComponent } from "../../graphV2/graphSettings/GraphLayoutSelector";
import { LayoutType } from "../../graphV2/UrlParametersContext";
import { useQueryContext } from "../QueryContextProvider";
import { detectGraph } from "../queryResults/useDisplayTypes";
import { convertToGraphData } from "../queryResults/useSetupQueryGraph";
import {
  QueryFullGraphContextProvider,
  useQueryFullGraph
} from "./QueryFullGraphContext";

export const QueryFullGraphView = () => {
  const { queryGraphResult } = useQueryContext();
  const { graphDataType } = detectGraph({
    result: queryGraphResult.result
  });
  const graphData = useMemo(() => {
    return convertToGraphData({ graphDataType, data: queryGraphResult.result });
  }, [graphDataType, queryGraphResult.result]);
  const visJsRef = React.useRef(null);

  return (
    <QueryFullGraphContextProvider visJsRef={visJsRef} graphData={graphData}>
      <QueryFullGraphViewInner />
    </QueryFullGraphContextProvider>
  );
};

const QueryGraphHeader = () => {
  const history = useHistory();
  return (
    <Flex padding="2" alignItems="center">
      <Button
        onClick={() => {
          history.push(`/queries`);
        }}
        size="sm"
        leftIcon={<ArrowBackIcon />}
      >
        Back to Query Editor
      </Button>
      <Flex marginLeft="auto" gap="2">
        <Tooltip hasArrow label={"Take a screenshot"} placement="bottom">
          <IconButton
            onClick={() => downloadCanvas("query-graph")}
            size="sm"
            icon={<Icon width="5" height="5" as={AddAPhoto} />}
            aria-label={"Take a screenshot"}
          />
        </Tooltip>
        <FullscreenGraphButton />
        <QueryGraphSettings />
      </Flex>
    </Flex>
  );
};

const QueryGraphSettings = () => {
  return (
    <Menu closeOnSelect={false} closeOnBlur={false}>
      <MenuButton
        as={Button}
        size="sm"
        leftIcon={<SettingsIcon />}
        aria-label={"Settings"}
        colorScheme="green"
      >
        Settings
      </MenuButton>
      <SettingsMenuContent />
    </Menu>
  );
};
const SettingsMenuContent = () => {
  const { settings, setSettings } = useQueryFullGraph();
  const { isOpen } = useMenuContext();
  const positionerProps = useMenuPositioner();
  const styles = useMultiStyleConfig("Menu", {});

  if (!isOpen) {
    return null;
  }
  return (
    <Portal>
      <Box
        {...positionerProps}
        __css={{
          ...styles.list
        }}
        maxHeight="600px"
        overflow="overlay"
        width="400px"
        position="relative"
        paddingY="0"
      >
        <Accordion allowMultiple allowToggle defaultIndex={[0]}>
          <GraphAccordionItem title="Graph">
            <Grid
              rowGap="2"
              alignItems="center"
              templateColumns="150px 1fr 40px"
              justifyItems="start"
            >
              <GraphLayoutSelectorComponent
                value={settings.layout}
                onChange={event => {
                  setSettings({
                    ...settings,
                    layout: event.target.value as LayoutType
                  });
                }}
              />
            </Grid>
          </GraphAccordionItem>
          <GraphAccordionItem title="Nodes">
            {/* <AccordionNodesContent /> */}
          </GraphAccordionItem>
          <GraphAccordionItem title="Edges">
            {/* <AccordionEdgesContent /> */}
          </GraphAccordionItem>
        </Accordion>
        <HStack
          background="white"
          borderTop="1px solid"
          borderColor="gray.300"
          position="sticky"
          bottom="0"
          justifyContent="end"
          padding="3"
          paddingRight="5"
        >
          <ApplyButton />
        </HStack>
      </Box>
    </Portal>
  );
};
const GraphAccordionItem = ({
  children,
  title
}: {
  children: ReactNode;
  title: string;
}) => {
  return (
    <AccordionItem borderColor="gray.300">
      <AccordionButton>
        <Box as="span" flex="1" textAlign="left" fontWeight="bold">
          {title}
        </Box>
        <AccordionIcon />
      </AccordionButton>

      <AccordionPanel pb={4}>{children}</AccordionPanel>
    </AccordionItem>
  );
};

const ApplyButton = () => {
  const { onApply } = useQueryFullGraph();
  return (
    <Button colorScheme="green" onClick={onApply}>
      Apply
    </Button>
  );
};

const QueryFullGraphViewInner = () => {
  const { network, visJsRef, progressValue } = useQueryFullGraph();
  return (
    <Flex direction="column" height="full" width="full">
      <QueryGraphHeader />
      <Box
        position="relative"
        width="full"
        height="calc(100vh - 60px)"
        overflow="auto"
      >
        {progressValue < 100 && (
          <Box marginX="10" marginY="10">
            <Progress value={progressValue} colorScheme="green" />
          </Box>
        )}
        {progressValue === 100 && (
          <Button
            position="absolute"
            zIndex="1"
            right="12px"
            top="12px"
            size="xs"
            variant="ghost"
            onClick={() => {
              network?.fit();
            }}
          >
            Reset zoom
          </Button>
        )}
        <Box height="full" ref={visJsRef} />
      </Box>
    </Flex>
  );
};

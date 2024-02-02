import { ArrowBackIcon, DeleteIcon } from "@chakra-ui/icons";
import { Button, Flex, Heading, IconButton, Stack } from "@chakra-ui/react";
import React from "react";
import { useHistory, useRouteMatch } from "react-router-dom";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { notifySuccess } from "../../../utils/notifications";
import { useAnalyzersContext } from "../AnalyzersContext";
import { AnalyzerInfo } from "./AnalyzerInfo";
import { DeleteAnalyzerModal } from "./DeleteAnalyzerModal";

export const SingleAnalyzerView = () => {
  const { params } = useRouteMatch<{ analyzerName: string }>();
  const { analyzers } = useAnalyzersContext();
  const { analyzerName } = params;
  const currentAnalyzer = analyzers?.find(a => a.name === analyzerName);
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const history = useHistory();
  if (!currentAnalyzer) {
    return null;
  }
  const isBuiltIn = !currentAnalyzer.name.includes("::");
  const currentDbName = window.frontendConfig.db;
  const isSameDb = currentAnalyzer.name.split("::")[0] === currentDbName;
  const showDeleteButton = !isBuiltIn && isSameDb;

  return (
    <Stack padding="6" width="100%" height="calc(100vh - 120px)">
      <Flex direction="row" alignItems="center">
        <IconButton
          aria-label="Back"
          variant="ghost"
          size="sm"
          icon={<ArrowBackIcon width="24px" height="24px" />}
          onClick={() => {
            history.push("/analyzers");
          }}
        />
        <Heading marginLeft="2" as="h1" size="lg">
          Analyzer: {params.analyzerName}
        </Heading>
        {showDeleteButton && (
          <Button
            size="xs"
            marginLeft="auto"
            leftIcon={<DeleteIcon />}
            colorScheme="red"
            onClick={() => {
              setShowDeleteModal(true);
            }}
          >
            Delete
          </Button>
        )}
      </Flex>
      <AnalyzerInfo analyzer={currentAnalyzer} />
      <ControlledJSONEditor
        mode="code"
        value={currentAnalyzer}
        htmlElementProps={{
          style: {
            height: "calc(100%)",
            width: "100%"
          }
        }}
      />

      {showDeleteModal && (
        <DeleteAnalyzerModal
          analyzer={currentAnalyzer}
          onSuccess={() => {
            notifySuccess(
              `Analyzer: ${currentAnalyzer.name} deleted successfully`
            );
            history.push("/analyzers");
          }}
          onClose={() => {
            setShowDeleteModal(false);
          }}
        />
      )}
    </Stack>
  );
};

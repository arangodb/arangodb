import { ArrowBackIcon, DeleteIcon } from "@chakra-ui/icons";
import { Button, Flex, Heading, IconButton, Stack } from "@chakra-ui/react";
import React from "react";
import { useHistory } from "react-router-dom";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { notifySuccess } from "../../../utils/notifications";
import { useCustomHashMatch } from "../../../utils/useCustomHashMatch";
import { useAnalyzersContext } from "../AnalyzersContext";
import { AnalyzerInfo } from "./AnalyzerInfo";
import { DeleteAnalyzerModal } from "./DeleteAnalyzerModal";
import { useNavbarHeight } from "../../useNavbarHeight";

export const SingleAnalyzerView = () => {
  // need to use winodw.location.hash here instead of useRouteMatch because
  // useRouteMatch does not work with hash router
  const analyzerName = useCustomHashMatch("#analyzers/");
  const decodedAnalyzerName = decodeURIComponent(analyzerName);
  const { analyzers } = useAnalyzersContext();
  const currentAnalyzer = analyzers?.find(a => a.name === decodedAnalyzerName);
  const [showDeleteModal, setShowDeleteModal] = React.useState(false);
  const history = useHistory();
  const navbarHeight = useNavbarHeight();
  if (!currentAnalyzer) {
    return null;
  }
  const isBuiltIn = !currentAnalyzer.name.includes("::");
  const currentDbName = window.frontendConfig.db;
  const isSameDb = currentAnalyzer.name.split("::")[0] === currentDbName;
  const showDeleteButton = !isBuiltIn && isSameDb;

  return (
    <Stack padding="6" width="100%" height={`calc(100vh - ${navbarHeight}px)`}>
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
          Analyzer: {decodedAnalyzerName}
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

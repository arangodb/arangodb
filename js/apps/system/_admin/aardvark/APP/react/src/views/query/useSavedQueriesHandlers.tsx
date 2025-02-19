import { useDisclosure } from "@chakra-ui/react";
import { mutate } from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";

export const useSavedQueriesHandlers = ({
  queryValue,
  queryBindParams
}: {
  queryValue: string;
  queryBindParams: { [key: string]: string };
  queryName?: string;
}) => {
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries();
  const onSaveAs = async (queryName: string) => {
    const newQueries = [
      ...(savedQueries || []),
      {
        name: queryName,
        value: queryValue,
        parameter: queryBindParams,
        created_at: Date.now()
      }
    ];
    await patchQueries({
      queries: newQueries,
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Saved query: ${queryName}`);
      }
    });
    mutate("/savedQueries");
  };
  const onSave = async (queryName: string) => {
    await mutate("/savedQueries");
    if (queryName) {
      const newQueries = savedQueries?.map(query => {
        if (query.name === queryName) {
          return {
            name: queryName,
            value: queryValue,
            parameter: queryBindParams,
            created_at: query.created_at,
            modified_at: Date.now()
          };
        }
        return query;
      });
      await patchQueries({
        queries: newQueries || [],
        onSuccess: () => {
          window.arangoHelper.arangoNotification(`Updated query: ${queryName}`);
        }
      });
      mutate("/savedQueries");
    }
  };

  const onDelete = async (queryName: string) => {
    const newQueries = savedQueries?.filter(query => query.name !== queryName);
    await patchQueries({
      queries: newQueries || [],
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Deleted query: ${queryName}`);
      }
    });
    mutate("/savedQueries");
  };

  const onSaveQueryList = async ({
    sanitizedQueries,
    onSuccess,
    onFailure
  }: {
    sanitizedQueries: QueryType[];
    onSuccess: () => void;
    onFailure: (e: any) => void;
  }) => {
    try {
      // add queries to savedQueries and dedupe
      const newQueries = [
        ...(savedQueries || []),
        ...sanitizedQueries.filter(
          query =>
            !savedQueries?.find(savedQuery => savedQuery.name === query.name)
        )
      ];
      await patchQueries({
        queries: newQueries,
        onSuccess
      });
    } catch (e: any) {
      onFailure(e);
    }
  };
  const {
    isOpen: isSaveAsModalOpen,
    onOpen: onOpenSaveAsModal,
    onClose: onCloseSaveAsModal
  } = useDisclosure();
  return {
    onSaveAs,
    onSave,
    onSaveQueryList,
    onDelete,
    savedQueries,
    isFetchingQueries,
    isSaveAsModalOpen,
    onOpenSaveAsModal,
    onCloseSaveAsModal
  };
};

const patchQueries = async ({
  queries,
  onSuccess
}: {
  queries: QueryType[];
  onSuccess: () => void;
}) => {
  let currentUser = window.App.currentUser;
  const frontendConfig = window.frontendConfig;

  if (!frontendConfig.authenticationEnabled) {
    currentUser = "root";
  }

  const currentDB = getCurrentDB();
  try {
    await currentDB.route().request({
      method: "PATCH",
      path: `/_api/user/${encodeURIComponent(currentUser)}`,
      body: {
        extra: {
          queries
        }
      }
    });
    onSuccess();
    mutate("/savedQueries");
  } catch (e: any) {
    const message = e.message || e.response.parsedBody.errorMessage;
    window.arangoHelper.arangoError(`Could not save query. Error - ${message}`);
  }
};

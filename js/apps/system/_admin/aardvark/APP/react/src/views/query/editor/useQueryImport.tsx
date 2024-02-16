import { useState } from "react";
import { mutate } from "swr";
import { getAardvarkRouteForCurrentDb } from "../../../utils/arangoClient";
import { QueryType } from "./useFetchUserSavedQueries";

const useReadFile = () => {
  const onReadFile = ({
    file,
    onLoad
  }: {
    file: File | undefined;
    onLoad: (data: { [key: string]: any }) => void;
  }) => {
    if (!file) {
      return;
    }
    const reader = new FileReader();
    reader.readAsText(file);
    reader.onload = async () => {
      const data = reader.result;
      if (typeof data !== "string") {
        window.arangoHelper.arangoError(`Failed to parse imported queries`);
        return;
      }
      try {
        const parsedData = JSON.parse(data);
        onLoad(parsedData);
      } catch (e: any) {
        window.arangoHelper.arangoError(
          `Failed to parse imported queries: ${e.message}`
        );
      }
    };
  };
  return { onReadFile };
};

const uploadToServer = async ({
  sanitizedQueries,
  onSuccess,
  onFailure
}: {
  sanitizedQueries: QueryType[];
  onSuccess: () => void;
  onFailure: (e: any) => void;
}) => {
  const route = getAardvarkRouteForCurrentDb(
    `query/upload/${window.App.currentUser}`
  );
  try {
    await route.request({
      method: "POST",
      body: sanitizedQueries
    });
    onSuccess();
  } catch (e: any) {
    onFailure(e);
  }
};
export const useQueryImport = ({ onClose }: { onClose: () => void }) => {
  const [isUploading, setIsUploading] = useState(false);
  const { onReadFile } = useReadFile();
  const handleSuccess = async () => {
    await mutate("/savedQueries");
    window.arangoHelper.arangoNotification("Successfully uploaded queries");
    setIsUploading(false);
    onClose();
  };
  const handleFailure = (e: any) => {
    window.arangoHelper.arangoError(`Failed to upload queries: ${e.message}`);
    setIsUploading(false);
    onClose();
  };
  const onUpload = async (file: File | undefined) => {
    if (!file) {
      return;
    }
    onReadFile({
      file,
      onLoad: async (parsedData: { [key: string]: any }) => {
        const sanitizedQueries = parsedData.map((query: QueryType) => {
          return {
            name: query.name,
            value: query.value,
            parameter: query.parameter,
            created_at: Date.now()
          };
        }) as QueryType[];
        setIsUploading(true);
        // if auth is preset, upload via aardvark server
        await uploadToServer({
          sanitizedQueries,
          onSuccess: handleSuccess,
          onFailure: handleFailure
        });
      }
    });
  };
  return { onUpload, isUploading };
};

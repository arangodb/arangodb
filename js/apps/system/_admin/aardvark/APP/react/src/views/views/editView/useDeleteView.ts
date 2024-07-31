import { mutate } from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useDeleteView = ({ name }: { name: string }) => {
  const handleDelete = async () => {
    try {
      const { encoded: encodedViewName } = encodeHelper(name);
      const result = await getCurrentDB().view(encodedViewName).drop();

      if (!result) {
        window.arangoHelper.arangoError(
          "Failure",
          `Failed to delete View: ${name}`
        );
      } else {
        mutate("/view");
        window.App.navigate("#views", { trigger: true });
        window.arangoHelper.arangoNotification(
          "Success",
          `Deleted View: ${name}`
        );
      }
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return { onDelete: handleDelete };
};

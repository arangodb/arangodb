import React, { useState } from "react";
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { FormState } from "./constants";
import { pick } from "lodash";
import { IconButton } from "../../components/arango/buttons";
import { mutate } from "swr";
import { useHistory, useLocation } from "react-router-dom";

declare var arangoHelper: { [key: string]: any };
declare var window: { [key: string]: any };

type ButtonProps = {
  view: FormState;
};

type SaveButtonProps = ButtonProps & {
  oldName?: string;
  menu?: string;
  setChanged: (changed: boolean) => void;
};

type BackButtonProps = {
  disabled?: boolean;
};

export const BackButton = ({ disabled }: BackButtonProps) => {
  const history = useHistory();
  const location = useLocation();

  const up = location.pathname.slice(0, location.pathname.lastIndexOf('/'));

  return (
    <IconButton
      icon={"arrow-up"}
      onClick={() => history.push(up)}
      type={"default"}
      disabled={disabled}
    >
      Up
    </IconButton>
  );
};

export const handleSave = async ({
                                   view,
                                   oldName,
                                   menu,
                                   setChanged
                                 }: SaveButtonProps) => {
  const route = getApiRouteForCurrentDB();
  let result;
  let error = false;
  const path = `/view/${view.name}/properties`;

  try {
    if (oldName && view.name !== oldName) {
      result = await route.put(`/view/${oldName}/rename`, {
        name: view.name
      });

      if (result.body.error) {
        arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
        error = true;
      }
    }

    if (!error) {
      const properties = pick(
        view,
        "consolidationIntervalMsec",
        "commitIntervalMsec",
        "cleanupIntervalStep",
        "links",
        "consolidationPolicy"
      );
      result = await route.patch(path, properties);

      if (result.body.error) {
        arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
      } else {
        if (view.name === oldName) {
          await mutate(path);
        } else {
          let newRoute = `#view/${view.name}`;
          if (menu) {
            newRoute += `/${menu}`;
          }
          window.App.navigate(newRoute, {
            trigger: true,
            replace: true
          });
        }

        window.sessionStorage.removeItem(oldName);
        window.sessionStorage.removeItem(`${oldName}-changed`);
        setChanged(false);

        arangoHelper.arangoNotification(
          "Success",
          `Updated View: ${view.name}`
        );
      }
    }
  } catch (e) {
    arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.message}`
    );
  }
};

export const SaveButton = ({
                             view,
                             oldName,
                             menu,
                             setChanged
                           }: SaveButtonProps) => {


  return (
    <IconButton icon={"save"} onClick={() => handleSave({
      view,
      oldName,
      menu,
      setChanged
    })} type={"success"}>
      Save
    </IconButton>
  );
};

export const DeleteButton = ({ view }: ButtonProps) => {
  const [show, setShow] = useState(false);

  const handleDelete = async () => {
    try {
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${view.name}`
      );

      if (result.body.error) {
        arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
      } else {
        window.App.navigate("#views", { trigger: true });
        arangoHelper.arangoNotification(
          "Success",
          `Deleted View: ${view.name}`
        );
      }
    } catch (e) {
      arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return (
    <>
      <IconButton
        icon={"trash-o"}
        onClick={() => setShow(true)}
        type={"danger"}
      >
        Delete
      </IconButton>
      <Modal
        show={show}
        setShow={setShow}
        cid={`modal-content-delete-${view.name}`}
      >
        <ModalHeader title={`Delete View ${view.name}?`}/>
        <ModalBody>
          <p>
            Are you sure? Clicking on the <b>Delete</b> button will permanently
            delete the View.
          </p>
        </ModalBody>
        <ModalFooter>
          <button className="button-close" onClick={() => setShow(false)}>
            Close
          </button>
          <button
            className="button-danger"
            style={{ float: "right" }}
            onClick={handleDelete}
          >
            Delete
          </button>
        </ModalFooter>
      </Modal>
    </>
  );
};

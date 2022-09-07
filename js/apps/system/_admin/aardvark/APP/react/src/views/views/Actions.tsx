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

type NavButtonProps = {
  disabled?: boolean;
  arrow?: string;
  text?: string;
};

export const NavButton = ({ disabled, arrow = 'up', text = 'Up' }: NavButtonProps) => {
  const history = useHistory();
  const location = useLocation();

  const up = location.pathname.slice(0, location.pathname.lastIndexOf('/'));

  return <IconButton icon={`arrow-${arrow}`} onClick={() => history.push(up)} type={"default"}
                     disabled={disabled}>{text}</IconButton>;
};

type SaveButtonProps = ButtonProps & {
  oldName?: string;
  menu?: string;
  setChanged: (changed: boolean) => void;
};

export const SaveButton = ({
                             view,
                             oldName,
                             menu,
                             setChanged
                           }: SaveButtonProps) => {
  const handleSave = async () => {
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
        arangoHelper.arangoMessage(
          "Saving",
          `Please wait while the view is being saved. This could take some time for large views.`
        );
        const properties = pick(
          view,
          "consolidationIntervalMsec",
          "commitIntervalMsec",
          "cleanupIntervalStep",
          "links",
          "consolidationPolicy"
        );
        result = await route.patch(path, properties);
        arangoHelper.hideArangoNotifications();

        if (result.body.error) {
          arangoHelper.arangoError(
            "Failure",
            `Got unexpected server response: ${result.body.errorMessage}`
          );
        } else {
          window.sessionStorage.removeItem(oldName);
          window.sessionStorage.removeItem(`${oldName}-changed`);
          setChanged(false);

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


  return <IconButton icon={"save"} onClick={handleSave} type={"success"} style={{
    marginTop: 10,
    marginBottom: 10,
    marginLeft: 0,
    marginRight: 10
  }}>
    Save View
  </IconButton>;
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
        style={{
          marginTop: 10,
          marginBottom: 10,
          marginLeft: 0
        }}
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

import React from "react";
import { FormState } from "./constants";
import { Modal } from "../../components/modal/Modal";
import { ModalHeader } from "../../components/modal/ModalHeader";
import { ModalBody } from "../../components/modal/ModalBody";
import { ModalFooter } from "../../components/modal/ModalFooter";

export const DeleteViewModal = ({
  isOpen,
  onClose,
  view,
  onDelete
}: {
  isOpen: boolean;
  onClose: () => void;
  view: FormState;
  onDelete: () => Promise<void>;
}) => {
  return (
    <Modal isOpen={isOpen} onClose={onClose}>
      <ModalHeader>Delete view "{view.name}"</ModalHeader>
      <ModalBody>
        <p>
          Are you sure? Clicking on the <b>Delete</b> button will permanently
          delete this view.
        </p>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={onClose}>
          Close
        </button>
        <button
          className="button-danger"
          style={{ float: "right" }}
          onClick={onDelete}
        >
          Delete
        </button>
      </ModalFooter>
    </Modal>
  );
};

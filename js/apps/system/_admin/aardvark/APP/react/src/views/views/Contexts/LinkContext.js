import React, { useState, useContext } from "react";

const LinkContext = React.createContext();
const ShowContext = React.createContext();
const FieldContext = React.createContext();
const FieldUpdateContext = React.createContext();
const PropertiesContext = React.createContext();
const SetPropertiesContext = React.createContext();

export function useShow() {
  return useContext(LinkContext);
}

export function useShowUpdate() {
  return useContext(ShowContext);
}

export const useField = () => {
  return useContext(FieldContext);
}
export const useUpdateField = () => {
  return useContext(FieldUpdateContext);
}

export const useProperties = () => {
  return useContext(PropertiesContext);
}
export function LinkProvider({ children }) {
  const [show, setShow] = useState("LinkList");
  const [field, setShowField] = useState({});
  const [properties, setProperties] = useState({});

  const handleShowState = (str) => {
    setShow(str);
  }

  const handleShowField = (f) => {
    setShowField(f);
  }

  const handleProperties = (p) => {
    setProperties(p)
  }
  return (
    <LinkContext.Provider value={show}>
      <ShowContext.Provider value={handleShowState}>
        <FieldContext.Provider value={field}>
          <FieldUpdateContext.Provider value={handleShowField}>
            <PropertiesContext.Provider value={properties}>
              <SetPropertiesContext.Provider value={handleProperties}>
                {children}
              </SetPropertiesContext.Provider>
            </PropertiesContext.Provider>
          </FieldUpdateContext.Provider>
        </FieldContext.Provider>
      </ShowContext.Provider>
    </LinkContext.Provider>
  )
}

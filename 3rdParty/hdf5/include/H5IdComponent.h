// C++ informative line for the emacs editor: -*- C++ -*-
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef H5IdComponent_H
#define H5IdComponent_H

namespace H5 {

/*! \class IdComponent
    \brief Class IdComponent provides wrappers of the C functions that
     operate on an HDF5 identifier.

    In most cases, the C library handles these operations and an application
    rarely needs them.
*/
class H5_DLLCPP IdComponent {
  public:
    // Increment reference counter.
    void incRefCount(const hid_t obj_id) const;
    void incRefCount() const;

    // Decrement reference counter.
    void decRefCount(const hid_t obj_id) const;
    void decRefCount() const;

    // Get the reference counter to this identifier.
    int getCounter(const hid_t obj_id) const;
    int getCounter() const;

    // Returns an HDF5 object type, given the object id.
    static H5I_type_t getHDFObjType(const hid_t obj_id);

    // Returns an HDF5 object type of this object.
    H5I_type_t getHDFObjType() const;

    // Returns the number of members in a type.
    static hsize_t getNumMembers(H5I_type_t type);

    // Checks if the given ID is valid.
    static bool isValid(hid_t an_id);

    // Determines if an type exists.
    static bool typeExists(H5I_type_t type);

    // Assignment operator.
    IdComponent &operator=(const IdComponent &rhs);

    // Sets the identifier of this object to a new value.
    void setId(const hid_t new_id);

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Gets the identifier of this object.
    virtual hid_t getId() const = 0;

    // Pure virtual function for there are various H5*close for the
    // subclasses.
    virtual void close() = 0;

    // Makes and returns the string "<class-name>::<func_name>";
    // <class-name> is returned by fromClass().
    H5std_string inMemFunc(const char *func_name) const;

    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const
    {
        return ("IdComponent");
    }

#endif // DOXYGEN_SHOULD_SKIP_THIS

    // Destructor
    virtual ~IdComponent() = default;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

  protected:
    // Default constructor.
    IdComponent();

    // Gets the name of the file, in which an HDF5 object belongs.
    H5std_string p_get_file_name() const;

    // Verifies that the given id is valid.
    static bool p_valid_id(const hid_t obj_id);

    // Sets the identifier of this object to a new value. - this one
    // doesn't increment reference count
    virtual void p_setId(const hid_t new_id) = 0;

    // This flag is used to decide whether H5dont_atexit should be called
    static bool H5dontAtexit_called;

  private:
    // This flag indicates whether H5Library::initH5cpp has been called
    // to register various terminating functions with atexit()
    static bool H5cppinit;

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end class IdComponent
} // namespace H5

#endif // H5IdComponent_H

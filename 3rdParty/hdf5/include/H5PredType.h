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

#ifndef H5PredType_H
#define H5PredType_H

namespace H5 {

/*! \class PredType
    \brief Class PredType holds the definition of all the HDF5 predefined
    datatypes.

    These types can only be made copy of, not created by H5Tcreate or
    closed by H5Tclose.  They are treated as constants.
*/
//  Inheritance: AtomType -> DataType -> H5Object -> H5Location -> IdComponent
class H5_DLLCPP PredType : public AtomType {
  public:
    ///\brief Returns this class name.
    virtual H5std_string
    fromClass() const override
    {
        return ("PredType");
    }

    // Makes a copy of the predefined type and stores the new
    // id in the left hand side object.
    PredType &operator=(const PredType &rhs);

    // Copy constructor: same as the original PredType.
    PredType(const PredType &original);

    // Noop destructor
    virtual ~PredType() override = default;

    /*! \brief This dummy function do not inherit from DataType - it will
        throw a DataTypeIException if invoked.
    */
    void commit(H5Location &loc, const H5std_string &name);
    /*! \brief This dummy function do not inherit from DataType - it will
        throw a DataTypeIException if invoked.
    */
    void commit(H5Location &loc, const char *name);
    /*! \brief This dummy function do not inherit from DataType - it will
        throw a DataTypeIException if invoked.
    */
    bool committed();

    ///\brief PredType constants
    static const PredType &STD_I8BE;
    static const PredType &STD_I8LE;
    static const PredType &STD_I16BE;
    static const PredType &STD_I16LE;
    static const PredType &STD_I32BE;
    static const PredType &STD_I32LE;
    static const PredType &STD_I64BE;
    static const PredType &STD_I64LE;
    static const PredType &STD_U8BE;
    static const PredType &STD_U8LE;
    static const PredType &STD_U16BE;
    static const PredType &STD_U16LE;
    static const PredType &STD_U32BE;
    static const PredType &STD_U32LE;
    static const PredType &STD_U64BE;
    static const PredType &STD_U64LE;
    static const PredType &STD_B8BE;
    static const PredType &STD_B8LE;
    static const PredType &STD_B16BE;
    static const PredType &STD_B16LE;
    static const PredType &STD_B32BE;
    static const PredType &STD_B32LE;
    static const PredType &STD_B64BE;
    static const PredType &STD_B64LE;
    static const PredType &STD_REF_OBJ;
    static const PredType &STD_REF_DSETREG;

    static const PredType &C_S1;
    static const PredType &FORTRAN_S1;

    static const PredType &IEEE_F32BE;
    static const PredType &IEEE_F32LE;
    static const PredType &IEEE_F64BE;
    static const PredType &IEEE_F64LE;

    static const PredType &UNIX_D32BE;
    static const PredType &UNIX_D32LE;
    static const PredType &UNIX_D64BE;
    static const PredType &UNIX_D64LE;

    static const PredType &INTEL_I8;
    static const PredType &INTEL_I16;
    static const PredType &INTEL_I32;
    static const PredType &INTEL_I64;
    static const PredType &INTEL_U8;
    static const PredType &INTEL_U16;
    static const PredType &INTEL_U32;
    static const PredType &INTEL_U64;
    static const PredType &INTEL_B8;
    static const PredType &INTEL_B16;
    static const PredType &INTEL_B32;
    static const PredType &INTEL_B64;
    static const PredType &INTEL_F32;
    static const PredType &INTEL_F64;

    static const PredType &ALPHA_I8;
    static const PredType &ALPHA_I16;
    static const PredType &ALPHA_I32;
    static const PredType &ALPHA_I64;
    static const PredType &ALPHA_U8;
    static const PredType &ALPHA_U16;
    static const PredType &ALPHA_U32;
    static const PredType &ALPHA_U64;
    static const PredType &ALPHA_B8;
    static const PredType &ALPHA_B16;
    static const PredType &ALPHA_B32;
    static const PredType &ALPHA_B64;
    static const PredType &ALPHA_F32;
    static const PredType &ALPHA_F64;

    static const PredType &MIPS_I8;
    static const PredType &MIPS_I16;
    static const PredType &MIPS_I32;
    static const PredType &MIPS_I64;
    static const PredType &MIPS_U8;
    static const PredType &MIPS_U16;
    static const PredType &MIPS_U32;
    static const PredType &MIPS_U64;
    static const PredType &MIPS_B8;
    static const PredType &MIPS_B16;
    static const PredType &MIPS_B32;
    static const PredType &MIPS_B64;
    static const PredType &MIPS_F32;
    static const PredType &MIPS_F64;

    static const PredType &NATIVE_CHAR;
    static const PredType &NATIVE_SCHAR;
    static const PredType &NATIVE_UCHAR;
    static const PredType &NATIVE_SHORT;
    static const PredType &NATIVE_USHORT;
    static const PredType &NATIVE_INT;
    static const PredType &NATIVE_UINT;
    static const PredType &NATIVE_LONG;
    static const PredType &NATIVE_ULONG;
    static const PredType &NATIVE_LLONG;
    static const PredType &NATIVE_ULLONG;
    static const PredType &NATIVE_FLOAT;
    static const PredType &NATIVE_DOUBLE;
    static const PredType &NATIVE_LDOUBLE;
    static const PredType &NATIVE_B8;
    static const PredType &NATIVE_B16;
    static const PredType &NATIVE_B32;
    static const PredType &NATIVE_B64;
    static const PredType &NATIVE_OPAQUE;
    static const PredType &NATIVE_HSIZE;
    static const PredType &NATIVE_HSSIZE;
    static const PredType &NATIVE_HERR;
    static const PredType &NATIVE_HBOOL;

    static const PredType &NATIVE_INT8;
    static const PredType &NATIVE_UINT8;
    static const PredType &NATIVE_INT16;
    static const PredType &NATIVE_UINT16;
    static const PredType &NATIVE_INT32;
    static const PredType &NATIVE_UINT32;
    static const PredType &NATIVE_INT64;
    static const PredType &NATIVE_UINT64;

    // LEAST types
    static const PredType &NATIVE_INT_LEAST8;
    static const PredType &NATIVE_UINT_LEAST8;

    static const PredType &NATIVE_INT_LEAST16;
    static const PredType &NATIVE_UINT_LEAST16;

    static const PredType &NATIVE_INT_LEAST32;
    static const PredType &NATIVE_UINT_LEAST32;

    static const PredType &NATIVE_INT_LEAST64;
    static const PredType &NATIVE_UINT_LEAST64;

    // FAST types
    static const PredType &NATIVE_INT_FAST8;
    static const PredType &NATIVE_UINT_FAST8;

    static const PredType &NATIVE_INT_FAST16;
    static const PredType &NATIVE_UINT_FAST16;

    static const PredType &NATIVE_INT_FAST32;
    static const PredType &NATIVE_UINT_FAST32;

    static const PredType &NATIVE_INT_FAST64;
    static const PredType &NATIVE_UINT_FAST64;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    // Deletes the PredType global constants
    static void deleteConstants();

    // Dummy constant
    static const PredType &PREDTYPE_CONST; // dummy constant

  protected:
    // Default constructor
    PredType();

    // Creates a pre-defined type using an HDF5 pre-defined constant
    PredType(const hid_t predtype_id); // used by the library only

  private:
    // Activates the creation of the PredType global constants
    static PredType *getPredTypes();

    // Dynamically allocates PredType global constants
    static void makePredTypes();

    // Dummy constant
    static PredType *PREDTYPE_CONST_;

    // Declaration of pointers to constants
    static PredType *STD_I8BE_;
    static PredType *STD_I8LE_;
    static PredType *STD_I16BE_;
    static PredType *STD_I16LE_;
    static PredType *STD_I32BE_;
    static PredType *STD_I32LE_;
    static PredType *STD_I64BE_;
    static PredType *STD_I64LE_;
    static PredType *STD_U8BE_;
    static PredType *STD_U8LE_;
    static PredType *STD_U16BE_;
    static PredType *STD_U16LE_;
    static PredType *STD_U32BE_;
    static PredType *STD_U32LE_;
    static PredType *STD_U64BE_;
    static PredType *STD_U64LE_;
    static PredType *STD_B8BE_;
    static PredType *STD_B8LE_;
    static PredType *STD_B16BE_;
    static PredType *STD_B16LE_;
    static PredType *STD_B32BE_;
    static PredType *STD_B32LE_;
    static PredType *STD_B64BE_;
    static PredType *STD_B64LE_;
    static PredType *STD_REF_OBJ_;
    static PredType *STD_REF_DSETREG_;

    static PredType *C_S1_;
    static PredType *FORTRAN_S1_;

    static PredType *IEEE_F32BE_;
    static PredType *IEEE_F32LE_;
    static PredType *IEEE_F64BE_;
    static PredType *IEEE_F64LE_;

    static PredType *UNIX_D32BE_;
    static PredType *UNIX_D32LE_;
    static PredType *UNIX_D64BE_;
    static PredType *UNIX_D64LE_;

    static PredType *INTEL_I8_;
    static PredType *INTEL_I16_;
    static PredType *INTEL_I32_;
    static PredType *INTEL_I64_;
    static PredType *INTEL_U8_;
    static PredType *INTEL_U16_;
    static PredType *INTEL_U32_;
    static PredType *INTEL_U64_;
    static PredType *INTEL_B8_;
    static PredType *INTEL_B16_;
    static PredType *INTEL_B32_;
    static PredType *INTEL_B64_;
    static PredType *INTEL_F32_;
    static PredType *INTEL_F64_;

    static PredType *ALPHA_I8_;
    static PredType *ALPHA_I16_;
    static PredType *ALPHA_I32_;
    static PredType *ALPHA_I64_;
    static PredType *ALPHA_U8_;
    static PredType *ALPHA_U16_;
    static PredType *ALPHA_U32_;
    static PredType *ALPHA_U64_;
    static PredType *ALPHA_B8_;
    static PredType *ALPHA_B16_;
    static PredType *ALPHA_B32_;
    static PredType *ALPHA_B64_;
    static PredType *ALPHA_F32_;
    static PredType *ALPHA_F64_;

    static PredType *MIPS_I8_;
    static PredType *MIPS_I16_;
    static PredType *MIPS_I32_;
    static PredType *MIPS_I64_;
    static PredType *MIPS_U8_;
    static PredType *MIPS_U16_;
    static PredType *MIPS_U32_;
    static PredType *MIPS_U64_;
    static PredType *MIPS_B8_;
    static PredType *MIPS_B16_;
    static PredType *MIPS_B32_;
    static PredType *MIPS_B64_;
    static PredType *MIPS_F32_;
    static PredType *MIPS_F64_;

    static PredType *NATIVE_CHAR_;
    static PredType *NATIVE_SCHAR_;
    static PredType *NATIVE_UCHAR_;
    static PredType *NATIVE_SHORT_;
    static PredType *NATIVE_USHORT_;
    static PredType *NATIVE_INT_;
    static PredType *NATIVE_UINT_;
    static PredType *NATIVE_LONG_;
    static PredType *NATIVE_ULONG_;
    static PredType *NATIVE_LLONG_;
    static PredType *NATIVE_ULLONG_;
    static PredType *NATIVE_FLOAT_;
    static PredType *NATIVE_DOUBLE_;
    static PredType *NATIVE_LDOUBLE_;
    static PredType *NATIVE_B8_;
    static PredType *NATIVE_B16_;
    static PredType *NATIVE_B32_;
    static PredType *NATIVE_B64_;
    static PredType *NATIVE_OPAQUE_;
    static PredType *NATIVE_HSIZE_;
    static PredType *NATIVE_HSSIZE_;
    static PredType *NATIVE_HERR_;
    static PredType *NATIVE_HBOOL_;

    static PredType *NATIVE_INT8_;
    static PredType *NATIVE_UINT8_;
    static PredType *NATIVE_INT16_;
    static PredType *NATIVE_UINT16_;
    static PredType *NATIVE_INT32_;
    static PredType *NATIVE_UINT32_;
    static PredType *NATIVE_INT64_;
    static PredType *NATIVE_UINT64_;

    // LEAST types
    static PredType *NATIVE_INT_LEAST8_;
    static PredType *NATIVE_UINT_LEAST8_;

    static PredType *NATIVE_INT_LEAST16_;
    static PredType *NATIVE_UINT_LEAST16_;

    static PredType *NATIVE_INT_LEAST32_;
    static PredType *NATIVE_UINT_LEAST32_;

    static PredType *NATIVE_INT_LEAST64_;
    static PredType *NATIVE_UINT_LEAST64_;

    // FAST types
    static PredType *NATIVE_INT_FAST8_;
    static PredType *NATIVE_UINT_FAST8_;

    static PredType *NATIVE_INT_FAST16_;
    static PredType *NATIVE_UINT_FAST16_;

    static PredType *NATIVE_INT_FAST32_;
    static PredType *NATIVE_UINT_FAST32_;

    static PredType *NATIVE_INT_FAST64_;
    static PredType *NATIVE_UINT_FAST64_;

#endif // DOXYGEN_SHOULD_SKIP_THIS

}; // end of PredType
} // namespace H5

#endif // H5PredType_H

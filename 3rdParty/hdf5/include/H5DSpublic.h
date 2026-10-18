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

#ifndef H5DSpublic_H
#define H5DSpublic_H

#define DIMENSION_SCALE_CLASS "DIMENSION_SCALE"
#define DIMENSION_LIST        "DIMENSION_LIST"
#define REFERENCE_LIST        "REFERENCE_LIST"
#define DIMENSION_LABELS      "DIMENSION_LABELS"

/**
 * \brief Prototype for H5DSiterate_scales() operator
 *
 */
//! <!-- [H5DS_iterate_t_snip] -->
typedef herr_t (*H5DS_iterate_t)(hid_t dset, unsigned dim, hid_t scale, void *visitor_data);
//! <!-- [H5DS_iterate_t_snip] -->

#ifdef __cplusplus
extern "C" {
#endif

/** \page H5DS_UG HDF5 High Level Dimension Scales
 *
 * \section sec_dim_scales_stand HDF5 Standard for Dimension Scales
 * Dimension scales are stored as datasets, with additional metadata indicating that they are to
 * be treated as dimension scales. Each dimension scale has an optional name. There is no requirement
 * as to where dimension scales should be stored in the file. Dimension Scale names are not required
 * to be unique within a file. (The name of a dimension scale does not have to be the same as the HDF5
 * path name for the dataset representing the scale.)
 *
 * Datasets are linked to dimension scales.  Each dimension of a Dataset may optionally have one or
 * more associated Dimension Scales, as well as a label for the dimension.  A Dimension Scale can be
 * shared by two or more dimensions, including dimensions in the same or different dataset.
 * Relationships between dataset dimensions and their corresponding dimension scales are not be directly
 * maintained or enforced by the HDF5 library.  For instance, a dimension scale would not be automatically
 * deleted when all datasets that refer to it are deleted.
 *
 * Functions for creating and using Dimension Scales are implemented as high level functions, see \ref H5DS.
 *
 * A frequently requested feature is for Dimension Scales to be represented as functions, rather than a
 * stored array of precomputed values. To meet this requirement, it is recommended that the dataset
 * model be expanded in the future to allow datasets to be represented by a generating function.
 *
 * \section sec_dim_scales_concept Conceptual model
 * Our study of dimension scale use cases has revealed an enormous variety of ways that dimension
 * scales can be used.  We recognize the importance of having a model that will be easy to understand
 * and use for the vast majority of applications.  It is our sense that those applications will need
 * either no scale, a single 1-D array of floats or integers, or a simple function that provides a
 * scale and offset.
 *
 * At the same time, we want to place as few restrictions as possible on other uses of dimension
 * scales.  For instance, we don’t want to require dimension scales to be 1-D arrays, or to allow
 * only one scale per dimension.
 *
 * So our goal is to provide a model that serves the needs of two communities.  We want to keep
 * the dimension scale model conceptually simple for the majority of applications, but also to
 * place as few restrictions as possible how dimension scales are interpreted and used.  With
 * this approach, it becomes the responsibility of applications to make sure that dimension scales
 * satisfy the constraints of the model that they are assuming, such as constraints on the size
 * of dimension scales and valid range of indices.
 *
 * \subsection subsec_dim_scales_concept_defs Definitions
 * Dimension Scales are implemented as an extension of these objects.  In the HDF5 Abstract Data
 * Model, a Dataset has a Dataspace, which defines a multi dimensional array of elements. Conceptually,
 * a Dataspace has N dimension objects, which define the current and maximum size of the array in
 * that dimension.
 *
 * It is important to emphasize that the Dataspace of a Dataset has no intrinsic meaning except
 * to define the layout in computer storage. Dimension Scales may be used to store application
 * specific labels to the positions in the stored data array, i.e., to add application specific
 * meaning to the dimensions of the dataspace.
 *
 * A Dimension Scale is an object associated with one dimension of a Dataspace. The meaning of
 * the association is left to applications. The values of the Dimension Scale are set by the
 * application to reflect semantics of the data, for example, to associate coordinates of a
 * reference system with positions on the dimension.
 *
 * In general, these associations define a mapping between values of a dimension index and
 * values of the Dimension Scale dataset. A simple case is where the Dimension Scale s is
 * a (one dimensional) sequence of labels for the dimension ix of Dataset d. In this case,
 * Dimension Scale is an array indexed by the same index as in the dimension of the Dataspace.
 * For example, for the Dimension Scale s, associated with dimension ix, the ith position of
 * ix is associated with the value s[i], so s[i] is taken as a label for ix[i].
 *
 * \subsection subsec_dim_scales_concept_rel Entity Relationship Diagrams
 * Figure 1 shows UML to illustrate the relationship between a Dimension and a Dimension
 * Scale object. Conceptually, each Dimension of a Dataspace may have zero or more Dimension
 * Scales associated with it. In turn, a Dimension Scale object may be associated with zero
 * or more Dimensions (in zero or more Dataspaces).
 *
 * Figure 2 illustrates the abstract model for a Dimension Scale object. A Dimension Scale
 * is represented as a sub-class of a Dataset: a Dimension Scale has all the properties of
 * a Dataset, with some specializations. A Dimension Scale dataset has an attribute “CLASS”
 * with the value “DIMENSION_SCALE”. (This is analogous to the Table, Image, and Palette
 * objects.) The Dimension Scale dataset has other attributes, including an optional
 * NAME and references to any associated Dataset, as discussed below.
 *
 * When the Dimension Scale is associated with a dimension of a Dataset, the association
 * is represented by attributes of the two datasets. In the Dataset, the DIMENSION_LIST
 * is an array of object references to scales (Dimension Scale Datasets) (Figure 1), and
 * in the Dimension Scale Dataset the REFERENCE_LIST is an array of object references
 * to Datasets (Figure 2).
 * <table>
 * <tr>
 * <td>
 * \image html H5DS_fig1.png "Figure 1. The relationship between a Dimension and a Dimension Scale."
 * </td>
 * </tr>
 * </table>
 *
 * <table>
 * <tr>
 * <td>
 * \image html H5DS_fig2.png "Figure 2. The definition of a Dimension Scale and its attributes."
 * </td>
 * </tr>
 * </table>
 *
 * \subsection subsec_dim_scales_concept_types What types of scales should be implemented?
 * There seems to be good agreement that the model should accommodate scales that consist of a
 * stored 1-D list of values, certain simple functions, and “no scale.”  This specification also
 * includes scales that are higher dimensional arrays, as well.
 *
 * The four types of scales are:
 * \li <strong>No scale.</strong>  Frequently no scale is needed, so it should not be required.
 *     In some of these cases, an axis label may still be needed, and should be available.
 *     In this case, the Datase * t defines a Dimension Scale label for a dimension, with no
 *     Dimension Scale dataset.
 * \li <strong>1-D array.</strong>  Both fixed length and extendable arrays should be available.
 *     The size of the Dimension Scale is not required by HDF5 to conform to the size of the
 *     corresponding dimension(s), so that the number of scale values could be less than,
 *     equal to, or greater than the corresponding dimension size.
 * \li <strong>Simple function.</strong>  At a minimum, a linear scale of the form A + Bx
 *     should be available. This will be discussed in a future proposal.
 * \li <strong>Higher dimensional arrays.</strong> This specification allows Dimension Scales
 *     to have any number of dimensions.
 *
 * A number of use cases have been proposed in which more than one scale is needed for a given dimension.
 * This specification places no restrictions on the number of scales that can be associated with a dimension,
 * nor on the number or identities of Dimensions that may share the same Dimension Scale.
 *
 * There are use cases for storing many types of data in a scale, including,
 * but not limited to integers, floats, and strings. Therefore, this specification places no restrictions
 * on the datatypes of scale values: a Dimension Scale can have any HDF5 Datatype.  The
 * interpretation of dimension scale values is left to applications.
 *
 * \subsection subsec_dim_scales_concept_limits Limitations of this Specification
 * One-to-many mapping. When there are fewer values in a dimension scale than in the corresponding
 * dimension, it is useful to have a mapping between the two.  For example, mappings are used by
 * HDF-EOS to map geolocation information to dimension, in order that, for example, every second
 * point may have geolocation.  On the other hand, the way that mappings are defined can be very
 * idiosyncratic, and it would seem to be challenging to provide a mapping model that satisfied a
 * large number of cases.  These mappings are not included in the model specified here.
 *
 * Visibility and Integrity. Since Dimension Scales are a specialization of a Dataset, it is
 * “visible” and accessible as a regular Dataset through the HDF5 API. This means that an application
 * program could alter the values of or delete a Dimension Scale object or required attributes without
 * regard to any of the semantics defined in this document.
 *
 * One advantage it that the implementation requires no changes to the base library, which reduces
 * the complexity of the code and the risk of side-effects. The implementation builds on existing
 * functions, which should improve the quality and reliability of the code. Also, this approach leaves
 * most of the semantics of dimension scales to applications and communities, who can use the
 * specification in any way they need.
 *
 * An important disadvantage is that the core HDF5 library will not manage the semantics of
 * Dimension Scales. In particular, applications or other software must implement:
 * \li <strong>Naming</strong> – the HDF5 library will impose no rules on the names of Dimension Scales
 * \li <strong>Consistency of references</strong> – e.g., if a Dataset (Dimension Scale) is deleted (e.g.,
 *     with #H5Ldelete, any Dimension Scales (Datasets) that it refers to (refer to it)
 *     will not be updated by the HDF5 library.
 * \li <strong>Consistency of extents</strong> – the HDF5 library will not assure that a Dimension and
 *     associated Dimension Scale have the same extent (number of elements), nor that shared
 *     objects are consistent with each other. As in the case of delete, if a Dimension or
 *     Dimension Scale is extended (e.g., H5S…), any associated objects will not be
 *     automatically extended.
 *
 * These are briefly summarized here.
 * \li <strong>Naming and Name Spaces.</strong> There are many potential schemes for naming dimensions, each
 *     suited for different uses. This specification does not impose any specific approach,
 *     so it may be used by different applications. However, the lack of restrictions has
 *     disadvantages as well.<br />For some purposes, it will be important to iterate through
 *     all the Dimension Scale objects in a file. The iterate operation is difficult to
 *     implement with the design specified here. This will be left to other software. For example,
 *     the HDF-EOS library has its own mechanism for managing a set of dimensions, and the
 *     netCDF4 library will implement this if it needs to.
 * \li <strong>Automatically extending dataset dimensions.</strong> When a dimension of a dataset is
 *     extended, should the library automatically extend the corresponding dimension scale, or
 *     should this be left to the application?  Since a dimension scale can be shared among many
 *     datasets, this raises a number of issues that are difficult to address in a general way.
 *     For instance, which dimension scale should be extended when only one dataset is extended,
 *     and what values are to be added?  We have seen no compelling reason to implement an automatic
 *     extension of dimension scales when dataset dimensions are extended, so we suggest letting
 *     applications be responsible for this operation.
 * \li <strong>Automatically deleting dimension scales.</strong> Should a dimension scale be deleted
 *     when all datasets that use it have been deleted? This is another case where different applications
 *     might have different requirements, so a general policy would be difficult to devise. Furthermore,
 *     enforcing a deletion policy, even a simple one, adds complexity to the library, and could also
 *     affect performance. Deletion policies seem best left to applications.
 *
 * Section \ref sec_dim_scales_api presents an API and programming model that implements some of these
 * features. However, applications
 * may ignore or bypass these APIs, to write or read the attributes directly.
 *
 * \section sec_dim_scales_spec The HDF5 Dimension Scale Specification
 *
 * \subsection subsec_dim_scales_spec_summary Brief Summary
 * A Dimension Scale is stored as an HDF5 Dataset.
 * \li A Dimension Scale is an object that is associated with a dimension of a Dataset.
 * \li A Dimension Scale can have at most one name.
 * \li A Dimension Scale may be associated with zero, one, or many different dimensions in any number of
 * Datasets.
 * \li Unless otherwise specified, a Dimension Scale inherits the properties of an HDF5 Dataset.
 * \li There are no restrictions on the size, shape, or datatype of a Dimension Scale.
 *
 * A Dimension Scale can be associated with a dimension of an HDF5 dataset
 * \li A dimension of a Dataset may have zero, one, or more Dimension Scales associated with it.
 * \li Each scale is identified by an index value.
 *
 * A dimension may have a label without a scale, and may have a scale with no label.
 * \li The label need not be the same as the name of any associated Dimension Scales.
 *
 * The implementation has two parts:
 * \li A storage profile
 * \li An API and programming model
 *
 * \subsection subsec_dim_scales_spec_store Storage Profile
 * This section specifies the storage profile for Dimension Scale objects and the association between
 * Dimensions and Dimension Scales.
 *
 * This profile is compatible with an earlier netcdf prototype and the HDF4 to HDF5 Mapping. This
 * profile is also compatible with the netCDF4 proposal. This profile may be used to augment the
 * HDF-EOS5 profile.
 *
 * See Appendix 2 for a discussion of how to store converted HDF4 objects. See Appendix 3 for a
 * discussion of netCDF4 issues. See Appendix 4 for a discussion of HDF-EOS5.
 *
 * \subsubsection subsubsec_dim_scales_spec_store_dset Dimension Scale Dataset
 * A Dimension Scale dataset is stored as an HDF5 dataset. Table 1 summarizes the stored data, i.e.,
 * the values of the scale. There are no restrictions on the dataspace or datatype, or storage properties of
 * the dataset.
 *
 * The scale may have any HDF5 datatype, and does not have to be the same as the datatype of
 * the Dataset(s) that use the scale. E.g., an integer dataset might have dimension scales
 * that are string or float values.
 *
 * The dataspace of the scale can be any rank and shape. A scale is not limited to one dimension,
 * and is not restricted by the size of any dimension(s) associated with it. When a dimension is
 * associated with a one dimensional scale, the scale may be a different size from the dimension.
 * In this case, it is up to the application to interpret or resolve the difference. When a
 * dimension is associated with a scale with a rank higher than 1, the interpretation of the
 * association is up to the application.
 *
 * The Dimension Scale dataset can use any storage properties (including fill values, filters,
 * and storage layout), not limited by the properties of any datasets that refer to it. When
 * the Dimension Scale is extendible, it must be chunked.
 *
 * Table 2 defines the required and optional attributes of the Dimension Scale Dataset. The
 * attribute REFERENCE_LIST is a list of (dataset, index) pairs. Each pair represents an
 * association defined by ‘attach_scale’. These pairs are stored as an array of compound data.
 * Table 3 defines this datatype.
 *
 * The Dimension Scale Dataset has an attribute called SUB_CLASS. This string is intended to
 * be used to document particular specializations of this profile, e.g., a Dimension Scale
 * created by netCDF4.
 *
 * <table><caption>Table 1. The properties of the Dimension Scale dataset</caption>
 * <tr>
 * <th>Field</th>
 * <th>Datatype</th>
 * <th>Dataspace</th>
 * <th>Storage Properties</th>
 * <th>Notes</th>
 * </tr>
 * <tr>
 * <td>&lt;data&gt;</td>
 * <td>Any</td>
 * <td>Any</td>
 * <td>Any</td>
 * <td>These are the values of the Dimension Scale.</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 2. Standard Attributes for a stored Dimension Scale dataset.</caption>
 * <tr>
 * <th>Attribute Name</th>
 * <th>Datatype and Dimensions</th>
 * <th>Value</th>
 * <th>Required / Optional</th>
 * <th>Notes</th>
 * </tr>
 * <tr>
 * <td>CLASS</td>
 * <td>#H5T_STRING length = 16</td>
 * <td>“DIMENSION_SCALE”</td>
 * <td>Required</td>
 * <td>This attribute distinguishes the dataset as a Dimension scale object.<br />This is set by
 *  #H5DSset_scale</td>
 * </tr>
 * <tr>
 * <td>NAME</td>
 * <td>#H5T_STRING length = &lt;user defined&gt;</td>
 * <td>&lt;user defined&gt; <br />The name does not have to be the same as the HDF5 path name for the dataset.
 * The name does not have to be related to any labels. Several Dimension Scales may have the same name.</td>
 * <td>Optional, (Maximum of 1)</td>
 * <td>The user defined label of the Dimension Scale.<br />This is set by #H5DSset_label</td>
 * </tr>
 * <tr>
 * <td>REFERENCE_LIST</td>
 * <td>Array of Dataset Reference Type (Compound Datatype), variable length.</td>
 * <td>[ {dataset1, ind1 },   …] [,…]  ….]</td>
 * <td>Optional, required when scale is attached</td>
 * <td>See Table 3. This is set by #H5DSattach_scale.</td>
 * </tr>
 * <tr>
 * <td>SUB_CLASS</td>
 * <td>#H5T_STRING length = &lt;profile defined&gt;</td>
 * <td>“HDF4_DIMENSION”,<br />“NC4_DIMENSION”,</td>
 * <td>Optional, defined by other profiles</td>
 * <td>This is used to indicate a specific profile was used.</td>
 * </tr>
 * <tr>
 * <td>&lt;Other attributes&gt;</td>
 * <td></td>
 * <td></td>
 * <td>Optional</td>
 * <td>For example, UNITS.</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 3. Dataset Reference Type.
 * This is a pair, &lt;dataset_ref, index&gt;. This is created when the Dimension Scale is attached to a
 * Dataset.</caption>
 * <tr>
 * <th>Field</th>
 * <th>Datatype</th>
 * <th>Value</th>
 * <th>Notes</th>
 * </tr>
 * <tr>
 * <td>DATASET</td>
 * <td>Object Reference.</td>
 * <td>Pointer to a Dataset that refers to the scale</td>
 * <td>Set by #H5DSattach_scale.<br />Removed by #H5DSdetach_scale.</td>
 * </tr>
 * <tr>
 * <td>INDEX</td>
 * <td>#H5T_NATIVE_INT</td>
 * <td>Index of the dimension the dataset pointed to by DATASET</td>
 * <td>Set by #H5DSattach_scale.<br />Removed by #H5DSdetach_scale.</td>
 * </tr>
 * </table>
 *
 * \subsubsection subsubsec_dim_scales_spec_store_attr Attributes of a Dataset with a Dimension Scale
 * A Dataset may have zero or more Dimension Scales associated with its dataspace. When present,
 * these associations are represented by two attributes of the Dataset. Table 4 defines these attributes.
 *
 * The DIMENSION_LIST is a two dimensional array with one row for each dimension of the Dataset, and
 * a variable number of entries in each row, one for each associated scale. This is stored as a one
 * dimensional array, with the HDF5 Datatype variable length array of object references.
 *
 * When a dimension has more than one scale, the order of the scales in the DIMENSION_LIST attribute
 * is not defined. A given Dimension Scale should appear in the list only once.
 * (I.E., the DIMENSION_LIST is a “set”.)
 *
 * When a scale is shared by more than one dimension (of one or more Dataset), the order of
 * the records in REFERENCE_LIST is not defined. The Dataset and Dimension should appear in the list only
 * once.
 *
 * <table><caption>Table 4. Standard Attributes of a Dataset with associated Dimension Scale.</caption>
 * <tr>
 * <th>Attribute Name</th>
 * <th>Datatype and Dimensions</th>
 * <th>Value</th>
 * <th>Required / Optional</th>
 * <th>Notes</th>
 * </tr>
 * <tr>
 * <td>DIMENSION_LIST</td>
 * <td>The HDF5 datatype is ARRAY of Variable Length #H5T_STD_REF_OBJ with rank of the dataspace.</td>
 * <td>[[{object__ref1, object__ref2, … object__refn}, …] […]  ..]</td>
 * <td>Optional, required if scales are attached</td>
 * <td>Set by #H5DSattach_scale.<br />Entries removed by #H5DSdetach_scale.</td>
 * </tr>
 * <tr>
 * <td>DIMENSION_LABELLIST</td>
 * <td>The HDF5 datatype is ARRAY of #H5T_STRING with rank of the dataspace.</td>
 * <td>[ &lt;Label1&gt;, &lt;Label2&gt;, …, &lt;Label3&gt;] </td>
 * <td>Optional, required for scales with a label</td>
 * <td>Set by #H5DSset_label.</td>
 * </tr>
 * </table>
 *
 * \subsection subsec_dim_scales_spec_lab Dimension Scale Names and Labels
 * Dimension scales are often referred to by name, so Dimension Scales may have names.
 * Since some applications do not wish to apply names to dimension scales, Dimension Scale
 * names be optional. In addition, some applications will have a name but no associated data
 * values for a dimension (i.e., just a label). To support this, each dimension may have a
 * label, which may be but need not be the same as the name of an associated Dimension Scale.
 *
 * <strong>Dimension Scale Name.</strong> Associated with the Dimension Scale object. A Dimension Scale may
 * have no name, or one name.
 *
 * <strong>Dimension Label.</strong> A optional label associated with a dimension of a Dataset.
 *
 * <strong>How is a name represented?</strong>  Three options seem reasonable:
 * \li Last link in the pathname. The h4toh5 mapping uses this approach [6], but there could be more
 *     than one path to a dataset, leading to ambiguities.  This could be overcome by enforcing conventions.
 * \li Attribute. This exposes this information at the top level, making it accessible to any viewer that
 *     can see attributes.  It also makes it easy for applications to change the name, which could be
 *     dangerous, or valuable.
 * \li Header message.  This approach makes the name a little less available at the top level,
 *     but firmly pushes the concept into the base format and library.  Since it also requires
 *     applications to change the name through a function call, it leaves open the possibility
 *     that the form of the name could be altered later without requiring application codes
 *     to change.  On the other hand, if we treat names this way, it means that the “name”
 *     attribute is being treated differently from the “class” attribute, which could be confusing.
 *
 * Dimension Scale names are stored in attributes of the Dimension Scale or the Dataset that refers
 * to a Dimension Scale.
 *
 * <strong>Should dimension scale names be unique among dimension scales within a file?</strong>
 * We have seen a number of cases in which applications need more than one dimension scale with
 * the same name.  We have also seen applications where the opposite is true: dimension scale
 * names are assumed to be unique within a file.  This specification leaves it to applications
 * to enforce a policy of uniqueness when they need it.
 *
 * <strong>Can a dimension have a label, without having an associated scale?</strong> Some
 * applications may wish to name dimensions without having an associated scale.  Therefore,
 * a dataset may have a label for a dimension without having an associated Dimension Scale dataset.
 *
 * <strong>Can a dimension have a scale, without having an associated label?</strong> Some
 * applications may wish to assign a dimension scale with no label.  Therefore, a dataset
 * may have one or more Dimension Scales for a dimension without having an associated label.
 *
 * <strong>Anonymous Dimensions.</strong> It is possible to have a Dimension Scale dataset
 * with no name, and associate it with a dimension of a dataset with no label.  This case
 * associates an array of data values to the dimension, but no identifier.
 *
 * <strong>A dimension with a label and a name.</strong> A dimension of a dataset can be
 * associated with a Dimension Scale that has a name, and assigned a label. In this case,
 * the association has two “names”, the label and the dimension scale name. It is up to
 * applications to interpret these names.
 *
 * Table 5 summarizes the six possible combinations of label and name.
 *
 * <table><caption>Table 5. Labels and scales of a dimension.</caption>
 * <tr>
 * <th></th>
 * <th>No scale</th>
 * <th>Scale with no name</th>
 * <th>Scale with name</th>
 * </tr>
 * <tr>
 * <td>No label</td>
 * <td>Dimension has no label or scale (default)</td>
 * <td>Dimension has an anonymous scale</td>
 * <td>Dimension has scale, the scale is called “name”</td>
 * </tr>
 * <tr>
 * <td>Label</td>
 * <td>Dimension has label</td>
 * <td>Dimension has scale with a label.</td>
 * <td>Dimension has scale with both a label and name.  A shared dimension has one name,
 * but may have several labels</td>
 * </tr>
 * </table>
 *
 * \subsection subsec_dim_scales_spec_shared Shared Dimension Scales
 * Given the design described above, datasets can share dimension scales. The following
 * additional capabilities would seem to be useful.
 * \li When a dimension scale is deleted, remove the reference to the dimension scale in
 *     all datasets that refer to it.
 * \li Determine how many datasets are attached to a given dimension scale
 * \li Determine what datasets are attached to a given dimension scale
 *
 * These capabilities can be provided in several ways:
 * \li Back pointers.  If every dimension scale contained a list of back pointers to all
 *     datasets that referenced it, then it would be relatively easy to open all of these
 *     datasets and remove the references, as well as to answer questions #2 and #3.
 *     This would require the library to update the back pointer list every time a link
 *     was made.
 * \li Alternatively, such lists could be maintained in a separate table. Such a table
 *     could contain all information about a number of dimension scales, which might provide
 *     a convenient way for applications to gain information about a set of dimension scales.
 *     For instance, this table might correspond to the coordinate variable definitions in a netCDF file.
 * \li If no such list were available, an HDF5 function could be available to search all datasets
 *     to determine which ones referenced a given dimension scale.  This would be straightforward,
 *     but in some cases could be very time consuming.
 *
 * This specification defines attributes that maintain back pointers, which enable
 * these kinds of cross referencing. Other software, such as NetCDF4, may well need a global table to
 * track a set of dimensions. Such a table can be done in addition to the attributes defined here.
 *
 * \subsection subsec_dim_scales_spec_ex Example
 * This section presents an example to illustrate the data structures defined above.
 *
 * Figure 3 shows a Dataset with a four dimensional Dataspace. The file also contains six Dimension
 * Scale datasets. The Dimension Scale datasets are HDF5 objects, with path names such as “/DS1”.
 *
 * Figure 4 illustrates the use of dimension scales in this example. Each Dimension Scale Dataset
 * has an optional NAME.  For example, “/DS3” has been assigned the name “Scale3”.
 *
 * The dimensions of dataset D have been assigned zero or more scales and labels. Dimension 0
 * has two scales, Dimension 1 has one scale, and so on. Dimension 2 has no scale associated with it.
 *
 * Some of the dimensions have labels as well. Note that dimension 2 has a label but no scale, and
 * dimension 3 has scales but no label.
 *
 * Some of the Dimension Scales are shared. Dimension Scale DS1 is referenced by dimension 0 of D
 * and by another unspecified dataset. Dimension Scale DS3 is referenced by dimension 1 and 3 of Dataset D.
 *
 * These relationships are represented in the file by attributes of the Dataset D and the Dimension
 * Scale Datasets. Figure 5 shows the values that are stored for the DIMENSION_LIST attribute of
 * Dataset D. This
 * <table>
 * <tr>
 * <td>
 * \image html UML_Attribute.jpg "The UML model for an HDF5 attribute"
 * </td>
 * </tr>
 * </table>
 * attribute is a one-dimensional array with the HDF5 datatype variable length
 * #H5T_STD_REF_OBJ. Each row of the array is zero or more object references for Dimension Scale datasets.
 *
 * Table 6 shows the DIMENSION_LABELLIST for Dataset D. This is a one dimensional array with some empty
 * values.
 *
 * Each of the Dimension Scale Datasets has a name and other attributes. The references are
 * represented in the REFERENCE_LIST attributes. Table 7 – Table 10 show the values for these
 * tables. Note that Dimension Scale DS4 and DS6 have no references to them in this diagram.
 *
 * The tables are stored as attributes of the Dimension Scale Dataset and the Datasets that
 * refer to scales. Essentially, the association between a dimension of a Dataset and a Dimension
 * Scale is represented by “pointers” (i.e., HDF5 Object References) in both of the associated
 * objects. Since there can be multiple associations, there can be multiple pointers stored at
 * each object, representing the endpoints of the associations. These will be stored in tables,
 * i.e., as an attribute with an array of values.
 *
 * When dimension scales are attached or detached, the tables in the Dataset and the Dimension
 * Scale must be updated. The arrays in the attributes can grow, and items can be deleted.
 *
 * The associations are identified by the object reference and dimension which is stored in a
 * back pointer and returned from an API. The detach function needs to be careful how it deletes
 * an item from the table, because the entries at both ends of the association must be updated
 * at the same time.
 *
 * <table>
 * <tr>
 * <td>
 * \image html H5DS_fig3.png "Figure 3. Example dataset and scales."
 * </td>
 * </tr>
 * </table>
 *
 * <table>
 * <tr>
 * <td>
 * \image html H5DS_fig4.png "Figure 4. Example labels, names, and attached scales."
 * </td>
 * </tr>
 * </table>
 *
 * <table>
 * <tr>
 * <td>
 * \image html H5DS_fig5.png "Figure 5. The table of dimension references, stored as an attribute of the
 * Dataset."
 * </td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 6. The table of dimension labels.</caption>
 * <tr>
 * <th>Dataset Dimension</th>
 * <th>Label</th>
 * </tr>
 * <tr>
 * <td>0</td>
 * <td>“LX”</td>
 * </tr>
 * <tr>
 * <td>1</td>
 * <td>“LZ”</td>
 * </tr>
 * <tr>
 * <td>2</td>
 * <td>“LQ”</td>
 * </tr>
 * <tr>
 * <td>3</td>
 * <td>“”</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 7. The reference list for DS1.</caption>
 * <tr>
 * <th>Reference</th>
 * <th>Dataset Reference Record</th>
 * </tr>
 * <tr>
 * <td>0</td>
 * <td>{Object reference to Dataset D, 0}</td>
 * </tr>
 * <tr>
 * <td>1</td>
 * <td>{Object reference to other Dataset, ?}</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 8. Reference list for DS2</caption>
 * <tr>
 * <th>Reference</th>
 * <th>Dataset Reference Record</th>
 * </tr>
 * <tr>
 * <td>0</td>
 * <td>{Object reference to Dataset D, 0}</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 9. Reference list for DS3</caption>
 * <tr>
 * <th>Reference</th>
 * <th>Dataset Reference Record</th>
 * </tr>
 * <tr>
 * <td>0</td>
 * <td>{Object reference to Dataset D, 1}</td>
 * </tr>
 * <tr>
 * <td>1</td>
 * <td>{Object reference to Dataset D, 3}</td>
 * </tr>
 * </table>
 *
 * <table><caption>Table 10. Reference List for DS5</caption>
 * <tr>
 * <th>Reference</th>
 * <th>Dataset Reference Record</th>
 * </tr>
 * <tr>
 * <td>0</td>
 * <td>{Object reference to Dataset D, 3}</td>
 * </tr>
 * </table>
 *
 * \section sec_dim_scales_api Programming Model and API
 *
 * \subsection subsec_dim_scales_api_model Programming Model
 * Dimension Scales are HDF5 Datasets, so they may be created and accesses through any
 * HDF5 API for datasets [10]. The HDF5 Dimension Scale API implements the specification
 * defined in this document. The operations include:
 * \li Convert dataset to scale (D) – convert dataset D to a dimension scale. D may be
 *     specified by an id or by a path name.
 * \li Attach scale (D, S, i) – attach dimension scale S to the ith dimension of D. D
 *     and S may be specified by an id or path name
 * \li Detach scale (D, i, scale) – detach scale from the ith dimension of D.
 * \li Iterate through scales of (D, i) – get each scale attached.
 * \li Get the number of scales of (D, i)
 * \li Get the ith scale of D
 * \li Set/Get name (S) – set/get the name about dimension scale S.
 *
 * The API also defines operations for dimension labels:
 * \li Set/Get label (D, i) – set/get the label for ith dimension of D.
 *
 * \subsubsection subsubsec_dim_scales_api_model_create Create new Dimension Scale with Initial Values
 *    1. Create dataset with for the Dimension Scale with H5Dcreate and other standard HDF5 calls.
 *    2. Initialize the values of the Dimension Scale with H5Dwrite and other calls.
 *    3. Convert the dataset to a Dimension Scale with H5DSmake_scale.
 *    4. Close the Dimension Scale when finished with H5Dclose.
 *
 * \subsubsection subsubsec_dim_scales_api_model_attach Attach Dimension Scale to Dataset
 *    1. Create or open the Dataset, D, with H5Dopen, etc.
 *    2. Create or open the Dimension Scale dataset, S, with H5Dopen or as above.
 *    3. Attach the Dimension Scale S to dimension j of Dataset D with H5DSattach_scale
 *    4. When finished, close the Dimension Scale and Dataset with H5Dclose.
 *
 * \subsubsection subsubsec_dim_scales_api_model_read Read Dimension Scale values
 *    1. Open the Dataset D, with H5Dopen
 *    2. Get the number of dimensions of D with H5Dget_space.
 *    3. Iterate through the scales of dimension i, locate the target scale, S (e.g., by its name).
 *    4. Get the datatype, dataspace, etc. of S with H5Dget_space, H5Dget_type, H5Sget_ndims, etc.
 *    5. Read the values of S into memory with H5Dread, e.g. into dscalebuff.
 *    6. When finished, close the S and other objects with H5Dclose etc.
 *    7. When finished, close the Dataset D with H5Dclose.
 *
 * \subsubsection subsubsec_dim_scales_api_model_write Write or Update Dimension Scale values
 *    1. Open the Dimension Scale Dataset S with H5open
 *    2. Get the datatype, dataspace, etc. of S with H5Dget_space, H5Dget_type, H5Sget_ndims, etc.
 *    3. If needed, read the values of S into memory with H5Dread. Note, may read selected values using a
 * selection.
 *    4. Write updated values to S with H5Dwrite. Note, may write selected values using a selection.
 *    5. When finished, close S and other objects with H5Dclose etc.
 *
 * \subsubsection subsubsec_dim_scales_api_model_label Create a label for a dimension
 *    1. Open the Dataset D with H5open
 *    2. Add write a label for dimension i of D, with H5DSset_label.
 *    3. When finished, close the Dimension Scale Dataset and other objects with H5Dclose etc.
 *
 * \subsubsection subsubsec_dim_scales_api_model_extend Extending a Dimension with a Dimension Scale attached
 * When an extendible Dataset has Dimension Scales, it is necessary to coordinate when the dimensions change
 * size.
 *    1. Open the Dataset to be extended, with H5Dopen.
 *    2. Extend the dimension(s) with H5Dextend
 *    3. Iterate through the scales of each extended dimension. For each scale
 *        a. Extend the scale to the new size of the dimension with H5Dextend
 *        b. Write new values to the extended scale with H5Dwrite, etc.
 *        c. Close the Dimension Scale Dataset with H5Dclose if necessary.
 *    4. When finished, close the Dataset with H5Dclose
 *
 * \subsubsection subsubsec_dim_scales_api_model_detach Detach Dimension Scale from Dataset
 * The detach operation removes an association between a dimension and a scale.  It does not delete the
 * Dimension Scale Dataset.
 *    1. Open the Dataset, D, with H5Dopen.
 *    2. Iterate through the scales of dimension i, locate the target scale, S (e.g., by its name).
 *    3. Detach the Dimension Scale S to dimension j of Dataset D with H5DSdetach_scale
 *    4. When finished, close the Dimension Scale and Dataset with H5Dclose.
 *
 * \subsubsection subsubsec_dim_scales_api_model_del Delete a Dimension Scale Dataset
 * When it is necessary to delete a Dimension Scale Dataset, it is necessary to detach it from all dataset.
 * This section outlines the necessary steps.
 *    1. Open the Dimension Scale to be deleted.
 *    2. Read the REFERENCE_LIST attribute into memory with H5Aread etc.
 *    3. For each entry in the list:
 *        a. Dereference the dataset reference
 *        b. Detach the scale with H5DSdetach_scale
 *        c. Close the dataset reference
 *    4. Delete the Dimension Scale Dataset
 *
 * \subsubsection subsubsec_dim_scales_api_model_clean Clean up Dimension Scales when deleting a Dataset
 * When it is necessary to delete a dataset that has scales attached, it is necessary to delete all the
 * scales before deleting the dataset.  Here is a sketch of the steps.
 *    1. Open the Dataset to be deleted, with H5Dopen.
 *    2. Iterate through the scales of each dimension of D
 *    3. For each scale, detach the Dimension Scale S from dimension j of Dataset D with H5DSdetach_scale
 *    4. Delete the Dataset, with H5Gunlink.
 *
 * \subsection subsec_dim_scales_api_func Programming API: H5DS
 *  @see H5DS Reference Manual
 *
 * @todo Under Construction
 */

/**\defgroup H5DS HDF5 Dimension Scales APIs (H5DS)
 *
 * <em>Creating and manipulating HDF5 datasets that are associated with
 * the dimension of another HDF5 dataset (H5DS)</em>
 *
 * \note \Bold{Programming hints:}
 * \note To use any of these functions or subroutines,
 *       you must first include the relevant include file (C) or
 *       module (Fortran) in your application.
 * \note The following line includes the HDF5 Dimension Scale package,
 *       H5DS, in C applications:
 *       \code #include "hdf5_hl.h" \endcode
 * \note This line includes the H5DS module in Fortran applications:
 *       \code use h5ds \endcode
 *
 * - \ref H5DSwith_new_ref
 *   \n Determines if new references are used with dimension scales.
 * - \ref H5DSattach_scale
 *   \n Attach dimension scale dsid to dimension idx of dataset did.
 * - \ref H5DSdetach_scale
 *   \n Detach dimension scale dsid from the dimension idx of Dataset did.
 * - \ref H5DSget_label
 *   \n Read the label for dimension idx of did into buffer label.
 * - \ref H5DSget_num_scales
 *   \n Determines how many Dimension Scales are attached
 *      to dimension idx of did.
 * - \ref H5DSget_scale_name
 *   \n Retrieves name of scale did into buffer name.
 * - \ref H5DSis_attached
 *   \n Report if dimension scale dsid is currently attached
 *      to dimension idx of dataset did.
 * - \ref H5DSis_scale
 *   \n Determines whether dset is a Dimension Scale.
 * - \ref H5DSiterate_scales
 *   \n Iterates the operation visitor through the scales
 *      attached to dimension dim.
 * - \ref H5DSset_label
 *   \n Set label for the dimension idx of did to the value label.
 * - \ref H5DSset_scale
 *   \n Convert dataset dsid to a dimension scale,
 *      with optional name, dimname.
 *
 */

/* THIS IS A NEW ROUTINE NOT ON OLD PORTAL */
/**
 *  --------------------------------------------------------------------------
 *  \ingroup H5DS
 *
 *  \brief Determines if new references are used with dimension scales.
 *
 *  \param[in] obj_id        Object identifier
 *  \param[out] with_new_ref New references are used or not
 *
 *  \return \herr_t
 *
 *  \details H5DSwith_new_ref() takes any object identifier and checks
 *           if new references are used for dimension scales. Currently,
 *           new references are used when non-native VOL connector is
 *           used or when H5_DIMENSION_SCALES_WITH_NEW_REF is set up
 *           via configure option.
 *
 */
H5_HLDLL herr_t H5DSwith_new_ref(hid_t obj_id, hbool_t *with_new_ref);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Attach dimension scale \p dsid to dimension \p idx of
 *        dataset did.
 *
 * \param[in] did   The dataset
 * \param[in] dsid  The scale to be attached
 * \param[in] idx   The dimension of \p did that \p dsid is associated with
 *
 * \return \herr_t
 *
 * \details Define Dimension Scale \p dsid to be associated with
 *          dimension \p idx of dataset \p did.
 *
 *          Entries are created in the #DIMENSION_LIST and
 *          #REFERENCE_LIST attributes, as defined in \ref subsec_dim_scales_spec_store section of
 *          the \ref H5DS_UG.
 *
 *          Fails if:
 *          - Bad arguments
 *          - If \p dsid is not a Dimension Scale
 *          - If \p did is a Dimension Scale
 *            (A Dimension Scale cannot have scales.)
 *
 * \note The Dimension Scale \p dsid can be attached to the
 *       same dimension more than once, which has no effect.
 */
H5_HLDLL herr_t H5DSattach_scale(hid_t did, hid_t dsid, unsigned int idx);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Detach dimension scale \p dsid from the dimension \p idx of dataset \p did.
 *
 * \param[in] did   The dataset
 * \param[in] dsid  The scale to be detached
 * \param[in] idx   The dimension of \p did to detach
 *
 * \return \herr_t
 *
 * \details If possible, deletes association of Dimension Scale \p dsid with
 *          dimension \p idx of dataset \p did. This deletes the entries in the
 *          #DIMENSION_LIST and #REFERENCE_LIST attributes,
 *          as defined in \ref subsec_dim_scales_spec_store section of
 *          the \ref H5DS_UG.
 *
 *          Fails if:
 *          - Bad arguments
 *          - The dataset \p did or \p dsid do not exist
 *          - The \p dsid is not a Dimension Scale
 *          - \p dsid is not attached to \p did
 *
 * \note A scale may be associated with more than dimension of the
 *       same dataset. If so, the detach operation only deletes one
 *       of the associations, for \p did.
 *
 */
H5_HLDLL herr_t H5DSdetach_scale(hid_t did, hid_t dsid, unsigned int idx);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Convert dataset \p dsid to a dimension scale,
 *        with optional name, \p dimname.
 *
 * \param[in] dsid      The dataset to be made a Dimemsion Scale
 * \param[in] dimname   The dimension name (optional), NULL if the
 *                      dimension has no name.
 *
 * \return \herr_t
 *
 * \details The dataset \p dsid is converted to a Dimension Scale dataset,
 *          as defined above. Creates the CLASS attribute, set to the value
 *          "DIMENSION_SCALE" and an empty #REFERENCE_LIST attribute,
 *          as described in \ref subsec_dim_scales_spec_store section of
 *          the \ref H5DS_UG.
 *
 *          If \p dimname is specified, then an attribute called NAME
 *          is created, with the value \p dimname.
 *
 *          Fails if:
 *          - Bad arguments
 *          - If \p dsid is already a scale
 *          - If \p dsid is a dataset which already has dimension scales
 *
 *          If the dataset was created with the Table, Image, or Palette interface [9],
 *          it is not recommended to convert to a Dimension Scale.
 *          (These Datasets will have a CLASS Table, Image, or Palette.)
 *
 * \todo what is [9] after Palette interface?
 */
H5_HLDLL herr_t H5DSset_scale(hid_t dsid, const char *dimname);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Determines how many Dimension Scales are attached
 *        to dimension \p idx of \p did.
 *
 * \param[in] did   The dataset to query
 * \param[in] idx   The dimension of \p did to query
 *
 * \return Returns the number of Dimension Scales associated
 *         with \p did, if successful, otherwise returns a
 *         negative value.
 *
 * \details H5DSget_num_scales() determines how many Dimension
 *          Scales are attached to dimension \p idx of
 *          dataset \p did.
 *
 */
H5_HLDLL int H5DSget_num_scales(hid_t did, unsigned int idx);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Set label for the dimension \p idx of \p did
 *        to the value \p label.
 *
 * \param[in] did   The dataset
 * \param[in] idx   The dimension
 * \param[in] label The label
 *
 * \return  \herr_t
 *
 * \details Sets the #DIMENSION_LABELS for dimension \p idx of
 *          dataset \p did. If the dimension had a label,
 *          the new value replaces the old.
 *
 *          Fails if:
 *          - Bad arguments
 *
 */
H5_HLDLL herr_t H5DSset_label(hid_t did, unsigned int idx, const char *label);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Read the label for dimension \p idx of \p did into buffer \p label.
 *
 * \param[in] did       The dataset
 * \param[in] idx       The dimension
 * \param[out] label    The label
 * \param[in] size      The length of the label buffer
 *
 * \return  Upon success, size of label or zero if no label found.
 *          Negative if fail.
 *
 * \details Returns the value of the #DIMENSION_LABELS for
 *          dimension \p idx of dataset \p did, if set.
 *          Up to \p size characters of the name are copied into
 *          the buffer \p label.  If the label is longer than
 *          \p size, it will be truncated to fit.  The parameter
 *          \p size is set to the size of the returned \p label.
 *
 *          If \p did has no label, the return value of
 *          \p label is NULL.
 *
 *          Fails if:
 *          - Bad arguments
 *
 */
H5_HLDLL ssize_t H5DSget_label(hid_t did, unsigned int idx, char *label, size_t size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Retrieves name of scale \p did into buffer \p name.
 *
 * \param[in] did       Dimension scale identifier
 * \param[out] name     Buffer to contain the returned name
 * \param[in] size      Size in bytes, of the \p name buffer
 *
 * \return  Upon success, the length of the scale name or zero if no name found.
 *          Negative if fail.
 *
 * \details H5DSget_scale_name() retrieves the name attribute
 *          for scale \p did.
 *
 *          Up to \p size characters of the scale name are returned
 *          in \p name; additional characters, if any, are not returned
 *          to the user application.
 *
 *          If the length of the name, which determines the required value of
 *          \p size, is unknown, a preliminary H5DSget_scale_name() call can
 *          be made by setting \p name to NULL. The return value of this call
 *          will be the size of the scale name; that value plus one (1) can then
 *          be assigned to \p size for a second H5DSget_scale_name() call,
 *          which will retrieve the actual name.  (The value passed in with the
 *          parameter \p size must be one greater than size in bytes of the actual
 *          name in order to accommodate the null terminator;
 *          if \p size is set to the exact size of the name, the last byte
 *          passed back will contain the null terminator and the last character
 *          will be missing from the name passed back to the calling application.)
 */
H5_HLDLL ssize_t H5DSget_scale_name(hid_t did, char *name, size_t size);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Determines whether \p did is a Dimension Scale.
 *
 * \param[in] did   The dataset to query
 *
 * \return  \htri_t
 *
 * \details H5DSis_scale() determines if \p did is a Dimension Scale,
 *          i.e., has class="DIMENSION_SCALE").
 *
 */
H5_HLDLL htri_t H5DSis_scale(hid_t did);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Iterates the operation visitor through the scales
 *        attached to dimension \p dim.
 *
 * \param[in]       did             The dataset
 * \param[in]       dim             The dimension of dataset \p did
 * \param[in,out]   idx             Input the index to start iterating,
 *                                  output the next index to visit.
 *                                  If NULL, start at the first position.
 * \param[in]       visitor         The visitor function
 * \param[in]       visitor_data    Arbitrary data to pass to the
 *                                  visitor function
 *
 * \return  Returns the return value of the last operator if it was
 *          non-zero, or zero if all scales were processed.
 *
 * \details H5DSiterate_scales() iterates over the scales attached to
 *          dimension \p dim of dataset \p did. For each scale in the
 *          list, the \p visitor_data and some additional information,
 *          specified below, are passed to the \p visitor function.
 *          The iteration begins with the \p idx object in the
 *          group and the next element to be processed by the operator
 *          is returned in \p idx. If \p idx is NULL, then the
 *          iterator starts at the first group member; since no
 *          stopping point is returned in this case,
 *          the iterator cannot be restarted if one of the calls
 *          to its operator returns non-zero.
 *
 *          The prototype for \ref H5DS_iterate_t is:
 *          \snippet this H5DS_iterate_t_snip
 *
 *          The operation receives the Dimension Scale dataset
 *          identifier, \p scale, and the pointer to the operator
 *          data passed in to H5DSiterate_scales(), \p visitor_data.
 *
 *          The return values from an operator are:
 *
 *          - Zero causes the iterator to continue, returning zero
 *            when all group members have been processed.
 *          - Positive causes the iterator to immediately return that
 *            positive value, indicating short-circuit success.
 *            The iterator can be restarted at the next group member.
 *          - Negative causes the iterator to immediately return
 *            that value, indicating failure. The iterator can be
 *            restarted at the next group member.
 *
 *          H5DSiterate_scales() assumes that the scales of the
 *          dimension identified by \p dim remain unchanged through
 *          the iteration. If the membership changes during the iteration,
 *          the function's behavior is undefined.
 */
H5_HLDLL herr_t H5DSiterate_scales(hid_t did, unsigned int dim, int *idx, H5DS_iterate_t visitor,
                                   void *visitor_data);

/**
 * --------------------------------------------------------------------------
 * \ingroup H5DS
 *
 * \brief Report if dimension scale \p dsid is currently attached to
 *        dimension \p idx of dataset \p did.
 *
 * \param[in] did   The dataset
 * \param[in] dsid  The scale to be attached
 * \param[in] idx   The dimension of \p did that \p dsid is associated with
 *
 * \return  \htri_t
 *
 * \details Report if dimension scale \p dsid is currently attached to
 *          dimension \p idx of dataset \p did.
 *
 *          Fails if:
 *          - Bad arguments
 *          - If \p dsid is not a Dimension Scale
 *          - The \p dsid is not a Dimension Scale
 *          - If \p did is a Dimension Scale (A Dimension Scale cannot have scales.)
 *
 */
H5_HLDLL htri_t H5DSis_attached(hid_t did, hid_t dsid, unsigned int idx);

#ifdef __cplusplus
}
#endif

#endif

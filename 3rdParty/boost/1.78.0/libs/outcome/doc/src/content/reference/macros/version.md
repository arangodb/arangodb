+++
title = "Version macros"
+++

*Overridable*: None of the following macros are overridable.

*Header*: `<boost/outcome/config.hpp>`

- <a name="version-major"></a>`BOOST_OUTCOME_VERSION_MAJOR <number>`

    Major version for cmake and DLL version stamping.
 
- <a name="version-minor"></a>`BOOST_OUTCOME_VERSION_MINOR <number>`

    Minor version for cmake and DLL version stamping.

- <a name="version-patch"></a>`BOOST_OUTCOME_VERSION_PATCH <number>`

    Patch version for cmake and DLL version stamping.
 
- <a name="version-revision"></a>`BOOST_OUTCOME_VERSION_REVISION <number>`

    Revision version for cmake and DLL version stamping.

- <a name="unstable-version"></a>`BOOST_OUTCOME_UNSTABLE_VERSION <number>`

    Defined between stable releases of Outcome. It means the inline namespace will be permuted per-commit to ensure ABI uniqueness such that multiple versions of Outcome in a single process space cannot collide.

- <a name="v2"></a>`BOOST_OUTCOME_V2 <tokens>`

    The namespace configuration of this Outcome v2. Consists of a sequence of bracketed tokens later fused by the preprocessor into namespace and C++ module names.
    
- <a name="v2-namespace"></a>`BOOST_OUTCOME_V2_NAMESPACE <identifier>`

    The Outcome namespace, which may be permuted per SHA commit. This is not fully qualified.

- <a name="v2-namespace-begin"></a>`BOOST_OUTCOME_V2_NAMESPACE_BEGIN <keywords and identifiers>`

    Expands into the appropriate namespace markup to enter the Outcome v2 namespace.

- <a name="v2-namespace-export-begin"></a>`BOOST_OUTCOME_V2_NAMESPACE_EXPORT_BEGIN <keywords and identifiers>`

    Expands into the appropriate namespace markup to enter the C++ module exported Outcome v2 namespace.

- <a name="v2-namespace-end"></a>`BOOST_OUTCOME_V2_NAMESPACE_END <keywords and identifiers>`

    Expands into the appropriate namespace markup to exit the Outcome v2 namespace.


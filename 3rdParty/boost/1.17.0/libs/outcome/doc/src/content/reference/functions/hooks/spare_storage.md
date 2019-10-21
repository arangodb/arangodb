+++
title = "`uint16_t spare_storage(const basic_result|basic_outcome *) noexcept`"
description = "Returns the sixteen bits of spare storage in the specified result or outcome."
+++

Returns the sixteen bits of spare storage in the specified result or outcome. You can set these bits using {{% api "void set_spare_storage(basic_result|basic_outcome *, uint16_t) noexcept" %}}.

*Overridable*: Not overridable.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_result.hpp>`

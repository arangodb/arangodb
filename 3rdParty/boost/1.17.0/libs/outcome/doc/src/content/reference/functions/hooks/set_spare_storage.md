+++
title = "`void set_spare_storage(basic_result|basic_outcome *, uint16_t) noexcept`"
description = "Sets the sixteen bits of spare storage in the specified result or outcome."
+++

Sets the sixteen bits of spare storage in the specified result or outcome. You can retrieve these bits later using {{% api "uint16_t spare_storage(const basic_result|basic_outcome *) noexcept" %}}.

*Overridable*: Not overridable.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_result.hpp>`

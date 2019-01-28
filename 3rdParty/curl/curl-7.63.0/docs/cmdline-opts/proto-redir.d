Long: proto-redir
Arg: <protocols>
Help: Enable/disable PROTOCOLS on redirect
Added: 7.20.2
---
Tells curl to limit what protocols it may use on redirect. Protocols denied by
--proto are not overridden by this option. See --proto for how protocols are
represented.

Example, allow only HTTP and HTTPS on redirect:

 curl --proto-redir -all,http,https http://example.com

By default curl will allow all protocols on redirect except several disabled
for security reasons: Since 7.19.4 FILE and SCP are disabled, and since 7.40.0
SMB and SMBS are also disabled. Specifying \fIall\fP or \fI+all\fP enables all
protocols on redirect, including those disabled for security.

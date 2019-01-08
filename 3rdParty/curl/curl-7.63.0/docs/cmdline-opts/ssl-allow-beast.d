Long: ssl-allow-beast
Help: Allow security flaw to improve interop
Added: 7.25.0
---
This option tells curl to not work around a security flaw in the SSL3 and
TLS1.0 protocols known as BEAST.  If this option isn't used, the SSL layer may
use workarounds known to cause interoperability problems with some older SSL
implementations. WARNING: this option loosens the SSL security, and by using
this flag you ask for exactly that.

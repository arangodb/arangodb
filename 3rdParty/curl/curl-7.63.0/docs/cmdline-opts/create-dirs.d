Long: create-dirs
Help: Create necessary local directory hierarchy
---
When used in conjunction with the --output option, curl will create the
necessary local directory hierarchy as needed. This option creates the dirs
mentioned with the --output option, nothing else. If the --output file name
uses no dir or if the dirs it mentions already exist, no dir will be created.

To create remote directories when using FTP or SFTP, try --ftp-create-dirs.

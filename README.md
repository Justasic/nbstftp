NBSTFTP
=======

The No Bullshit TFTP server. I started this project because of my constant frustration with other
TFTP server implementations which require an obscene number of steps for a simple protocol. The
servers expected some kind of special folder that had a magical permission number with a special
user and group which were configured in files that are in non-standard locations.
They called it the "Trivial" file transfer protocol but the only trivial part about it is the
actual protocol, I intend to fix that with this project.


NBSTFTP does not use xinetd or any of that garbage. It is a lightweight TFTP implementation which
reads a config file from /etc/ (unless otherwise specified) and serves files from a configured
directory. Setup should take no more than 5 minutes at max.

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

Building
--------

You need:

 - CMake
 - A C11 compatible C compiler (most modern linux distros have a C11 compiler)
 - flex
 - bison

All compiles require an out-of-tree compile (eg, make a directory somewhere and build), for this example
we will simply make a sub-directory within our source tree called `build`:

`mkdir build; cd build`

To build the project for Debug, simply use:

`cmake ..`

To build the project for Release, use:

`cmake -DNO_CLANG=BOOLEAN:TRUE -DCMAKE_BUILD_TYPE:STRING=RELEASE ..`

The variable define `-DNO_CLANG=BOOLEAN:TRUE` tells cmake not to default to clang but does not eliminate clang from
being found as a compatible compiler, you may omit this if you wish to compile with clang rather than GCC.

Omition of the `-DCMAKE_BUILD_TYPE:STRING=RELEASE` will cause cmake to use debug release instead. See CMake's 
documentation for more details on build types.

The next steps are like any other compile:

`make`

`sudo make install`


Supported Platforms
-------------------

Currently basically everything but Windows and some odd-ball platforms are supported. The following platforms
have been tested and work:

 - Linux
 - FreeBSD
 - Mac OS X
 
The following platforms have not been tested but are expected to work just fine:

 - OpenSolaris 11
 - OpenBSD (with a C11 compatible compiler)
 - Other BSD-based operating systems
 

Contributing
------------

I am in desperate need of someone who can properly document everything and make this project more presentable
to people who have no idea what it is.

Outside of that, anything you see is broken or ugly, feel free to make a pull request or file a bug report!


License
-------

The No Bullshit TFTP server is licensed under the [BSD 2-clause license](https://github.com/Justasic/nbstftp/blob/master/LICENSE).

[`reallocarray`](http://www.openbsd.org/cgi-bin/man.cgi?query=reallocarray) is licensed under the [OpenBSD license](https://github.com/robertbachmann/openbsd-libc/blob/master/stdlib/reallocarray.c).

The `vec` [vector implementation](https://github.com/rxi/vec) in C is licensed under the MIT license.

See the various LICENSE files for details
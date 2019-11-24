# PBD <img width="100" align="right" src="https://i.imgur.com/YOpi5Pu.png"/>
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

PBD is a C debugger for Linux systems, which aims to monitor all global and
local variables (of a specific function) and nicely print the old/new values on
the screen, as well as the place where the change has occurred.

## Introduction
Although there are several debuggers for C, most of them are somewhat complicated
to use, have bad interfaces and end up driving the user away from the language,
especially a beginner.

PBD's main purpose is to debug variables, i.e: monitor all occurrences of changes
and display them on screen, but in an uncomplicated and transparent way for the
user. Like most debuggers, PBD is an _external debugger_ and therefore does not
require any changes to the source code nor does binary instrumentation.

<p align="center">
<a href="https://asciinema.org/a/283214" target="_blank">
<img align="center" src="https://asciinema.org/a/283214.svg" alt="PBD example">
<br>
PBD example
</p>
</a>

## Features
PBD is in earlier stages but is capable to debug all primitive types, _typedefs_ to
primitive types, pointers* and arrays up to 8 dimensions (`[][][][][]`....). For each
supported type, PBD will monitor and print on the screen the old and new values as
soon as the variable gets changed.

PBD also allows the user to track only globals, only locals and/or selectively
watch/ignore a predefined variables list, as can be seen from the help menu:
```
Usage: pbd [options] executable function_name [executable_options]
Options:
  -h --help          Display this information
  -v --version       Display the PBD version
  -s --show-lines    Shows the debugged source code portion in the output
  -l --only-locals   Monitors only local variables (default: global + local)
  -g --only-globals  Monitors only global variables (default: global + local)
  -i --ignore-list <var1, ...> Ignores a specified list of variables names
  -w --watch-list  <var1, ...> Monitors a specified list of variables names

Notes:
  - Options -i and -w are mutually exclusive!

  - The executable *must* be built without any optimization and with at
    least -gdwarf-2 and no PIE! (if PIE enabled by default)

The following options are for PBD internals:
  -d --dump-all    Dump all information gathered by the executable
```

## Speed
Some might say: _Why should I worry about PBD? My GDB already does this with the `watch` command!_

Yes, indeed, and it has helped me a lot over the years, but GDB has one problem:
_software watchpoint_. GDB (and possibly others as well) has 2 main ways to monitor
variables:
1) Hardware Watchpoint
2) Software Watchpoint

The former is GDB's default behavior, and coupled with special processor registers
(DR0-DR3 on Intel x86) it achieves excellent performance with virtually no overhead,
but has one major problem:
regardless of architecture, there will always be a physical limit on how many addresses
the processor will support. For Intel processors, the limit is 4 addresses only, i.e:
GDB can monitor (with HW assistance) up to 4 64-bit variables.

When the amount of data exceeds 32 bytes (64 * 4 bits), GDB is forced to use a software
watchpoint, which is quite simple: the software runs single-step and each monitored
address is checked at each break, which means It's ridiculously slow and costly for the
machine.

PBD, on the other hand, behaves similarly but differently, let me explain: since the idea
is to debug more than just 32 bytes, PBD doesn't use _hardware watchpoint_, but it doesn't
perform single-step all the time either. The main operation is based on _lines of code
executed_, or rather: statements executed. This means that the PBD puts breakpoints at
every start of statements and thus checks for all variables at each break.

This slight difference is enough to translate into a large speedup, depending on the
versions of GCC / Clang, GDB, and PBD.
<p align="center"><img align="center" src="https://i.imgur.com/NZYnvPE.png"></p>

The above chart contains a comparison between GDB v7.12 and PBD v0.5, compiled with GCC 5.3
(on Linux v4.4.38), running code that contains 40,044 bytes of memory to be watched, which runs
within a loop that runs from 12,500 iterations to 100,000 iterations. In addition to seeing a
linear growth in the runtime of the two debuggers, it is interesting to note that for 100k
iterations, PBD was about 22 times faster than GDB (3.33s vs 76.5s).

The tests can be run by issuing: `make bench` (GDB, Rscript and bc are required, in order
to execute and plot the graphs).

## Limitations
At the moment PBD has some limitations, such as features, compilers, operating systems, of which:
### Operating Systems
Exclusive support for Linux (and possibly Unix-like as well) environments. I do not intend to
support Windows, but I can accept contributions  in this regard.

### Compilers
PBD was designed exclusively for GCC and Clang and has not been tested for other compilers, so
I cannot guarantee it will work.

Also, it only supports the DWARF-2 standard (you *must* build your executable with `-gdwarf-2`
instead of `-g`) and does not support PIE executables, so you need to disable (`-no-pie` is
enough) if enabled by default on your compiler.

### Architecture
Supports x86_64 only. I have plans to support x86 in the future as well. As for other architectures
(like ARM), I need to think carefully, but it would be interesting, right?

### C Features
PBD lacks the following C features:
- Structs / Unions [(#3)](https://github.com/Theldus/PBD/issues/3)
- Enums [(#2)](https://github.com/Theldus/PBD/issues/2)
- Pointers* [(#5)](https://github.com/Theldus/PBD/issues/5)

Pointer support is partially supported: PBD will monitor only the value of the pointer, rather
than the content pointed to by it. Please check the issues for a detailed explanation and to
follow the implementation progress.

Any input on one of these points is most welcome, ;-).

## Installing
### Pre-build packages
You can also check for static pre-build binaries in the
[releases](https://github.com/Theldus/PBD/releases) page.

### Build from source
The latest changes are here, if you want to build from source, you are just required to install
`libdwarf` first.
#### Ubuntu/Debian
`sudo apt install libdwarf-dev`
#### Slackware
```
wget https://www.prevanders.net/libdwarf-20190529.tar.gz
wget https://slackbuilds.org/slackbuilds/14.2/development/dwarf.tar.gz
tar xf dwarf.tar.gz
mv libdwarf* dwarf/
sudo sh dwarf/dwarf.SlackBuild
sudo installpkg /tmp/dwarf-*.tgz
```
#### Your distro here
`sudo something...`

After the dependencies are met, just clone the repository and install as usual:
```
git clone https://github.com/Theldus/PBD.git
cd PBD/src/
make              # Build
make test         # Tests, I highly recommend you to _not_ skip this!
sudo make install # Install
```

## Contributing
The PBD is always open to the community and willing to accept contributions, whether with issues,
documentation, testing, new features, bugfixes, typos... welcome aboard.

## License and Authors
PBD is licensed under MIT License. Written by Davidson Francis and (hopefully) others
[contributors](https://github.com/Theldus/PBD/graphs/contributors).

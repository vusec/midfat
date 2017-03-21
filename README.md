# Mid-fat Pointers

Mid-fat pointers provides a framework for efficient compiler-based defenses
for programs written in C or C++. For more information, see the paper
"Fast and Generic Metadata Management with Mid-fat Pointers" by Taddeus Kroes,
Koen Koning, Cristiano Giuffrida, Herbert Bos, and Erik van der Kouwe
presented at the EuroSec 2017 workshop.

## Terminology

To explain our system to automatically build Mid-fat and instrument programs,
we will use the following terms: instance and target.

An instance is a compiler configuration used to instrument a program.
Instances provided by this repository are:

* baseline-lto compiles a program without instrumentation, using LLVM with
  link-time optimizations and using the base version of tcmalloc;
* dummy is the dummy pass described in the paper;
* dummy-sfi is the dummy pass with SFI enabled;
* midfat is the dummy pass with SFI using the Mid-fat Pointers framework..

A target is a program to be instrumented by Mid-fat. We include support for
the SPEC CPU2006 benchmarking suite target (named spec-cpu2006) by default.

## Prerequistes

Mid-fat Pointers runs on Linux and was tested on Ubuntu 16.04.2 LTS 64-bit.
It requires a number of packages to be installed, depending on the particular
Linux distribution used. In case of Ubuntu 16.04.2 LTS, the following command
installs the required packages (on a clean server installation):

    sudo apt-get install bison build-essential gettext git pkg-config python ssh subversion

Our prototype includes scripts to instrument the SPEC CPU2006 benchmarks.
SPEC CPU2006 is not freely available and must be supplied by the user.

Our prototype requires about 11GB of disk space, which includes about 2GB
for the SPEC CPU2006 installation. The latter is optional.

## Installation

First, obtain the Mid-fat source code:

    git clone https://github.com/vusec/midfat.git

The following command automatically installs remaining dependencies locally
(no need for root access), builds Mid-fat, builds all targets for all instances,
and generates scripts to run the targets conveniently:

    cd midfat
    PATHSPEC=/path/to/spec/cpu2006 ./autosetup.sh

To control which targets are built, set and export the TARGETS environment
variable to a space-separated (possibly empty) list of desired targets.
Currently supported option is spec-cpu2006. The default is to build all targets.

When building the SPEC CPU2006 target, PATHSPEC must point to an existing
SPEC CPU2006 installation. We recommend creating a fresh installation for
Mid-fat to use because we need to apply some (very minor) patches.

## Running benchmarks

After building Mid-fat and the desired targets, the targets can be executed
using the run scripts generated in the root directory of the Mid-fat repository.
The run scripts pass along parameters to the run utility supplied by the
benchmarking suite to allow the user to specify the benchmark and any other
settings. There is a separate run script for each instance. For example,
run-spec-cpu2006-midfat.sh runs the SPEC CPU2006 target instrumented
with Mid-fat.

For example, to run the bzip2 benchmark from SPEC CPU2006 instrumented by
Mid-fat, use the following command:

    ./run-spec-cpu2006-midfat.sh 401.bzip2

A lists of available benchmarks can be found in
autosetup/targets/spec-cpu2006/benchmarks.inc.

## Analyzing results

The run scripts write logs to the standard output. To analyze the results after
running a number of benchmarks, redirect each output to a separate file and pass
the names of output files (or, alternatively, the name of the directory
containing the output files) to scripts/analyze-logs.py. 

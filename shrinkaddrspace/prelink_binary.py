#!/usr/bin/env python

import os
import sys
import shlex
import shutil
import subprocess
import re

from elftools.elf.elffile import ELFFile
from elftools.elf.segments import InterpSegment


def ex(cmd):
    """Execute a given command (string), returning stdout if succesfull and
    raising an exception otherwise."""

    p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
    p.wait()
    if p.returncode:
        print "Error while executing command '%s': %d" % (cmd, p.returncode)
        print "stdout: '%s'" % p.stdout.read()
        print "stderr: '%s'" % p.stderr.read()
        raise Exception("Error while executing command")
    return p.stdout.read()


def get_overlap(mapping, other_mappings):
    """Returns the *last* area out of `other_mappings` that overlaps with
    `mapping`, or None. Assumes `other_mappings` is ordered."""

    start, size = mapping
    end = start + size

    for m in other_mappings[::-1]:
        m_start, m_size = m
        m_end = m_start + m_size
        if (start >= m_start and start < m_end) or \
           (end > m_start and end <= m_end) or \
           (start <= m_start and end >= m_end):
            return m

    return None


def get_binary_info(prog):
    """Look for the loader requested by the program, and record existing
    mappings required by program itself."""

    interp = None
    binary_mappings = []

    with open(prog, 'rb') as f:
        e = ELFFile(f)
        for seg in e.iter_segments():
            if isinstance(seg, InterpSegment):
                interp = seg.get_interp_name()
            if seg['p_type'] == 'PT_LOAD':
                binary_mappings.append((seg['p_vaddr'], seg['p_memsz']))
    if interp is None:
        raise Exception("Could not find interp in binary")

    return interp, binary_mappings


def get_library_deps(library):
    """Look for all dependency libraries for a given ELF library/binary, and
    return a list of full paths. Uses the ldd command to find this information
    at load-time, so may not be complete."""

    # TODO: do we have to do this recursively for all deps?

    deps = []

    ldd = ex("ldd \"%s\"" % library)
    for l in ldd.split("\n"):
        m = re.search(r".*.so.* => (/.*\.so[^ ]*)", l)
        if not m:
            continue
        deps.append(m.group(1))
    return deps


def prelink_libs(libs, outdir, existing_mappings, baseaddr=0xf0ffffff):
    """For every library we calculate its size and alignment, find a space in
    our new compact addr space and create a copy of the library that is
    prelinked to the addr. Start mapping these from the *end* of the addr space
    down, but leaving a bit of space at the top for stuff like the stack."""

    for lib in libs:
        with open(lib, 'rb') as f:
            # Determine the alignment and size required for all LOAD segments
            # combined
            align, size = 0, 0
            e = ELFFile(f)
            for seg in e.iter_segments():
                if seg['p_type'] != 'PT_LOAD':
                    continue
                if seg['p_align'] > align:
                    align = seg['p_align']
                size = seg['p_vaddr'] + seg['p_memsz']

            baseaddr -= size

            # Search for a slot that is not overlapping with anything else and
            # aligned properly
            found = False
            while not found:
                if baseaddr % align:
                    baseaddr -= baseaddr % align
                overlap = get_overlap((baseaddr, size), existing_mappings)
                if overlap:
                    baseaddr = overlap[0] - size
                else:
                    found = True

            print "Found %08x - %08x for %s" % (baseaddr, baseaddr + size, lib)

            newlib = os.path.join(outdir, os.path.basename(lib))
            shutil.copy(lib, newlib)
            ex("prelink -r 0x%x \"%s\"" % (baseaddr, newlib))



def main():
    import argparse

    parser = argparse.ArgumentParser(description="Shrink address space of "
            "given binary by prelinking all dependency libraries.")
    parser.add_argument("binary", help="The ELF binary")
    parser.add_argument("--in-place", help="Modify binary in-place",
            action="store_true", default=False)
    parser.add_argument("--set-rpath",
            help="Set RPATH of (new) binary and preload lib to out-dir",
            action="store_true", default=False)
    parser.add_argument("--preload-lib", default="libpreload.so",
            help="Library used via LD_PRELOAD that moves stack and mmap_base")
    parser.add_argument("--out-dir", default="",
            help="Output directory for prelinked libs")
    args = parser.parse_args()

    outdir = args.out_dir
    if not args.out_dir:
        outdir = os.path.abspath("prelink-%s" % args.binary.replace("/", "_"))

    print "Using output dir %s" % outdir
    if os.path.isdir(outdir):
        shutil.rmtree(outdir)
    os.mkdir(outdir)

    # Get loader and existing mappings for binary
    interp, binary_mappings = get_binary_info(args.binary)

    # Determine all dependency libraries
    libs = set()
    libs.update(get_library_deps(args.binary))
    libs.update(get_library_deps(args.preload_lib))
    libs.add(interp)
    libs.add(args.preload_lib)

    # The magic, construct new addr space by prelinking all dependency libs
    prelink_libs(libs, outdir, binary_mappings)

    # Update the loader to use our prelinked version
    if args.in_place:
        newprog = args.binary
    else:
        newprog = os.path.join(outdir, os.path.basename(args.binary))
        shutil.copy(args.binary, newprog)
    newinterp = os.path.realpath(os.path.join(outdir, os.path.basename(interp)))
    ex("patchelf --set-interpreter \"%s\" \"%s\"" % (newinterp, newprog))

    # By setting the rpath, we can avoid having to specify LD_LIBRARY_PATH
    if args.set_rpath:
        absoutdir = os.path.realpath(outdir)
        ex("patchelf --set-rpath \"%s\" \"%s\"" % (absoutdir, newprog))
        newpreload = os.path.join(outdir, os.path.basename(args.preload_lib))
        ex("patchelf --set-rpath \"%s\" \"%s\"" % (absoutdir, newpreload))

if __name__ == '__main__':
    main()

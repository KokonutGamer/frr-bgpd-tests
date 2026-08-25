#!/usr/bin/bash
#
#===============================================================================
#  Sets up additional tools, such as a newer version of Valgrind and gcovr.
#===============================================================================
#

#===============================================================================
#  Clang tools
#===============================================================================

sudo apt-get update && sudo apt-get install -y clang llvm lld lldb ninja-build

#===============================================================================
#  Valgrind
#===============================================================================

VALGRIND_VERSION="3.27.1"
VALGRIND_TAR="valgrind-$VALGRIND_VERSION.tar.bz2"
VALGRIND_URL="https://sourceware.org/pub/valgrind/$VALGRIND_TAR"
VALGRIND_DIR="/home/frr/valgrind"

mkdir -p "$VALGRIND_DIR"
cd "$VALGRIND_DIR"
wget "$VALGRIND_URL"

tar -xvf "$VALGRIND_TAR"

cd "valgrind-$VALGRIND_VERSION"
./configure
make

make install # must be done as sudo

#===============================================================================
#  gcovr
#===============================================================================

GCOVR_EXEC="gcovr"
GCOVR_BIN_DIR="/home/frr/.local/bin"

sudo -H -u frr pip install --user "$GCOVR_EXEC"

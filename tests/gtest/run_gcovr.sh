#!/bin/bash
#
#===============================================================================
#  Run from project root (gcovr-test directory).
#===============================================================================

help()
{
    # Display help
    echo "Collect coverage information on how the Google Test suite uses FRR."
    echo
    echo "Syntax: run_gcovr.sh [-b|l|g|h]"
    echo "options:"
    echo "b     Collect coverage info on FRR's bgpd directory. Default behavior."
    echo "l     Collect coverage info on FRR's lib directory."
    echo "g     Collect coverage info on the Google Test suite."
    echo "h     Display help info about this script."
    echo
}

exec_gcovr()
{
    echo "Collecting coverage info for $2."
    echo

    mkdir -p "$3"

    if [ -z "${5:-}" ]; then
        gcovr "$2" -f "$2" -r "$1" --html-details "$3/$4"
    else
        gcovr "$2" -f "$2" -e "$2/$5" -r "$1" --html-details "$3/$4"
    fi
}

# Constants
COVERAGE_DIR="coverage"
LIB_DIR="/home/frr/frr"
PROJECT_ROOT="/gtest"

if [ $# -eq 0 ]; then
    # default to bgpd coverage
    exec_gcovr "$PROJECT_ROOT" "$LIB_DIR/bgpd" "$COVERAGE_DIR/bgpd" "bgpd-cov.html"
fi

while getopts ":hblg" option; do
    case $option in
        h) # display help
            help
            exit
            ;;
        b) # bgpd directory
            exec_gcovr "$PROJECT_ROOT" "$LIB_DIR/bgpd" "$COVERAGE_DIR/bgpd" "bgpd-cov.html"
            ;;
        l) # lib directory
            exec_gcovr "$PROJECT_ROOT" "$LIB_DIR/lib" "$COVERAGE_DIR/lib" "lib-cov.html"
            ;;
        g) # 
            exec_gcovr "$PROJECT_ROOT" "$PROJECT_ROOT" "$COVERAGE_DIR/gtest" "gtest-cov.html" "bin"
            ;;
       \?) # invalid option
            echo "Error: unknown option"
            echo
            help
            exit
            ;;
    esac
done

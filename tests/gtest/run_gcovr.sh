#!/bin/bash
#
#===============================================================================
#  Run from project root (gcovr-test directory).
#===============================================================================

PROJECT_ROOT="/gtest"
OUTPUT_DIR="coverage"
OUTPUT_FILE="frr-bgp-ls-gtest-cov.html"
LIB_DIR="/home/frr/frr"
GTEST_DIR="src"

LIB_BGP="$LIB_DIR/bgpd"

FILTER_LIST=("$LIB_BGP/bgp_ls.c" "$LIB_BGP/bgp_ls_ted.c" "$LIB_BGP/")

mkdir -p "$OUTPUT_DIR"
gcovr "$LIB_BGP" --filter "$LIB_BGP" -r "$PROJECT_ROOT" --html-details "$OUTPUT_DIR/$OUTPUT_FILE"

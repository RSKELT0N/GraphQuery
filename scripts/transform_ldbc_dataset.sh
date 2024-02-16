#!/bin/bash

#************************************************************
 # \author Ryan Skelton
 # \date 18/09/2023
 # \file transform_ldbc_datasets.hpp
 # \brief Script to transform a dataset from LDBC SNB to be readable
 #        by GraphQuery CDatasetLDBC instance.
 #
 # /url LDBC SNB Datasets - https://ldbcouncil.org/data-sets-surf-repository/snb-business-intelligence.html
#************************************************************/

if [ "$#" -ne 2 ];
then
    echo "Incorrect number of params, pass the path to recursively unzip and suffix type."
    exit 1
fi

echo "Starting LDBC dataset transformation.."
# shellcheck disable=SC2038
find "$1" -name "*.$2" | xargs gunzip
echo "All files with suffix '*.$2' have been unzipped."
# shellcheck disable=SC2038
find "$1" -type f -not -name "*.csv" | xargs rm -f
echo "All other files except '*.csv' have been removed."
# shellcheck disable=SC2156
find "$1" -type f -name "*.csv" -exec sh -c 'dir=$(dirname "{}") && new_name="$(dirname "$dir")/$(basename "$dir").csv" && if [ -f "{}" ]; then mv "{}" "$new_name" && rm -r "$dir"; fi' \;
echo "All '*.csv' files have been extracted and renamed."

initial_snapshot_dir=$(find $1 -type d -name "initial_snapshot" -print -quit)

if [[ -n $initial_snapshot_dir ]]; then
    find "$initial_snapshot_dir" -type f -name '*_*' -exec sh -c 'mkdir -p "${1%/*}/edges" && mv "$1" "${1%/*}/edges/"' sh {} \;
    find "$initial_snapshot_dir" -type f ! -name '*_*' -exec sh -c 'mkdir -p "${1%/*}/vertices" && mv "$1" "${1%/*}/vertices/"' sh {} \;
fi
echo "Vertex and edge files have been stored under their own folder, for dynamic and static parts of the graph."
echo "Script finished successfully."
exit 0



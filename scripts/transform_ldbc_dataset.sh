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

if [ "$#" -ne 2 ]; then
    echo "Incorrect number of params, pass the path to recursively unzip and suffix type."
    exit 1
fi

echo "Starting LDBC dataset transformation.."

# Unzip files
find "$1" -name "*.$2" | xargs gunzip
echo "All files with suffix '*.$2' have been unzipped."

# Remove non-CSV files
find "$1" -type f -not -name "*.csv" | xargs rm -f
echo "All other files except '*.csv' have been removed."

# Concatenate CSV files in each directory into the first CSV file found in that directory
find "$1" -type d -exec sh -c '
    for dirpath do
        csv_files=($(find "$dirpath" -maxdepth 1 -type f -name "*.csv"))
        if [ ${#csv_files[@]} -gt 1 ]; then
            first_csv="${csv_files[0]}"
            tail -q -n +2 ${csv_files[@]:1} >> $first_csv
            for file_to_remove in ${csv_files[@]:1}; do
                if [ -f "$file_to_remove" ]; then
                    rm "$file_to_remove"
                fi
            done
        fi
    done' sh {} +

# Rename remaining CSV files
find "$1" -type f -name "*.csv" -exec sh -c '
    dir=$(dirname "{}")
    new_name="$(dirname "$dir")/$(basename "$dir").csv"
    if [ -f "{}" ]; then
        mv "{}" "$new_name"
        rm -r "$dir"
    fi' \;
echo "All '*.csv' files have been extracted, concatenated, and renamed."

initial_snapshot_dir=$(find "$1" -type d -name "initial_snapshot" -print -quit)

if [[ -n $initial_snapshot_dir ]]; then
    find "$initial_snapshot_dir" -type f -name '*_*' -exec sh -c '
        mkdir -p "${1%/*}/edges" && mv "$1" "${1%/*}/edges/"' sh {} \;
    find "$initial_snapshot_dir" -type f ! -name '*_*' -exec sh -c '
        mkdir -p "${1%/*}/vertices" && mv "$1" "${1%/*}/vertices/"' sh {} \;
    echo "Vertex and edge files have been stored under their own folder, for dynamic and static parts of the graph."
    python remap_ids.py "$initial_snapshot_dir"
    echo "Non-sequential IDs have been remapped to a lower space."
fi

echo "Script finished successfully."
exit 0


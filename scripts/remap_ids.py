#************************************************************
# \author Ryan Skelton
# \date 18/09/2023
# \file remap_ids.py
# \brief Script to remap non-sequential IDs into a lower
#        space.
#
# /url LDBC SNB Datasets - https://ldbcouncil.org/data-sets-surf-repository/snb-business-intelligence.html
#************************************************************/

import os
import sys
import pandas as pd

new_id = 0

def remap_ids(df):
    global new_id
    id_mapping = {}

    for old_id in df['id']:
        # Assign a new ID to each unique old ID
        if old_id not in id_mapping:
            id_mapping[old_id] = new_id
            new_id += 1

    # Apply the mapping to the 'id' column
    df['id'] = df['id'].map(id_mapping).astype(int)

    return df, id_mapping

def process_directory(directory, subdirectory, mappings):
    directory = os.path.join(directory, subdirectory)
    # Process the vertices files
    vertices_dir = os.path.join(directory, 'vertices')
    for filename in os.listdir(vertices_dir):
        if filename.endswith(".csv"):
            df = pd.read_csv(os.path.join(vertices_dir, filename), delimiter='|')
            print("~ Remapping " + filename)
            
            # Remap the IDs if they are not sequential
            df, id_mapping = remap_ids(df)

            type = filename.split('.')[0]
            mappings[type] = id_mapping

            if type == "Place":
                mappings["City"] = id_mapping
                mappings["Country"] = id_mapping
                mappings["Continent"] = id_mapping
            elif type == "Organisation":
                mappings["Company"] = id_mapping
                mappings["University"] = id_mapping

            # Overwrite the original CSV file
            df.to_csv(os.path.join(vertices_dir, filename), index=False, sep='|')

    # Then, process the edges files
    edges_dir = os.path.join(directory, 'edges')
    for filename in os.listdir(edges_dir):
        if filename.endswith(".csv"):
            df = pd.read_csv(os.path.join(edges_dir, filename), delimiter='|')
            print("~ Remapping " + filename)

            # Check if this file corresponds to a vertices file
            parts = filename.split('_')
            src_type = parts[0]
            dst_type = parts[2].split('.')[0]

            src_col_idx = 0 if subdirectory == "static" else 1
            dst_col_idx = 1 if subdirectory == "static" else 2
            df[df.columns[src_col_idx]] = df[df.columns[src_col_idx]].map(mappings[src_type]).fillna(df[df.columns[src_col_idx]]).astype(int)
            df[df.columns[dst_col_idx]] = df[df.columns[dst_col_idx]].map(mappings[dst_type]).fillna(df[df.columns[dst_col_idx]]).astype(int)
            
            # Overwrite the original CSV file
            df.to_csv(os.path.join(edges_dir, filename), index=False, sep='|')

def process_files(directory):
    mappings = {}

    # Process the 'dynamic' subdirectory
    process_directory(directory, 'static', mappings)

    # Process the 'static' subdirectory
    process_directory(directory, 'dynamic', mappings)

# Get the directory from the command line arguments
if len(sys.argv) != 2:
    print("Usage: python script.py <directory>")
else:
    directory = sys.argv[1]
    process_files(directory)


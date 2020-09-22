#!/bin/bash

######################################################################################
# script to generate SRSPMD trips based on bus trip data

# 0. compile C++ code used in this script
# code in this repository
g++ -o nd nodes_distances.cpp -O3 -march=native -std=gnu++11
g++ -o st3 sample_trips3.cpp -O3 -march=native -std=gnu++11
g++ -o dm dist_matrix.cpp -O3 -march=native -std=gnu++11

# code needed to extract trips
git clone https://github.com/dkondor/join-utils.git
cd join-utils
g++ -o hashjoin hashjoin.cpp -O3 -march=native -std=gnu++11
cd ..

# 1. extract trips 
zcat origin_destination_bus_201901.zip | tail -n +2 | awk -F, '{if($2 == "WEEKDAY") print $3,$5,$6,$7;}' > bustrips_all_weekday.dat
# 1.1. filter trips to only include ones from Toa Payoh
# this uses the hashjoin program from here: https://github.com/dkondor/join-utils
# alternatively, you could sort the files and use the standard join program
tail -n +2 busstops_toa_payoh.csv | cut -d , -f 1 | join-utils/hashjoin -1 1 -2 2 -o1 "" - bustrips_all_weekday.dat > bustrips_all_weekday_tmp1.dat
tail -n +2 busstops_toa_payoh.csv | cut -d , -f 1 | join-utils/hashjoin -1 1 -2 3 -o1 "" - bustrips_all_weekday_tmp1.dat > bustrips_toa_payoh_weekday.dat


# 2. calculate distances among OSM nodes
./nd -N -n toa_payoh_paths_edges.dat | cut -f 1,2,3 > toa_payoh_paths_nodes_distances.dat

# 2.1. (optional) create a binary distance matrix for faster processing
./dm -i toa_payoh_paths_nodes_distances.dat -o toa_payoh_paths_nodes_distances.bin > toa_payoh_paths_nodes_distances_ids.dat



# 3. create random sample of trips

# parameters used (the following could be run in a loop for multiple parameter combinations)
nt=1000 # number of trips to generate
R=2000 # maximum trip distance (in meters)
s=1 # random seed to use

# 3.1. using the list of distances
./st3 -N $nt -D $R -d toa_payoh_paths_nodes_distances.dat -i bustrips_toa_payoh_weekday.dat -s $s -b toa_payoh_buildings_osm_center_busstops.csv -B toa_payoh_buildings_osm_center_filtered.csv -n toa_payoh_buildings_osm_center_nodes.csv -p busstops_matches.dat -c trips_coords_R"$R"_N"$nt"_s$s.csv > trips_R"$R"_N"$nt"_s$s.dat

# 3.2. using the distances in binary format
./st3 -N $nt -D $R -d toa_payoh_paths_nodes_distances.bin -I toa_payoh_paths_nodes_distances_ids.dat -i bustrips_toa_payoh_weekday.dat -s $s -b toa_payoh_buildings_osm_center_busstops.csv -B toa_payoh_buildings_osm_center_filtered.csv -n toa_payoh_buildings_osm_center_nodes.csv -p busstops_matches.dat -c trips_coords_R"$R"_N"$nt"_s$s.csv > trips_R"$R"_N"$nt"_s$s.dat





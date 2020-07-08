# Road and path network data from OSM

This folder contains data about roads and paths in Singapore, obtained from OpenStreetMap (OSM). This includes the raw data and the extracted path network with https://github.com/dkondor/osmconvert/.

 - Singapore.osm.pbf: orginal OSM extract for Singapore
 - sg_osm_nodes.dat: nodes relevant for path; columns are OSM ID and coordinates
 - sg_osm_edges.dat: network edges for routing (undirected); columns include OSM IDs of nodes and distance along the path in meters

Note that intermediate nodes (nodes with degree == 2) were removed; distance is calculated along the original path and can be larger than the straight line distance between nodes.


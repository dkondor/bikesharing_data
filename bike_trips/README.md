# Bike trips

Trip data used in paper https://arxiv.org/abs/1909.03679.

This includes the following:
 
 - bike_trips.csv: Original trips
 - bike_trips2_nodes_distances_2017091?.dat: trips matched to closest OSM nodes and shortest path distances calculated between these.

The processed TSV files include the following columns:
 
 - trip_id: unique integer for each trip
 - bike_id: identifier for the bike used in the trip
 - start_ts: timestamp for the start of the trip (note: without time zone)
 - end_ts: timestamp for the end of the trip
 - start_node: ID of closest OSM node to the trip start coordinates
 - start_dist: distance between start_node and the actual trip start coordinates
 - end_node: ID of closest OSM node to the trip end coordinates
 - end_dist: distance between end_node and the actual trip end coordinates
 - trip_dist: shortest path distance between start_node and end_node

All OSM nodes are from the set included in the osm folder in this repository. All distances are in meters.



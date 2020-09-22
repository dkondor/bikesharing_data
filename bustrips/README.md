# Bustrip data

This folder contains data about bus trips and bus stop locations obtained from LTA's [DataMall](https://www.mytransport.sg/content/mytransport/home/dataMall.html) interface. See additional documentation [here](https://www.mytransport.sg/content/dam/datamall/datasets/LTA_DataMall_API_User_Guide.pdf) and license terms [here](https://www.mytransport.sg/content/mytransport/home/dataMall/SingaporeOpenDataLicence.html). This includes the following:

 - origin_destination_bus_201901.zip: Total count of bus trips between all bus stop pairs in Singapore in January 2019. Downloaded in April 2019.
 - busstops.csv: List of all bus stops in Singapore as of September 2019.
 - busstops_toa_payoh.csv: Subset of the previous, including the bus stops identified to be in Toa Payoh, specifically in the region used in the paper https://arxiv.org/abs/1909.03679.
 - busstops_all_nodes.dat: Mapping of bus stops to closest OSM network nodes. The columns are bus stop ID, OSM node ID, distance in meters. OSM node IDs refer to the network in this repository, under osm/sg_osm_nodes.dat
 - busstops_matches.dat: Matching bus stop pairs. This file includes pairs of stops that were identified by manual inspection to be paired, being on the opposite side of the road, used by services in the opposite directions. These were merged together in a preprocessing step.

It also contains the building data and the path network edges extracted from OSM in the Toa Payoh area of Singapore:

 - toa_payoh_buildings_osm_center_filtered.csv: center coordinates of buildings
 - toa_payoh_buildings_osm_center_nodes.csv: matching of buildings to OSM nodes in the path network (InputID refers to the buildings, TargetID refers to the path nodes)
 - toa_payoh_buildings_osm_center_busstops.csv: matching of buildings to bus stops  (InputID refers to the buildings, TargetID refers to the bus stop)
 - toa_payoh_paths_edges.dat: path network edges withing Toa Payoh (filtered from the data under the OSM folder)

All distances are in meters.

OSM data redistributed under the Open Database License: see [LICENSE.md](https://github.com/dkondor/bikesharing_data/blob/master/osm/LICENSE.md) or http://opendatacommons.org/licenses/odbl/1.0/.


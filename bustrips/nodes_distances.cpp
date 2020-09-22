/*
 * busstop_distances.cpp -- calculate (shortest) distances between a set
 * 	of bus stops
 * includes matching bus stops to nodes on the OSM network
 * 
 * Copyright 2018 Daniel Kondor <kondor.dani@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 */


#include <stdio.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <utility>

#include "read_table.h"


struct node {
	double d; /* current estimate of distance to this node */
	double real_d; /* "real" distance; the above can be the weighted distance */
	uint64_t node_id; /* node id */
	uint64_t ancestor; /* last node in the path leading to this */
	bool operator < (const node& n) const {
		/* note: node_id is part of the comparison so that nodes can be found exactly
		 * (even if distances are the same for multiple nodes);
		 * ancestor and real distance are not part of the comparison as that is not known or relevant when searching */
		return d < n.d || (d == n.d && node_id < n.node_id);
	}
};

struct edge_info {
	double d; /* edge distance */
	unsigned int cnt; /* total times this edge was used */
	unsigned int first_ts; /* first time this edge was used */
	bool is_improved; /* flag if the edge is part of the improved network */
	explicit edge_info(double d_) : d(d_), cnt(0), first_ts(0), is_improved(false) { }
	edge_info() : d(0), cnt(0), first_ts(0), is_improved(false) { }
};

int main(int argc, char **argv)
{
	char* network_fn = 0; /* input: network file (with distances for each edge; symmetrized when reading) */
	char* points_fn = 0; /* input: points to process -- distances are calculated among all pairs */
	char* improved_edges = 0; /* optionally: list of edges which have been improved (allow faster travel) */
	double improved_edge_weight = 1.5; /* extra preference toward improved edges */
	bool network_distance = false; /* if true, do not read points, just calculate the distances between the nodes in the network */
	for(int i=1;i<argc;i++) {
		if(argv[i][0] == '-') switch(argv[i][1]) {
			case 'n':
				network_fn = argv[i+1];
				i++;
				break;
			case 'p':
				points_fn = argv[i+1];
				i++;
				break;
			case 'I':
				improved_edge_weight = atof(argv[i+1]);
				i++;
				break;
			case 'i':
				improved_edges = argv[i+1];
				i++;
				break;
			case 'N':
				network_distance = true;
				break;
			default:
				fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
				break;
		}
		else fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
	}
	
	if(!network_distance) {
		if(points_fn == 0 && network_fn == 0) {
			fprintf(stderr,"At least one input file name needs to be specified!\n");
			return 1;
		}
	}
	
	/* read the network */
	 // graph is simply an associative container of edges with distances and counts (of trips using the edge)
	std::unordered_map<uint64_t,std::unordered_map<uint64_t,edge_info > > n;
	{
		read_table2 rt(network_fn,stdin);
		while(rt.read_line()) {
			uint64_t n1,n2;
			double d;
			if(!rt.read(n1,n2,d)) break;
			n[n1][n2] = edge_info(d);
			n[n2][n1] = edge_info(d);
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading network:\n");
			rt.write_error(stderr);
			return 1;
		}
	}
	
	/* read improved edges (if any) */
	if(improved_edges) {
		if(improved_edge_weight <= 0) {
			fprintf(stderr,"Improved edge weight must be positive!\n");
			return 1;
		}
		if(improved_edge_weight <= 1) fprintf(stderr,"Improved edge weight seems too low (%g <= 1)\n",improved_edge_weight);
		unsigned int cnt = 0;
		read_table2 rt(improved_edges);
		while(rt.read_line()) {
			uint64_t n1,n2;
			if(!rt.read(n1,n2)) break;
			if(n.count(n1) == 0 || n.count(n2) == 0) {
				fprintf(stderr,"Improved edge %lu -- %lu not in network!\n",n1,n2);
				return 1;
			}
			if(n[n1].count(n2) == 0 || n[n2].count(n1) == 0) {
				fprintf(stderr,"Improved edge %lu -- %lu not in network!\n",n1,n2);
				return 1;
			}
			n[n1][n2].is_improved = true;
			n[n2][n1].is_improved = true;
			cnt++;
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading improved edges:\n");
			rt.write_error(stderr);
			return 1;
		}
		fprintf(stderr,"%u improved edges read\n",cnt);
	}
	
	/* read the trips */
	size_t npoints = 0;
	std::unordered_map<uint64_t, std::vector<std::pair<uint64_t,double> > > nodes_points;
	if(network_distance) for(const auto& x : n) {
		nodes_points.insert(std::make_pair(x.first,std::vector<std::pair<uint64_t,double> >({std::make_pair(x.first,0.0)})));
		npoints++;
	}
	else {
		read_table2 rt(points_fn,stdin);
		while(rt.read_line()) {
			uint64_t ptid;
			uint64_t nid;
			double d;
			if(!rt.read(ptid,nid,d)) break;
			if(!n.count(nid)) {
				fprintf(stderr,"Node node found:\n%s\n",rt.get_line_str());
				return 1;
			}
			nodes_points[nid].push_back(std::make_pair(ptid,d));
			npoints++;
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading points:\n");
			rt.write_error(stderr);
			return 1;
		}
	}
	if(nodes_points.size() == 0) {
		fprintf(stderr,"No trips read!\n");
		return 1;
	}
	
	FILE* fout = stdout;
	unsigned int searches = 0;
	
	for(const auto& x : nodes_points) {
		/* perform a search from each node that has assigned point */
		uint64_t start_node = x.first;
		
		std::set<node> q; /* queue of nodes to process by distance */
		std::unordered_map<uint64_t,double> node_distances; /* distance of all nodes from the start node */
		node_distances[start_node] = 0;
		q.insert(node {0.0,0.0,start_node,start_node});
		size_t found = 0;
		
		do {
			auto it = q.begin();
			uint64_t current = it->node_id;
			double d = it->d;
			double real_d = it->real_d;
			q.erase(it);
			
			auto it2 = nodes_points.find(current);
			if(it2 != nodes_points.end()) {
				found += it2->second.size();
				for(const auto& n1 : x.second) for(const auto& n2 : it2->second) if(n1.first < n2.first)
					fprintf(fout,"%lu\t%lu\t%f\t%f\t%f\t%f\n",n1.first,n2.first,d,real_d,n1.second,n2.second);
			}
			/* exit if found all points */
			if(found == npoints) break;
			/* add to the queue the nodes reachable from the current */
			for(const auto& x : n[current]) {
				uint64_t n1 = x.first; /* node ID */
				double real_d1 = real_d + x.second.d; /* total real distance this way */
				double d1 = d; /* total weighted distance this way */
				if(x.second.is_improved) d1 += x.second.d / improved_edge_weight;
				else d1 += x.second.d;
				auto it3 = node_distances.find(n1);
				if(it3 == node_distances.end()) { /* this node was not seen yet, we can add to the queue */
					node_distances.insert(std::make_pair(n1,d1));
					q.insert(node{d1,real_d1,n1,current});
				}
				else {
					/* node already found, may need to be updated -- only if new distance is shorter */
					if(d1 < it3->second) {
						if(q.erase(node{it3->second,0,n1,0}) != 1) {
							fprintf(stderr,"Error: node %lu not found in the queue (distance: %f)!\n",n1,it3->second);
							return 1;
						}
						it3->second = d1;
						q.insert(node{d1,real_d1,n1,current});
					}
				}
			}
		} while(q.size());
		
		if(found != npoints) {
			fprintf(stderr,"Not all points found!\n");
			return 1;
		}
		searches++;
		fprintf(stderr,"\r%u start nodes processed",searches);
		fflush(stderr);
	}
	putc('\n',stderr);
	
	return 0;
}


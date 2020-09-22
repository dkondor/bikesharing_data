/*
 * sample_trips3.cpp -- create a sample of trips based on aggregated data
 * 
 * assign trip start / end to building locations instead of bus stops
 * 
 * new format: separate files for matching buildings to bus stops and nodes
 * 	+ extra matching among pairs of bus stops (serving the same area in
 * 	opposite direction); only pairs of stops supported, not larger clusters
 * 
 * Copyright 2019 Daniel Kondor <kondor.dani@gmail.com>
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
 */


#include <stdio.h>
#include <stdint.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <unordered_map>
#include <vector>
#include <random>
#include "read_table.h"


/*-----------------------------------------------------------------------------
 * pair_hash: combine the hash of two 64-bit unsigned integers
 * 
 * adapted from Murmurhash, from
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
 * MurmurHash2, 64-bit versions, by Austin Appleby
 * 64-bit hash for 64-bit platforms
 * 
 * MurmurHash2 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
*/
struct pair_hash {
	uint64_t seed;
	explicit pair_hash(uint64_t s = 0xe6573480bcc4fceaUL) : seed(s) { }
	uint64_t operator () (const std::pair<uint64_t,uint64_t>& p) const {
		
		const uint64_t m = 0xc6a4a7935bd1e995UL;
		const int r = 47;
		uint64_t h = seed ^ (16UL * m); /* 16UL since we have 16 bytes in total */
		
		const std::array<uint64_t,2> a = {p.first, p.second};
		for(uint64_t k : a) {
			k *= m; 
			k ^= k >> r; 
			k *= m; 
			
			h ^= k;
			h *= m; 
		}
		
		h ^= h >> r;
		h *= m;
		h ^= h >> r;
		
		return h;
	}
};

struct building_node {
	uint64_t pc; /* postal code */
	uint64_t nid; /* node id */
	double dist; /* distance of building to node */
};


/* generic interface for distances -- store them in a matrix */
class distances {
	protected:
		void* map;
		double* matrix;
		size_t n;
		size_t map_size;
		std::unordered_map<uint64_t,size_t> ids;
		int f;
		const static uint64_t file_id = 0x47a9b290e72d9f21UL;
		
	public:
		distances():map(MAP_FAILED),matrix(0),n(0UL),map_size(0UL),f(-1) { }
		~distances() { clear(); }
		
		void clear() {
			if(map != MAP_FAILED) munmap(map,map_size);
			else if(matrix) free(matrix);
			map = MAP_FAILED;
			matrix = 0;
			n = 0;
			map_size = 0;
			ids.clear();
			if(f != -1) close(f);
			f = -1;
		}
		
		bool open_dists(const char* df, const char* fids) {
			clear();
			/* load ids first */
			{
				std::vector<uint64_t> vids;
				read_table2 rt(fids);
				while(rt.read_line()) {
					uint64_t id;
					if(!rt.read(id)) break;
					vids.push_back(id);
				}
				if(rt.get_last_error() != T_EOF) {
					fprintf(stderr,"distances::open_dists(): Error reading ids:\n");
					rt.write_error(stderr);
					return false;
				}
				
				for(size_t i=0;i<vids.size();i++) ids.insert(std::make_pair(vids[i],i));
			}
			n = ids.size();
			/* try opening distances file */
			f = open(df,O_RDONLY | O_CLOEXEC | O_NOATIME);
			if(f == -1) {
				fprintf(stderr,"distances::open_dists(): Error opening file %s!\n",df);
				ids.clear();
				return false;
			}
			{
				struct stat st;
				if(fstat(f,&st)) {
					fprintf(stderr,"distances::open_dists(): Error with stat() on file %s!\n",df);
					ids.clear();
					close(f);
					f = -1;
					return false;
				}
				map_size = st.st_size;
			}
			if(map_size != 16 + sizeof(double)*n*n) {
				fprintf(stderr,"distances::open_dists(): unexpected file size!\n");
				ids.clear();
				close(f);
				f = -1;
				return false;
			}
			map = mmap(0,map_size,PROT_READ,MAP_SHARED,f,0);
			if(map == MAP_FAILED) {
				fprintf(stderr,"distances::open_dists(): error with mmap()!\n");
				ids.clear();
				close(f);
				f = -1;
				return false;
			}
			
			uint64_t* tmp = (uint64_t*)map;
			if(tmp[0] != file_id) {
				fprintf(stderr,"distances::open_dists(): unexpected file ID!\n");
				clear();
				return false;
			}
			if(tmp[1] != n) {
				fprintf(stderr,"distances::open_dists(): unexpected size in file (%lu instead of %lu)!\n",tmp[1],n);
				clear();
				return false;
			}
			
			matrix = (double*)(map+16);
			return true;
		}
		
		double get_dist(uint64_t n1, uint64_t n2) {
			n1 = ids.at(n1);
			n2 = ids.at(n2);
			return matrix[n1*n+n2];
		}
		
		bool read_dists(read_table2& rt) {
			clear();
			std::unordered_map<std::pair<uint64_t,uint64_t>,double,pair_hash> dists;
			while(rt.read_line()) {
				uint64_t n1,n2;
				double d;
				if(!rt.read(n1,n2,d)) break;
				dists.insert(std::make_pair(std::make_pair(n1,n2),d));
				dists.insert(std::make_pair(std::make_pair(n2,n1),d));
			}
			if(rt.get_last_error() != T_EOF) {
				fprintf(stderr,"distances::read_dists(): Error reading distances:\n");
				rt.write_error(stderr);
				return false;
			}
			std::vector<uint64_t> nids2;
	
			for(const auto& x : dists) {
				if(ids.count(x.first.first) == 0) {
					ids[x.first.first] = nids2.size();
					nids2.push_back(x.first.first);
				}
				if(ids.count(x.first.second) == 0) {
					ids[x.first.second] = nids2.size();
					nids2.push_back(x.first.second);
				}
			}
			
			n = nids2.size();
			matrix = (double*)malloc(sizeof(double)*n*n);
			if(!matrix) {
				fprintf(stderr,"distances::read_dists(): Error allocating memory!\n");
				ids.clear();
				return false;
			}
			
			for(uint64_t i=0;i<n;i++) for(uint64_t j=0;j<n;j++) {
				double dist = 0.0;
				if(i != j) dist = dists.at(std::make_pair(nids2[i],nids2[j]));
				matrix[i*n+j] = dist;
			}
			return true;
		}
		bool read_dists(read_table2&& rt) {
			return read_dists(rt);
		}
};

struct busstops_pairs_t {
	std::unordered_map<uint64_t,uint64_t> busstops_pairs;
	void set(uint64_t n1, uint64_t n2) { busstops_pairs[n1] = n2; }
	uint64_t get(uint64_t n1) const {
		auto it = busstops_pairs.find(n1);
		if(it != busstops_pairs.cend()) n1 = it->second;
		return n1;
	}
};

int main(int argc, char **argv)
{
	char* infn = 0; /* input: aggregated trips */
	char* dist_fn = 0; /* distances between nodes (not bus stops) */
	char* buildings_fn = 0; /* matching of buildings to bus stops */
	char* buildings_nodes_fn = 0; /* matching of buildings to nodes */
	unsigned int N = 1000; /* generate this many trips */
	double max_dist = 0.0; /* maximum distance to consider */
	double v = 5000.0 / 3600.0; /* speed of vehicles (with user), in m/s */
	char* buildings_coords_fn = 0; /* if set, load building coordinates from this file */
	char* trip_coords_out = 0; /* save trips with coordinates here */
	char* dists_ids = 0; /* if given, distances are stored in a binary file already */
	char* busstops_pairs_fn = 0; /* pairs of bus stops to be considered as same */
	
	uint64_t seed = time(0);
	
	for(int i=1;i<argc;i++) {
		if(argv[i][0] == '-') switch(argv[i][1]) {
			case 'i':
				infn = argv[i+1];
				i++;
				break;
			case 'd':
				dist_fn = argv[i+1];
				i++;
				break;
			case 'N':
				N = atoi(argv[i+1]);
				i++;
				break;
			case 'D':
				max_dist = atof(argv[i+1]);
				i++;
				break;
			case 's':
				seed = strtoul(argv[i+1],0,10);
				i++;
				break;
			case 'b':
				buildings_fn = argv[i+1];
				i++;
				break;
			case 'n':
				buildings_nodes_fn = argv[i+1];
				i++;
				break;
			case 'v':
				v = atof(argv[i+1]) / 3.6; /* speed is given by user in km / h */
				i++;
				break;
			case 'B':
				buildings_coords_fn = argv[i+1];
				i++;
				break;
			case 'c':
				trip_coords_out = argv[i+1];
				i++;
				break;
			case 'I':
				dists_ids = argv[i+1];
				i++;
				break;
			case 'p':
				busstops_pairs_fn = argv[i+1];
				i++;
				break;
			default:
				fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
				break;
		}
		else fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
	}
	
	std::mt19937_64 rng(seed);
	
	if(dist_fn == 0 || buildings_fn == 0) {
		fprintf(stderr,"Error: missing input files!\n");
		return 1;
	}
	if(trip_coords_out && !buildings_coords_fn) {
		fprintf(stderr,"Error: no building coordinates file given!\n");
		return 1;
	}
	
	//~ std::unordered_map<std::pair<uint64_t,uint64_t>,double,pair_hash> dists;
	std::unordered_map<std::pair<uint64_t,uint64_t>,unsigned int,pair_hash> ids;
	std::vector<std::pair<uint64_t,uint64_t> > pairs;
	std::vector<double> w;
	const unsigned int hours = 24;
	
	/* match to nodes (via buildings) and the associated distances */
	std::unordered_map<uint64_t,std::vector<building_node> > nodes;
	/* building coordinates */
	std::unordered_map<uint64_t,std::pair<double,double> > building_coords;
	
	/* replace bus stop IDs by matched pairs (if given) */
	busstops_pairs_t busstops_pairs;
	if(busstops_pairs_fn) {
		read_table2 rt(busstops_pairs_fn);
		while(rt.read_line()) {
			uint64_t n1,n2;
			if(!rt.read(n1,n2)) break;
			busstops_pairs.set(n1,n2);
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading bus stop pairs:\n");
			rt.write_error(stderr);
			return 1;
		}
	}
	
	/* read distances between nodes */
	distances dists;
	
	if(dists_ids) {
		if(!dists.open_dists(dist_fn,dists_ids)) return 1;
	}
	else if(!dists.read_dists(read_table2(dist_fn))) return 1;
	
	
	
	/* read match between bus stops, buildings and network nodes */
	{
		std::unordered_map<uint64_t,std::pair<uint64_t,double> > buildings_nodes;
		{
			read_table2 rt(buildings_nodes_fn);
			rt.set_delim(',');
			rt.read_line(); /* skip header */
			while(rt.read_line()) {
				uint64_t id,nid;
				double dist;
				if(!rt.read(id,nid,dist)) break;
				buildings_nodes[id] = std::make_pair(nid,dist);
			}
			if(rt.get_last_error() != T_EOF) {
				fprintf(stderr,"Error reading building data:\n");
				rt.write_error(stderr);
				return 1;
			}
		}
		{
			read_table2 rt(buildings_fn);
			rt.set_delim(',');
			rt.read_line(); /* skip header */
			while(rt.read_line()) {
				building_node n1;
				uint64_t sid;
				if(!rt.read(n1.pc,sid)) break;
				/* replace bus stop ID if it has a pair */
				sid = busstops_pairs.get(sid);
				const auto& tmp = buildings_nodes.at(n1.pc);
				n1.nid = tmp.first;
				n1.dist = tmp.second;
				nodes[sid].push_back(n1);
			}
			if(rt.get_last_error() != T_EOF) {
				fprintf(stderr,"Error reading building data:\n");
				rt.write_error(stderr);
				return 1;
			}
		}
	}
	
	/* read bus trip data */
	{
		read_table2 rt(infn,stdin);
		unsigned int nids = 0;
		unsigned int lines = 0;
		while(rt.read_line()) {
			unsigned int h;
			uint64_t n1,n2;
			unsigned int cnt;
			size_t id;
			if(!rt.read(read_bounds(h,0U,23U),n1,n2,cnt)) break;
			/* replace bus stop IDs if any of them has a pair */
			n1 = busstops_pairs.get(n1);
			n2 = busstops_pairs.get(n2);
			
			/* check if both bus stops have associated buildings */
			if(nodes.count(n1) == 0 || nodes.count(n2) == 0) continue;
			
			auto p = std::make_pair(n1,n2);
			auto it = ids.find(p);
			if(it == ids.end()) {
				id = nids;
				ids.insert(std::make_pair(p,id));
				pairs.push_back(p);
				w.insert(w.end(),hours,0.0);
				nids++;
			}
			else id = it->second;
			size_t pos = id*hours + h;
			w[pos] += cnt; /* it's possible that a "pair" has multiple entries */
			lines++;
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading trips:\n");
			rt.write_error(stderr);
			return 1;
		}
		fprintf(stderr,"%u records read, %u pairs\n",lines,nids);
	}
	
	/* read building coordinates (if needed) */
	if(buildings_coords_fn && trip_coords_out) {
		read_table2 rt(buildings_coords_fn);
		rt.set_delim(',');
		rt.read_line();
		while(rt.read_line()) {
			std::pair<double,double> c;
			uint64_t id;
			if(!rt.read(read_bounds_coords(c),id)) break;
			building_coords[id] = c;
		}
	}
	
	std::discrete_distribution<size_t> dst(w.cbegin(),w.cend());
	std::uniform_int_distribution<unsigned int> hdst(0,3599);
	
	FILE* fout = stdout;
	FILE* fout2 = 0;
	if(trip_coords_out) {
		fout2 = fopen(trip_coords_out,"w");
		if(!fout2) {
			fprintf(stderr,"Error opening output file %s!\n",trip_coords_out);
			return 1;
		}
	}
	for(unsigned int i=0;i<N;) {
		size_t x = dst(rng);
		unsigned int h = x%hours;
		unsigned int p1 = x/hours;
		unsigned int ts = h*3600 + hdst(rng);
		auto p = pairs[p1];
		
		const auto& n1 = nodes.at(p.first);
		const auto& n2 = nodes.at(p.second);
		
		/* select random building and corresponding node */
		size_t i1 = 0;
		size_t i2 = 0;
		if(n1.size() > 1) {
			std::uniform_int_distribution<size_t> tmp(0,n1.size()-1);
			i1 = tmp(rng);
		}
		if(n2.size() > 1) {
			std::uniform_int_distribution<size_t> tmp(0,n2.size()-1);
			i2 = tmp(rng);
		}
		
		double d1 = n1[i1].dist;
		double d2 = n2[i2].dist;
		double d3 = dists.get_dist(n1[i1].nid,n2[i2].nid);
		double dist = d1+d2+d3;
		if(max_dist > 0.0) if(dist > max_dist) continue;
		
		unsigned int ts2 = ts + (unsigned int)round(dist / v);
		
		fprintf(fout,"%u\t%u\t%u\t%u\t%lu\t%f\t%lu\t%f\t%f\t%lu\t%lu\n",i,i,ts,ts2,n1[i1].nid,d1,n2[i2].nid,d2,d3,n1[i1].pc,n2[i2].pc);
		if(fout2) {
			const auto& c1 = building_coords.at(n1[i1].pc);
			const auto& c2 = building_coords.at(n2[i2].pc);
			fprintf(fout2,"%u,%u,%u,%u,%f,%f,%f,%f\n",i,i,ts,ts2,c1.first,c1.second,c2.first,c2.second);
		}
		i++;
	}
	
	return 0;
}


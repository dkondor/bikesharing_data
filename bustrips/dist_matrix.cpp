/*
 * dist_matrix.cpp -- create binary distance matrix from a list
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
 * 
 */


#include <stdio.h>
#include <stdint.h>
#include <utility>
#include <vector>
#include <unordered_map>
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


int main(int argc, char **argv)
{
	char* fnin = 0;
	char* matrix_fn = 0; /* for output */
	for(int i=1;i<argc;i++) {
		if(argv[i][0] == '-') switch(argv[i][1]) {
			case 'i':
				fnin = argv[i+1];
				i++;
				break;
			case 'o':
				matrix_fn = argv[i+1];
				i++;
				break;
			default:
				fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
				break;
		}
		else fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
	}
	
	if(!matrix_fn) {
		fprintf(stderr,"Error: no output file name given!\n");
		return 1;
	}
	
	std::unordered_map<std::pair<uint64_t,uint64_t>,double,pair_hash> dists;
	
	{
		read_table2 rt(fnin,stdin);
		while(rt.read_line()) {
			uint64_t n1,n2;
			double d;
			if(!rt.read(n1,n2,d)) break;
			dists.insert(std::make_pair(std::make_pair(n1,n2),d));
			dists.insert(std::make_pair(std::make_pair(n2,n1),d));
		}
		if(rt.get_last_error() != T_EOF) {
			fprintf(stderr,"Error reading distances:\n");
			rt.write_error(stderr);
			return 1;
		}
	}
	
	std::unordered_map<uint64_t,size_t> nids;
	std::vector<uint64_t> nids2;
	
	for(const auto& x : dists) {
		if(nids.count(x.first.first) == 0) {
			nids[x.first.first] = nids2.size();
			nids2.push_back(x.first.first);
		}
		if(nids.count(x.first.second) == 0) {
			nids[x.first.second] = nids2.size();
			nids2.push_back(x.first.second);
		}
	}
	
	fprintf(stderr,"%lu nodes, %lu distances read\n",nids2.size(),dists.size());
	
	const uint64_t file_id = 0x47a9b290e72d9f21UL;
	FILE* fout = fopen(matrix_fn,"w");
	
	if(fwrite(&file_id,8,1,fout) != 1UL) {
		fprintf(stderr,"Error writing output file!\n");
		return 1;
	}
	uint64_t n = nids2.size();
	if(fwrite(&n,8,1,fout) != 1UL) {
		fprintf(stderr,"Error writing output file!\n");
		return 1;
	}
	
	for(uint64_t i=0;i<n;i++) for(uint64_t j=0;j<n;j++) {
		double dist = 0.0;
		if(i != j) dist = dists.at(std::make_pair(nids2[i],nids2[j]));
		if(fwrite(&dist,sizeof(double),1,fout)!=1UL) {
			fprintf(stderr,"Error writing output file!\n");
			return 1;
		}
	}
	fclose(fout);
	
	/* write IDs in proper order to stdout */
	fout = stdout;
	for(uint64_t i=0;i<n;i++) fprintf(fout,"%lu\n",nids2[i]);
	
	return 0;
}


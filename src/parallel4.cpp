#include <iostream>

#include <mcbsp.hpp>
#include <mcbsp-templates.hpp>
#include <thc.h>
#include <btree.h>
#include <btree_set.h>
#include <btree_container.h>
#include <cstdlib>
#include <queue>
#include "parallel_utils.cpp"

using namespace thc;
using namespace std;



//#define DEBUG
#ifdef DEBUG
#define debug_bsp_sync() bsp_sync()	
#define debug_print(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)            
#else
#define debug_bsp_sync()	
#define debug_print(a, ...)
#endif

// Third implementation
// Hash position of chessboard and module amount of cores
// Send positions to their own core and process it there
// - Less communication
// - Auto work distribution if hash is uniform
// Please be uniform
// - If every core counts their own unique positions then the sum should be ok.
// - We might get a lot of extra work though??? I don't think so, but we can not send things that have already been send to the core.

int max_depth = 4;
int cores;

class Test : public mcbsp::BSP_program {

	void spmd() {

		int nprocs = bsp_nprocs();
		int pid = bsp_pid();
		double start = bsp_time();

		bsp_set_tagsize<int>();
		bsp_sync();

		// Create root board position, we need this to distribute legal moves over the different cores.
		ChessRules* root = new ChessRules();
		CompressedPosition position = compress(root);

		if (pid == 0) printf("depth: %d, hash: %u\n", 0, hashPositionColor(position, 1));

		// Create the large PositionSet that will hold all positions for each cores
		vector<PositionSet> corePositionsWhiteToMove = vector<PositionSet>(nprocs, PositionSet());
		vector<PositionSet> corePositionsBlackToMove = vector<PositionSet>(nprocs, PositionSet());
		corePositionsBlackToMove[pid].insert(position);


		// Calculate children
		PositionSet compressedPositions = GetChildren(root);

		// Get the work that needs to be done for this core.
		PositionSet distribution = redistribute(compressedPositions, corePositionsWhiteToMove, 1, true)[pid];

		debug_bsp_sync();
		debug_print("(%d) distribution size: %lu\n", pid, distribution.size());


		// We are at this depth at the moment.
		int currentDepth = 1;

		// Soooooo.... Get the distribution		
		int totalSum = 0;

		while (currentDepth < max_depth) {
			// We go to the next depth
			currentDepth++;
			if (pid == 0) debug_print("\033[1;32mcurrent layer: %d\033[0m\n", currentDepth);

			// Get the positions for the next layer			

			// Redistribute the positions for the cores.
			vector<PositionSet> &colorToMove = (currentDepth % 2 == 0) ? corePositionsBlackToMove : corePositionsWhiteToMove;
			
			PositionSet nextLayer; 
			vector<PositionSet> redistribution;
			
			if ((max_depth + currentDepth) % 2 == 0) {
				PositionSet captured;				
				double st = bsp_time();
				executeExtraInfo(distribution, nextLayer, captured);
				
				debug_print("Execute size: %ld, time: %f\n", distribution.size(), bsp_time() - st);
				st = bsp_time();

				totalSum += nextLayer.size() + captured.size();
				
				if(pid==0) debug_print("Captured: %ld, color: %d\n", captured.size(), max_depth % 2);
				//redistribution = redistributeWithCaptured(nextLayer, captured, colorToMove, currentDepth);
				redistribution = redistribute(captured, colorToMove, currentDepth);							
				if(pid == 0) debug_print("Distribution time: %f\n", bsp_time() - st);
				st = bsp_time();
				distribution = nextLayer;
				distribution.insert(redistribution[pid].begin(), redistribution[pid].end());
				if(pid == 0) debug_print("c time: %f\n", bsp_time() - st);
			} else {
				double startTime = bsp_time();
				nextLayer = execute(distribution);	
				totalSum += nextLayer.size();			
				debug_print("Execute size: %ld, time: %f\n", distribution.size(), bsp_time() - startTime);
				if(pid==0) debug_print("NextLayer: %ld, color: %d\n", nextLayer.size(), max_depth % 2);
				
				redistribution = redistribute(nextLayer, colorToMove , currentDepth);							
				if(pid == 0) debug_print("Distribution time: %f\n", bsp_time() - startTime);
				distribution = redistribution[pid];				
			}
					
			double st = bsp_time();
			sendRedistribution(redistribution);
			bsp_sync();

			receiveDistribution(distribution);
			if(pid == 0) debug_print("Communication time: %f\n", bsp_time() - st);
			colorToMove[pid].insert(distribution.begin(), distribution.end());
			debug_bsp_sync();

			debug_print("(%d) Depth: %d, work done: %lu\n", pid, currentDepth, nextLayer.size());
			debug_print("(%d) distribution size: %lu\n", pid, distribution.size());
		}

		debug_print("There are %ld positions found in %d, time: %f \n", corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size(), (int)pid, bsp_time() - start);



		int total = merge(corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size());
		double timeItTook = bsp_time() - start;


		bsp_sync();
		
		// DEBUG METHODS

		int totalWork = merge(totalSum);
		
		double totalTime = merge<double>(totalSum / (float)totalWork  - (1.0 / (float)nprocs) ) + bsp_nprocs() - 1;

		// ----------------------
		debug_print("(%d) total work: %d, effective: %f\n", pid, totalSum, totalSum / (float)totalWork  - (1.0 / (float)nprocs) );
		
		bsp_sync();
		if (pid == 0) {			
			printf("\033[1;31m Total work: %d, effective: %f\n\033[0m", totalWork, (total / (float)totalWork));
			printf("Average work per core: %d\n", totalWork / nprocs);
			printf("Average effectivity per core: %f\n", totalTime / (float)nprocs);			
			printf("Total: %d, time: %f\n", total, timeItTook);
		}
	}

	/// Get the next layer of the distribution.
	PositionSet execute(PositionSet &distribution) {

		PositionSet nextDepth;
		for (auto it = distribution.begin(); it != distribution.end(); it++) {
			CompressedPosition position = *it;
			PositionSet children = GetCompressedChildren(position);

			nextDepth.insert(children.begin(), children.end());
		}

		return nextDepth;
	}
	
	//Mutually exclusive
	void executeExtraInfo(PositionSet &distribution, PositionSet &children, PositionSet &capture) {		
		for (auto it = distribution.begin(); it != distribution.end(); it++) {
			CompressedPosition position = *it;			
			GetChildrenExtraInfo(position, children, capture);			
		}
	}

	/// Returns what should be send to other cores
	/// This method distributes the positions over the different cores and adds them to the found list.
	vector<PositionSet> redistribute(PositionSet &list, vector<PositionSet> &cores, int depth, const bool force = false) {

		// Reserve space for redistribution set.
		vector<PositionSet> corePositionSets = vector<PositionSet>(bsp_nprocs(), PositionSet());

		CoreId currentCore = bsp_pid();
		int nprocs = bsp_nprocs();
	
		int color = (max_depth) % 2;				
		for (auto it = list.begin(); it != list.end(); it++) {
			CompressedPosition position = *it;
			BoardHash hash = hashPositionColor(position, color);

			CoreId core = nprocs == 1 ? 0 : hash % nprocs;

			// If we haven't seen this one yet, send it.
			if (cores[core].insert(position).second) {
				corePositionSets[core].insert(position);
			}
		}	

		return corePositionSets;
	}
	
	vector<PositionSet> redistributeWithCaptured(PositionSet &list, PositionSet &captured, vector<PositionSet> &cores, int depth, const bool force = false) {
		// Reserve space for redistribution set.
		vector<PositionSet> corePositionSets = vector<PositionSet>(bsp_nprocs(), PositionSet());

		CoreId currentCore = bsp_pid();
		int nprocs = bsp_nprocs();
	
		int color = (max_depth) % 2;				
		for (auto it = captured.begin(); it != captured.end(); it++) {
			CompressedPosition position = *it;
			BoardHash hash = hashPositionColor(position, color);
			CoreId core = nprocs == 1 ? 0 : hash % nprocs;			

			// If we haven't seen this one yet, send it.
			if (cores[core].insert(position).second) {
				corePositionSets[core].insert(position);
			}
		}		
		corePositionSets[currentCore].insert(list.begin(), list.end());
		corePositionSets[currentCore].erase(captured.begin(), captured.end());

		return corePositionSets;
	}

	void sendRedistribution(vector<PositionSet> &redistribution) {
		// Send the tag.
		int pid = bsp_pid();
		int sum = 0;
		for (int i = 0; i < redistribution.size(); i++) {
			if (i != pid) {
				vector<CompressedPosition> vec;
				vec.insert(vec.begin(), redistribution[i].begin(), redistribution[i].end());
				sum += vec.size();
				bsp_send(i, &pid, &vec[0], vec.size());
			}
		}
		//printf("(%d) Sending distribution: %d\n", bsp_pid(), sum);
	}

	void receiveDistribution(PositionSet &distribution) {
		unsigned int messages_received;
		size_t bytes_received;

		bsp_qsize(&messages_received, &bytes_received);

		for (unsigned int i = 0; i < messages_received; i++) {
			size_t payload;
			int sender;

			bsp_get_tag(&payload, &sender);

			if (payload == -1) {
				break;
			}

			int amount = payload / sizeof(CompressedPosition);
			vector<CompressedPosition> positions = vector<CompressedPosition>(amount);
			bsp_move<CompressedPosition>(&positions[0], amount);

			distribution.insert(positions.begin(), positions.end());
			positions.clear();
		}
	}

	//Send size to processors.	
	template<typename T>
	T merge(T amount) {
		int pid = bsp_pid();
		bsp_set_tagsize<T>();
		bsp_sync();
		for (int i = 0; i < bsp_nprocs(); i++) {
			bsp_send(i, &pid, &amount);
		}

		bsp_sync();

		T sum = 0;
		for (int i = 0; i < bsp_nprocs(); i++) {
			T tempamount;
			bsp_move<T>(&tempamount, sizeof(T));

			sum += tempamount;
		}

		return sum - bsp_nprocs() + 1;
	}

	virtual BSP_program * newInstance() {
		return new Test();
	}
};

void bs() {
	bsp_begin(cores)
	new Test()->spmd();
	bsp_end();
}
int main(int argc, char* args[]) {
	bsp_init(&argc, argv)
	printf("Start algorithm\n");

	

	switch (argc) {
	case 0:
	case 1:
		max_depth = 4;
		cores = 2;
		break;
	case 2:
		max_depth = 4;
		cores = atoi(args[1]);
		break;
	case 3:
		max_depth = atoi(args[2]);
		cores = atoi(args[1]);
		break;
	}

	printf("Depth: %d, cores: %d\n", max_depth, cores);

	if (max_depth == 0) {
		printf("Hardcoded position: 1, time 0\n");
		return 0;
	}

	//Test* test = new Test();
	bsp_init(&bs, argc, args);
	//test->begin(cores);
}

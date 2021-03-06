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

		// Create the large PositionSet that will hold all positions for each cores
		vector<PositionSet> corePositionsWhiteToMove = vector<PositionSet>(nprocs, PositionSet());
		vector<PositionSet> corePositionsBlackToMove = vector<PositionSet>(nprocs, PositionSet());
		corePositionsBlackToMove[pid].insert(position);


		// Calculate children
		PositionSet compressedPositions = GetChildren(root);

		// Get the work that needs to be done for this core.
		PositionSet distribution = redistribute(compressedPositions, corePositionsWhiteToMove)[pid];

		// We are at this depth at the moment.
		int currentDepth = 1;

		// Soooooo.... Get the distribution

		int totalSum = 0;
		while (currentDepth < max_depth) {
			// We go to the next depth
			currentDepth++;
			if (pid == 0) debug_print("\033[1;32mcurrent layer: %d\033[0m\n", currentDepth);
			
			// Get the positions for the next layer
			double st = bsp_time();
			PositionSet nextLayer = execute(distribution);
			totalSum += nextLayer.size();
			if (pid == 0) debug_print("Execute size: %d, time: %f\n", distribution.size(), bsp_time() - st);

			// Redistribute the positions for the cores.
			st = bsp_time();
			vector<PositionSet> redistribution = redistribute(nextLayer, (currentDepth % 2 == 0) ? corePositionsBlackToMove : corePositionsWhiteToMove);
			if (pid == 0) debug_print("Redistribution time: %f\n", bsp_time() - st);
			st = bsp_time();

			// Get the distribution for the next round
			distribution = redistribution[pid];

			// Empty the next layer because they are redistributed and put in corePositions
			//nextLayer.clear();

			// Send the redistribution to other cores.
			sendRedistribution(redistribution);

			bsp_sync();

			receiveDistribution(distribution);

			if (currentDepth % 2 == 0) { //If the current depth is white to play
				corePositionsBlackToMove[pid].insert(distribution.begin(), distribution.end());
			}
			else {
				corePositionsWhiteToMove[pid].insert(distribution.begin(), distribution.end());
			}			

			debug_print("(%d) Depth: %d, work done: %lu\n", pid, currentDepth, workSum);
			debug_print("(%d) distribution size: %lu\n", pid, distribution.size());
		}

		debug_print("There are %ld positions found in %d, time: %f \n", corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size(), (int)pid, bsp_time() - start);

		int totalWork = merge(totalSum);

		int total = merge(corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size());

		double timeItTook = bsp_time() - start;
		
		double totalTime = merge<double>(totalSum / (float)totalWork  - (1.0 / (float)nprocs) ) + bsp_nprocs() - 1;			
		
		//debug_print("(%d) total work: %d, effective: %f\n", pid, totalSum, totalSum / (float)totalWork );				
		if (pid == 0) {			
			printf("\033[1;31mbold Total work: %d, effective: %f\n\033[0m", totalWork, (total / (float)totalWork));
			printf("Average work per core: %d\n", totalWork / nprocs);
			printf("Average effectivity per core: %f\n", totalTime / (float)nprocs);			
			printf("Total: %d, Time: %f\n", total, timeItTook);
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

	/// Returns what should be send to other cores
	/// This method distributes the positions over the different cores and adds them to the found list.
	vector<PositionSet> redistribute(PositionSet &list, vector<PositionSet> &cores) {

		// Reserve space for redistribution set.
		vector<PositionSet> corePositionSets = vector<PositionSet>(bsp_nprocs(), PositionSet());

		int nprocs = bsp_nprocs();
		for (auto it = list.begin(); it != list.end(); it++) {
			CompressedPosition position = *it;		
			
			BoardHash hash = position.storage[0] +
				position.storage[1] +
				position.storage[2] +
				position.storage[3] +
				position.storage[4] +
				position.storage[5] +
				position.storage[6] +
				position.storage[7] +
				position.storage[8] +
				position.storage[9] +
				position.storage[10] +
				position.storage[11] +
				position.storage[12] +
				position.storage[13] +
				position.storage[14] +
				position.storage[15] +
				position.storage[16] +
				position.storage[17] +
				position.storage[18] +
				position.storage[19] +
				position.storage[21] +
				position.storage[22] +
				position.storage[23];
			unsigned int core = nprocs == 1 ? 0 : hash % nprocs;			

			// If we haven't seen this one yet, send it.
			if (cores[core].insert(position).second) {
				corePositionSets[core].insert(position);
			}
		}

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

int main(int argc, char* args[]) {

	printf("Start algorithm\n");

	int cores;

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

	Test* test = new Test();
	test->begin(cores);
}

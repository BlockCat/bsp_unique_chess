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

		while (currentDepth < max_depth) {
			// We go to the next depth
			currentDepth++;

			// Get the positions for the next layer
			PositionSet nextLayer = execute(distribution);

			unsigned long workSum = nextLayer.size();

			// Redistribute the positions for the cores.
			bool skipLayer = shouldSkipLayer(currentDepth);
			vector<PositionSet> &currentColorToMove = (currentDepth % 2 == 0) ? corePositionsBlackToMove : corePositionsWhiteToMove;
			vector<PositionSet> redistribution = redistribute(nextLayer, (currentDepth % 2 == 0) ? corePositionsBlackToMove : corePositionsWhiteToMove, currentDepth);

			// Get the distribution for the next round
			distribution = redistribution[pid];

			// Send the redistribution to other cores.
			if (!skipLayer) {
				sendRedistribution(redistribution);
				bsp_sync();
				receiveDistribution(distribution);

				((currentDepth % 2 == 0) ? corePositionsBlackToMove : corePositionsWhiteToMove)[pid].insert(distribution.begin(), distribution.end());
				printf("(%d) Depth: %d, work done: %lu\n", pid, currentDepth, workSum);			
			}
			
		}

		printf("There are %ld positions found in %d, time: %f \n", corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size(), (int)pid, bsp_time() - start);

		int totalSum = 0;
		for (int i = 0; i < bsp_nprocs(); i++) {
			totalSum += corePositionsBlackToMove[i].size() + corePositionsWhiteToMove[i].size();
		}

		int totalWork = merge(totalSum);
		int total = merge(corePositionsBlackToMove[pid].size() + corePositionsWhiteToMove[pid].size());

		printf("(%d) total: %d, time: %f\n", pid, total, bsp_time() - start);
		bsp_sync();
		printf("(%d) total work: %d, effective: %f\n", pid, totalSum, totalSum / (float)totalWork );
		bsp_sync();
		if (pid == 0) printf("\033[1;31mbold Total work: %d, effective: %f\n\033[0m", totalWork, total / (float)totalWork );
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
	vector<PositionSet> redistribute(PositionSet &list, vector<PositionSet> &cores, int depth, const bool force = false) {

		// We made a move in depth, and the list of position is in the next depth
		//depth = depth + 1;

		// Reserve space for redistribution set.
		vector<PositionSet> corePositionSets = vector<PositionSet>(bsp_nprocs(), PositionSet());

		CoreId currentCore = bsp_pid();
		int nprocs = bsp_nprocs();

		bool skipLayer = shouldSkipLayer(depth);
		int color = calculateColor();
		
		//If depth
		if (skipLayer) {
			cores[currentCore].insert(list.begin(), list.end());
			corePositionSets[currentCore].insert(list.begin(), list.end());
		} else {
			for (auto it = list.begin(); it != list.end(); it++) {
				CompressedPosition position = *it;
				BoardHash hash = hashPositionColor(position, color);

				CoreId core = nprocs == 1 ? 0 : hash % nprocs;

				// If we haven't seen this one yet, send it.
				if (cores[core].insert(position).second) {
					corePositionSets[core].insert(position);
				}
			}
		}		

		return corePositionSets;
	}

	int calculateColor() {	
		return 0;//(max_depth + 1) % 2;
	}

	bool shouldSkipLayer(int depth) {		
		return depth % 2 == 0; //(max_depth + depth + 1) % 2 == 0;
	}

	void sendRedistribution(vector<PositionSet> &redistribution) {
		// Send the tag.
		int pid = bsp_pid();

		for (int i = 0; i < redistribution.size(); i++) {
			if (i != pid) {
				vector<CompressedPosition> vec;
				vec.insert(vec.begin(), redistribution[i].begin(), redistribution[i].end());
				bsp_send(i, &pid, &vec[0], vec.size());
			}
		}
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
	int merge(int amount) {
		int pid = bsp_pid();

		for (int i = 0; i < bsp_nprocs(); i++) {
			bsp_send(i, &pid, &amount);
		}

		bsp_sync();

		int sum = 0;
		for (int i = 0; i < bsp_nprocs(); i++) {
			int tempamount;
			bsp_move<int>(&tempamount, 1);

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

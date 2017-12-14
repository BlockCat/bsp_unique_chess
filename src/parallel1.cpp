#include <iostream>

#include <mcbsp.hpp>
#include <mcbsp-templates.hpp>
#include <thc.h>
#include <btree.h>
#include <btree_set.h>
#include <btree_container.h>
#include <cstdlib>
#include <queue>
#include "main.h"

using namespace thc;
using namespace std;

PositionSet positionSet[4];
int max_depth = 4;


typedef struct {
	CompressedPosition compressed;
	unsigned long moves_amount;
	Move* moves;
} ChessPackage;

//#define DEBUG
#ifdef DEBUG
#define debug_bsp_sync() bsp_sync()	
#define debug_print(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)            
#endif // DEBUG
#ifndef DEBUG
#define debug_bsp_sync()	
#define debug_print(a, ...)
#endif


class Test : public mcbsp::BSP_program {

	CompressedPosition compress(ChessRules* board) {
		CompressedPosition position;
		board->Compress(position);
		return position;
	}

	void spmd() {

		int p = bsp_nprocs();
		int nproc = bsp_pid();
		double start = bsp_time();

		// Create root board position, we need this to distribute legal moves over the different cores.
		ChessRules* root = new ChessRules();
		CompressedPosition position = compress(root);

		// Calculate legal moves 
		vector<Move> legal;
		root->GenLegalMoveList(legal);

		// Add root position which we will add to the database at the end.
		vector<CompressedPosition> rootPositions;
		rootPositions.push_back(position);

		// Create queue that is distributed
		PositionSet queue;

		// Set tag size
		bsp_set_tagsize<int>();
		bsp_sync();


		debug_print("(%d) Initiating first iteration\n", nproc);

		// First iteration boards
		for (int i = 0; i < legal.size(); i++) {
			root->PlayMove(legal[i]);
			CompressedPosition ncompressed = compress(root);
			root->PopMove(legal[i]);

			// Add compressed to the root position
			rootPositions.push_back(ncompressed);

			// Add compressed to work load
			if ((nproc + i) % p == 0) {
				queue.insert(ncompressed);
			}
		}

		//Now everyone has the first 21 positions...

		// Blocks
		// -- Execute their own distribution. 
		// -- Send results to others (board, [moves])
		// -- Receive others results add them to the btree.
		// -- Continue with next

		PositionSet positions;
		execute(positions, queue, max_depth - 1);

		printf("There are %ld positions found in %d \n", positions.size(), (int)bsp_pid(), bsp_time() - start);
		send(positions);
		bsp_sync();
		receive(positions);

		positions.insert(rootPositions.begin(), rootPositions.end());

		bsp_sync();
		printf("There are %ld positions found in %d, time: %f \n", positions.size(), (int)bsp_pid(), bsp_time() - start);
	}

	// Execute own distribution
	void execute(PositionSet& positions, PositionSet& queue, const int depth) {

		if (depth <= 0) {
			// The end is reached, so we want to add the chess boards that should have been processed to the set.		
			// At this point, the positions in the queue should have been sent to other cores already.
			positions.insert(queue.begin(), queue.end());
			return;
		}

		// Prepare the working set for the next depth.
		PositionSet next;


		// Prepare iteration of the working queue.		
		debug_print("(%d)Start queue...\n", bsp_pid());
		PositionSet::iterator it = queue.begin();

		// Process each compressed position in the queue
		while (it != queue.end()) {

			// Get the compressed position from the iterator
			CompressedPosition compressed = *it;

			positions.insert(compressed);
			// For some reason it doesn't work correctly with this piece of code running.
			/*if (!positions.insert(compressed).second) {
			// This board position is already in the set.
			// If it's already in the set that means that it is already processed.
			// It can not be processed in a heigher depth, because then the position would not have been added.
			// If it's in the same then also.
			//it++;
			//continue;
			}*/

			ChessRules* board = new ChessRules();
			board->Decompress(compressed);

			// Generate legal moves
			vector<Move> legals;
			board->GenLegalMoveList(legals);

			for (int i = 0; i < legals.size(); i++) {
				CompressedPosition ncompressed;
				board->PlayMove(legals[i]);
				board->Compress(ncompressed);

				if (positions.count(ncompressed) == 0) {
					next.insert(ncompressed);
				}
				board->PopMove(legals[i]);
			}

			it++;
		}

		// All items in queue have been handled.
		queue.clear();
		debug_print("(%d)End queue...\n", (int)bsp_pid());

		// Once synchronized:
		debug_print("Next depth: %d, to be processed: %ld\n", max_depth - depth + 1, next.size());
		debug_bsp_sync();
		execute(positions, next, depth - 1);
	}

	void send(PositionSet next) {
		// Create a vector to send to the others.
		// Vectors are a continouous memory strip.
		CompressedQueue nextVector;
		nextVector.reserve(next.size());
		nextVector.insert(nextVector.begin(), next.begin(), next.end());

		// Add to send queue
		int pid = bsp_pid();
		size_t size = sizeof(CompressedPosition) * next.size();
		for (int i = 0; i < bsp_nprocs(); i++) {
			if (i != bsp_pid()) {
				bsp_send(i, &pid, (void*)&nextVector[0], size);
			}
		}
	}

	void receive(PositionSet &positions) {
		int pid = bsp_pid();
		// Go through each tag-message========
		debug_print("(%d)Messages starting...\n", pid);
		// Decide amount of messages received:
		unsigned int messages_received;
		size_t bytes_received;

		bsp_qsize(&messages_received, &bytes_received);

		debug_print("(%d) Received: %u, %lu\n", pid, messages_received, bytes_received);

		// Handle each message
		for (int i = 0; i < messages_received; i++) {
			size_t payload;
			int sender;

			bsp_get_tag(&payload, &sender);

			if (payload == -1) {
				break;
			}

			debug_print("(%d) Payload: %lu\n", pid, payload);

			CompressedPosition *recv_positions = (CompressedPosition*)malloc(payload);
			// Move the data into our object			
			bsp_move((void*)recv_positions, payload);

			int amount = payload / sizeof(CompressedPosition);

			for (int j = 0; j < amount; j++) {
				positions.insert(recv_positions[j]);
			}

			free(recv_positions);
		}
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

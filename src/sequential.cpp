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
     
    void spmd() {        
        double start = bsp_time();
		
		// Create root board position, we need this to distribute legal moves over the different cores.
		ChessRules* root = new ChessRules();		
		CompressedPosition position = compress(root);		

		// Calculate legal moves 
		PositionSet children = GetChildren(root);
		
		// Add root position which we will add to the database at the end.
		vector<CompressedPosition> rootPositions;
		rootPositions.push_back(position);
		rootPositions.insert(rootPositions.end(), children.begin(), children.end());
		
		PositionSet positions;

		positions.insert(rootPositions.begin(), rootPositions.end());

		execute(positions, children, max_depth - 1);	

		printf("There are %ld positions found, time: %f \n", positions.size(), bsp_time() - start);
    }        

	// Execute own distribution
	void execute(PositionSet& positions, PositionSet& queue, const int depth) {

		if (depth <= 0) {
			// The end is reached, so we want to add the chess boards that should have been processed to the set.		
			// At this point, the positions in the queue should have been sent to other cores already.
			//positions.insert(queue.begin(), queue.end());
			//queue.clear();
			return;
		}	

		// Prepare the working set for the next depth.
		PositionSet next;		

		// Prepare iteration of the working queue.				
		PositionSet::iterator it = queue.begin();

		// Process each compressed position in the queue
		while (it != queue.end()) {
			
			// Get the compressed position from the iterator
			CompressedPosition compressed = *it;

			positions.insert(compressed);

			// Generate legal moves					
			MoveList legal;
			ChessRules* board = decompress(compressed);
			board->GenLegalMoveList(legal);

			for (auto cit = legal.begin(); cit != legal.end(); cit++) {
				board->PushMove(*cit);

				CompressedPosition temp = compress(board);

				if (positions.insert(temp).second) {
					next.insert(temp);
				}				
				board->PopMove(*cit);
			}

			it = queue.erase(it);			
		}			
		execute(positions, next, depth - 1);
	}
    
    virtual BSP_program * newInstance() {
        return new Test();
    }
};

int main(int argc, char* args[]) {

	printf("Start algorithm\n");
	
	switch (argc) {
	case 0:
	case 1:
		max_depth = 4;		
		break;
	case 2:
	default:
		max_depth = atoi(args[1]);;		
		break;
	}

	printf("Depth: %d, cores: %d\n", max_depth, 1);

	Test* test = new Test();
    test->begin(1);
}

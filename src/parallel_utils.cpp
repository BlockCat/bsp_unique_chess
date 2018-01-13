#include <btree_set.h>
#include<thc.h>

using namespace thc;
using namespace std;

struct CompressedLess {
	CompressedLess() : n(2) {}
	CompressedLess(size_t length)
		: n(length) {
	}
	bool operator()(const CompressedPosition &a, const CompressedPosition &b) const {
		int as = memcmp(&a, &b, 24);
		return as < 0;
	}
	size_t n;
};

typedef unsigned int CoreId;
typedef btree::btree_set<CompressedPosition, CompressedLess> PositionSet;
typedef vector<CompressedPosition> PositionList;
typedef vector<Move> MoveList;

CompressedPosition compress(ChessRules* board) {
	CompressedPosition position;
	board->Compress(position);
	return position;
}

ChessRules* decompress(CompressedPosition position) {
	ChessRules* board = new ChessRules();
	board->Decompress(position);
	return board;
}

PositionSet GetChildren(ChessRules* board) {
	PositionSet children;
	
	MoveList legal;
	board->GenLegalMoveList(legal);

	for (auto it = legal.begin(); it != legal.end(); it++) {
		board->PlayMove(*it);
		children.insert(compress(board));
		board->PopMove(*it);
	}

	return children;
}

PositionSet GetCompressedChildren(CompressedPosition &position) {
	ChessRules* board = new ChessRules();
	board->Decompress(position);

	PositionSet children = GetChildren(board);
	free(board);

	return children;
}



// Hash the board
unsigned int HashBoard(CompressedPosition &position) {	

	// Decompress the board
	/*ChessPosition* board = new ChessPosition();
	board->Decompress(position);*/

	// Compress the board for the hash.	
	unsigned short hash = (position.storage[0] +
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
				position.storage[23]) >> 1;
	

	//unsigned int hash = board->HashCalculate();	

	return hash;
}


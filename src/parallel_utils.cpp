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

PositionSet GetCompressedChildren(CompressedPosition position) {
	ChessRules* board = new ChessRules();
	board->Decompress(position);

	PositionSet children = GetChildren(board);
	free(board);

	return children;
}



// Hash the board
unsigned int HashBoard(CompressedPosition position) {	

	// Decompress the board
	ChessPosition* board = new ChessPosition();
	board->Decompress(position);

	// Compress the board for the hash.
	CompressedPosition compressed;
	unsigned short hash = board->Compress(compressed);
	free(board);

	//unsigned int hash = board->HashCalculate();	

	return hash;
}


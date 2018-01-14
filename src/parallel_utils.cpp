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
typedef unsigned int BoardHash;
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
BoardHash HashBoard(CompressedPosition &position) {	

	// Decompress the board
	/*ChessPosition* board = new ChessPosition();
	board->Decompress(position);*/

	// Compress the board for the hash.	
	BoardHash bHash = (position.storage[0] +
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

		// Thank you random person on the internet
	// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
    bHash = ((bHash >> 16) ^ bHash) * 0x45d9f3b;
    bHash = ((bHash >> 16) ^ bHash) * 0x45d9f3b;	
    bHash = (bHash >> 16) ^ bHash;
	return bHash;
}

#define WHITE 0
#define BLACK 1


BoardHash hashPositionColor(const CompressedPosition &src, int color) {
	BoardHash bHash = 0;

	ChessPosition* position = new ChessPosition();
	position->Decompress(src);

	for (int i = 0; i < 64; i++) {
		char c = position->squares[i];		
		if (color == WHITE) {
			switch(c) {				
				case 'P': 
				case 'R': 
				case 'N':
				case 'B':
				case 'Q':
				case 'K':
					bHash += (i * 1000 + c) >> 1;
			}
		}
		if (color == BLACK) {
			switch(c) {				
				case 'p': 
				case 'r': 
				case 'n':
				case 'b':
				case 'q':
				case 'k':
					bHash += (i * 1000 + c) >> 1;
			}
		}		
	}

	// Thank you random person on the internet
	// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
    bHash = ((bHash >> 16) ^ bHash) * 0x45d9f3b;
    bHash = ((bHash >> 16) ^ bHash) * 0x45d9f3b;	
    bHash = (bHash >> 16) ^ bHash;
    return bHash;
}

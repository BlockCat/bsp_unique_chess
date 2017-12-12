#include <thc.h>
#include <stdio.h>

using namespace thc;
using namespace std;

class GameTree {   


    public: 

        GameTree(Move value, GameTree* parent) {
            this->parent = parent;            
            this->value = value;
        };
        
        ~GameTree() {
            int children_count = this->children.size();
            for (int i = 0; i < children_count; i++) {
                free(this->children[i]);
            }                
        };

        ChessRules* RecreatePosition();

        Move value;
        GameTree* parent; // Points to the parent
        vector<GameTree*> children; // Points to the children        
};

class BinaryGameTree {   

public:

    BinaryGameTree(GameTree* value) {        
        this->Move = value;
        this->LEFT = nullptr;
        this->RIGHT = nullptr;    
    };
    ~BinaryGameTree() {        
        free(this->LEFT);
        free(this->RIGHT);
    };


    BinaryGameTree* LEFT;
    BinaryGameTree* RIGHT;
    GameTree* Move;
    
    bool AddMove(GameTree* move, ChessRules* currentPosition);
    bool ContainsPosition(ChessRules* searchPosition);
};


#include <iostream>
#include <thc.h>
#include "GameTree.h"

using namespace thc;

ChessRules* GameTree::RecreatePosition() {
    // If this position has a parent
    if (this->parent != nullptr) {
        // Recreate the position of its parent
        ChessRules* rules = this->parent->RecreatePosition();
        // Play the move of the position
        rules->PlayMove(this->value);
    } else {
        // Has no parent, so this is the first start position of the game
        return new ChessRules();
    }
}

/// Returns true if the move was found in the tree
bool BinaryGameTree::ContainsPosition(ChessRules* currentPosition) {   

       // Recreate board
       ChessRules* rules = this->Move->RecreatePosition();
    
        if (currentPosition == rules) {
            return false;
        }
    
        // Check right
        if (currentPosition > rules) {
            if (this->RIGHT == nullptr) {
                return false;
            } else {
                return this->RIGHT->ContainsPosition(currentPosition);
            }
        } else {
            if (this->LEFT == nullptr) {
                return false;
            } else {
                return this->LEFT->ContainsPosition(currentPosition);
            }
        }
}

/// Returns true if move is successfully added, false if game already is in the tree.
bool BinaryGameTree::AddMove(GameTree* move, ChessRules* currentPosition) {   

    // Recreate board
    ChessRules* rules = this->Move->RecreatePosition();

    if (currentPosition == rules) {
        return false;
    }

    // Check right

    bool goRight = currentPosition > rules;

    free(rules);

    if (goRight) {
        if (this->RIGHT == nullptr) {
            // Add it to the tree            
            this->RIGHT = new BinaryGameTree(move);            
            return true;
        } else {
            return this->RIGHT->AddMove(move, currentPosition);
        }
    } else {
        if (this->LEFT == nullptr) {
            // Add it to the tree            
            this->LEFT = new BinaryGameTree(move);
            return true;
        } else {
            return this->LEFT->AddMove(move, currentPosition);
        }
    }
}
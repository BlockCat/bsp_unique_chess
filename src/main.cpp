#include <iostream>

#include <mcbsp.hpp>
#include <mcbsp-templates.hpp>
#include <thc.h>
#include <btree.h>
#include <btree_set.h>
#include <btree_container.h>
#include <cstdlib>
#include "main.h"

using namespace thc;
using namespace std;

PositionSet positionSet[4];
int depth = 4;

class Test : public mcbsp::BSP_program {

    void sequential(CompressedPosition* compressed, PositionSet* set, int depth) {
        
            // Add this position to the set    
            set->insert(*compressed);            
        
            // If no needs
            if (depth == 0) return;
        
            ChessRules* rules = new ChessRules();
            rules->Decompress(*compressed);
                
            vector<Move> legal;
            rules->GenLegalMoveList(legal);   
        
            for (int i = 0; i < legal.size(); i++) {
                // for every move, 
                ChessRules* copy = new ChessRules(*rules);
                copy->PlayMove(legal[i]);
                CompressedPosition nepos;
                copy->Compress(nepos);
                free(copy);
        
                sequential(&nepos, set, depth - 1);
            }                
            free(rules);
        }        
        
        void spmd() {            
            int p = bsp_nprocs();
            int nproc = bsp_pid();
            double start = bsp_time();

            positionSet[nproc] = PositionSet();

            printf("Core: %d\n", nproc);

            if (depth == 0) return;            

            ChessRules* root = new ChessRules();
            
            CompressedPosition compressed;
            root->Compress(compressed);
            positionSet[nproc].insert(compressed);
            
            vector<Move> legal;
            root->GenLegalMoveList(legal);

            for (int i = nproc; i < legal.size(); i+= p) {
                ChessRules* copy = new ChessRules(*root);
                copy->PlayMove(legal[i]);                    
                CompressedPosition nepos;
                copy->Compress(nepos);
                free(copy);        
                sequential(&nepos, &positionSet[nproc], depth - 1);
            }
            free(root);
            bsp_sync();

            printf("pid: %d, size: %d, time: %f\n", nproc, positionSet[nproc].size(), (bsp_time() - start));
            if (nproc == 0) {               
                
                PositionSet ps = PositionSet();
                
                for (int i = 0; i < p; i++) {
					ps.insert(positionSet[i].begin(), positionSet[i].end());
                    /*for(auto it = positionSet[i].begin(), end = positionSet[i].end(); it != end; it++) {
                        
                    }*/
                }
                printf("Result: %d, time: %f \n", ps.size(), (bsp_time() - start));
            }
        }        
    
        virtual BSP_program * newInstance() {
            return new Test();
        }
    };

int main(int argc, char* args[]) {

    int nproc = 0;   

    int i = bsp_nprocs();	
	
	depth = atoi(args[1]);
	int procs = atoi(args[2]);
	
	
	Test* test = new Test();
    test->begin(procs);
}

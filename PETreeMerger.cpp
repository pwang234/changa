#include "PETreeMerger.h"
#include "DataManager.h"

extern CProxy_TreePiece treeProxy; 
const char *typeString(NodeType type);

void PETreeMerger::mergeNonLocalRequests(GenericTreeNode *root, TreePiece *treePiece){
  submittedRoots.push_back(root);
  submittedTreePieces.push_back(treePiece);

  if(submittedRoots.length() == 1){
    CkLocMgr *locMgr = treeProxy.ckLocMgr();  
    locMgr->iterate(submittingTreePieceCounter);
  }

  if(submittedRoots.length() == submittingTreePieceCounter.count){
    mergeWalk(submittedRoots,submittedTreePieces);
    for(int i = 0; i < submittedTreePieces.length(); i++){
      submittedTreePieces[i]->mergeNonLocalRequestsDone();
    }
    submittingTreePieceCounter.reset();
    submittedRoots.length() = 0;
    submittedTreePieces.length() = 0;
  }
}

GenericTreeNode *PETreeMerger::mergeWalk(CkVec<GenericTreeNode*> &mergeList, CkVec<TreePiece*> &treePieceList){
  int nUnresolved;
  int pickedIndex;
  GenericTreeNode *pickedNode = DataManager::pickNodeFromMergeList(mergeList.length(),&mergeList[0],nUnresolved,pickedIndex);

  ostringstream oss;
  // picked node promises to give moments to others in need
  TreePiece *pickedTreePiece = treePieceList[pickedIndex];
  // make an entry in the table recording clients for nodes
  // only if there are any clients in the first place
  int numClients = 0;
  for(int i = 0; i < mergeList.length(); i++){
    if(i == pickedIndex || mergeList[i]->getType() != NonLocal) continue;
    numClients++;
  }

  if(numClients > 0){
    std::map<NodeKey,NonLocalMomentsClientList>::iterator it;
    it = pickedTreePiece->createTreeBuildMomentsEntry(pickedNode);
    for(int i = 0; i < mergeList.length(); i++){
      if(i == pickedIndex || mergeList[i]->getType() != NonLocal) continue;
      it->second.addClient(NonLocalMomentsClient(treePieceList[i],mergeList[i]));

    /*
      oss << "(" << 
        mergeList[i]->getKey() << "," << 
        typeString(mergeList[i]->getType()) << "," << 
        treePieceList[i]->getIndex() << "); ";
        */
    }
    
    MERGE_REMOTE_REQUESTS_VERBOSE("[%d] clients (%llu,%s,%d): %s\n", CkMyPe(), pickedNode->getKey(), typeString(pickedNode->getType()), pickedTreePiece->getIndex(), oss.str().c_str());
  }


  if(pickedNode->getType() == NonLocal){
    // this is the NonLocal node which will request moments
    // from the owner, and upon receiving them, will forward
    // them to the clients (see above)
    // there can be no Boundary nodes in the list with this NonLocal
    // node
    CkAssert(nUnresolved == 0);
    pickedTreePiece->sendRequestForNonLocalMoments(pickedNode);
    return pickedNode;
  }
  else if(nUnresolved >= 1){
    // we only treat Boundary nodes as unresolved
    CkAssert(pickedNode->getType() == Boundary);

    CkVec<GenericTreeNode*> nextLevelMergeList;
    CkVec<TreePiece*> nextLevelTreePieceList;

    for(int i = 0; i < pickedNode->numChildren(); i++){
      for(int j = 0; j < mergeList.size(); j++){
        if(mergeList[j]->getType() == Tree::Boundary){
          nextLevelMergeList.push_back(mergeList[j]->getChildren(i));
          nextLevelTreePieceList.push_back(treePieceList[j]);
        }
      }

      GenericTreeNode *mergedChild = mergeWalk(nextLevelMergeList,nextLevelTreePieceList);
      nextLevelMergeList.length() = 0;
      nextLevelTreePieceList.length() = 0;
      /*
      pickedNode->setChildren(i,mergedChild);
      if(mergedChild->getType() == Tree::Boundary ||
         mergedChild->getType() == Tree::NonLocal ||
         mergedChild->getType() == NonLocalBucket) mergedNodeIsInternal = false;
         */
    }

    /*
    if(mergedNodeIsInternal){
      pickedNode->setType(Internal);
    }
    */

    return pickedNode;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "DMID.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/Algos/DMID/VertexSumAggregator.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

/** Aggregator name of the DMID disassortativity vector DA */
static std::string const DA_AGG = "aggDA";

/** Aggregator name of the DMID leadership vector LS */
static std::string const LS_AGG = "aggLS";

/**
 * Aggregator name of the FollowerDegree vector where entry i determines how
 * many follower vertex i has
 */
static std::string const FD_AGG = "aggFD";

/**
 * Aggregator name of the DMID GlobalLeader vector where entry i determines
 * if vertex i is a global leader
 */
static std::string const GL_AGG = "aggGL";

/**
 * Aggregator name of the new Member flag Indicates if a vertex adopted a
 * behavior in the Cascading Behavior Phase of DMID
 **/
static std::string const NEW_MEMBER_AGG = "aggNewMember";

/**
 * Aggregator name of the all vertices assigned flag Indicates if there is a
 * vertex that did not adopted a behavior in the Cascading Behavior Phase of
 * DMID
 **/
static std::string const NOT_ALL_ASSIGNED_AGG = "aggNotAllAssigned";

/**
 * Aggregator name of the iteration count. Denotes the current iteration of
 * the cascading behavior phase times 3 (each step in the cascading behavior
 * phase is divided into 3 supersteps)
 */
static std::string const ITERATION_AGG = "aggIT";

/**
 * Aggregator name for the profitability threshold of the cascading behavior
 * phase of DMID
 */
static std::string const PROFITABILITY_AGG = "aggProfit";

/** Maximum steps for the random walk, corresponds to t*. Default = 1000 */
static uint64_t const long RW_ITERATIONBOUND = 10;


struct DMIDComputation
    : public VertexComputation<DMIDValue, float, DMIDMessage> {
  DMIDComputation() {}

  void compute(
      MessageIterator<DMIDMessage> const& messages) override {
    
    if (globalSuperstep() == 0) {
      superstep0(messages);
    }
    
    if (globalSuperstep() == 1) {
      superstep1(messages);
    }
    
    if (globalSuperstep() == 2) {
      superstep2(messages);
    }
    
    if ((globalSuperstep() >= 3 && globalSuperstep() <= RW_ITERATIONBOUND + 3)) {
      /**
       * TODO: Integrate a precision factor for the random walk phase. The
       * phase ends when the infinity norm of the difference between the
       * updated vector and the previous one is smaller than this factor.
       */
      superstepRW(messages);
    }
    
    long rwFinished = RW_ITERATIONBOUND + 4;
    if (globalSuperstep() == rwFinished) {
      superstep4(vertex, messages);
    }
    
    if (globalSuperstep() == rwFinished +1) {
      /**
       * Superstep 0 and RW_ITERATIONBOUND + 5 are identical. Therefore
       * call superstep0
       */
      superstep0(messages);
    }
    
    if (globalSuperstep() == rwFinished+2) {
      superstep6(messages);
    }
    
    if (globalSuperstep() == rwFinished + 3) {
      superstep7(messages);
      
    }
    
    int64_t iterationCounter = getAggregatedValue<int64_t>(ITERATION_AGG);
    
    if (globalSuperstep() >= rwFinished +4
        && (iterationCounter % 3 == 1 )) {
      superstep8(messages);
    }
    if (globalSuperstep() >= rwFinished +5
        && (iterationCounter % 3 == 2 )) {
      superstep9(messages);
    }
    if (globalSuperstep() >= rwFinished +6
        && (iterationCounter % 3 == 0 )) {
      superstep10(messages);
    }
  }
      
     /**
      * SUPERSTEP 0: send a message along all outgoing edges. Message contains
      * own VertexID and the edge weight.
      */
      void superstep0(MessageIterator<DMIDMessage> const& messages messages) {
        DMIDMessage message(pregelId(), 0);
        RangeIterator<Edge<int64_t>> edges = getEdges();
        for (Edge<int64_t>* edge : edges) {
          message.value = *edge->data(); // edge weight
          sendMessage(edge, message);
        }
      }
      
      /**
       * SUPERSTEP 1: Calculate and save new weightedInDegree. Send a message of
       * the form (ID,weightedInDegree) along all incoming edges (send every node
       * a reply)
       */
      void superstep1(MessageIterator<DMIDMessage> const& messages) {
        
        float weightedInDegree = 0.0;
        /** vertices that need a reply containing this vertexs weighted indegree */
        std::set<PregelId> predecessors);
        
        for (DMIDMessage const* message : messages) {
          /**
           * sum of all incoming edge weights (weightedInDegree).
           * msg.getValue() contains the edgeWeight of an incoming edge. msg
           * was send by msg.getSourceVertexId()
           *
           */
          predecessors.push(message->senderId);
          weightedInDegree += message->value;
        }
        /** update new weightedInDegree */
        mutableVertexValue()->weightedInDegree = weightedInDegree;
        
        // send to all predecessors
        DMIDMessage message(pregelId(), weightedInDegree);
        for (PregelId const& pid : predecessors) {
          sendMessage(pid, message);
        }
      }
      
      /**
       * SUPERSTEP 2: Iterate over all messages. Set the entries of the
       * disassortativity matrix column with index vertexID. Normalize the column.
       * Save the column as a part of the vertexValue. Aggregate DA with value 1/N
       * to initialize the Random Walk.
       */
      void superstep2(MessageIterator<DMIDMessage> const& messages) {
        
        
        /** Sum of all disVector entries */
        DMIDValue* vertexState = this->mutableVertexData();
        float ownWeight = vertexState->weightedInDegree;
        
        /** Set up new disCol */
        float disSum = 0;
        for (DMIDMessage const* message : messages) {
          
          /** Weight= weightedInDegree */
          float senderWeight = msg.getValue();
          float disValue = fabs(ownWeight - senderWeight);
          disSum += disValue;
          /** disValue = disassortativity value of senderID and ownID */
          vertexState->disCol[msg->pregelId] = disValue;
        }
        /**
         * Normalize the new disCol (Note: a new Vector is automatically
         * initialized 0.0f entries)
         */
        for (auto &pair : vertexState->disCol) {
          pair.second = pair.second / disSum;
          /** save the new disCol in the vertexValue */
        }
        
        //vertex.getValue().setDisCol(disVector, getTotalNumVertices());
        /**
         * Initialize DA for the RW steps with 1/N for your own entry
         * (aggregatedValue will be(1/N,..,1/N) in the next superstep)
         * */

        VertexSumAggregator *agg = (VertexSumAggregator*)getAggregator(DA_AGG);
        agg->aggregate(this->shard(), this->key(), 1.0 / context->vertexCount());
        //DoubleDenseVector init = new DoubleDenseVector(
        //                                               (int) getTotalNumVertices());
        //init.set((int) vertex.getId().get(), (double) 1.0
        //         / getTotalNumVertices());
        
        //aggregate(DA_AGG, init);
      }
      
      /**
       * SUPERSTEP 3 - RW_ITERATIONBOUND+3: Calculate entry DA^(t+1)_ownID using
       * DA^t and disCol. Save entry in the DA aggregator.
       */
      void superstepRW(MessageIterator<DMIDMessage> const& messages) {
        
        DMIDValue* vertexState = mutableVertexData();
        VertexSumAggregator *curDA = (VertexSumAggregator*)getAggregator(DA_AGG);

        //DoubleDenseVector curDA = getAggregatedValue(DA_AGG);
        //DoubleSparseVector disCol = vertex.getValue().getDisCol();
        
        /**
         * Calculate DA^(t+1)_ownID by multiplying DA^t (=curDA) and column
         * vertexID of T (=disCol)
         */
        /** (corresponds to vector matrix multiplication R^1xN * R^NxN) */
        double newEntryDA = 0.0;
        curDA->forEach([&](PregelID const& _id, double entry) {
          newEntryDA += entry * vertexState->disCol[_id];
        });
        curDA->aggregateValue(this->shard(), this->key(), newEntryDA);
      }
      
      /**
       * SUPERSTEP RW_ITERATIONBOUND+4: Calculate entry LS_ownID using DA^t* and
       * weightedInDegree. Save entry in the LS aggregator.
       */
      private void superstep4(MessageIterator<DMIDMessage> const& messages) {
        DMIDValue* vertexState = mutableVertexData();
        VertexSumAggregator *finalDA = (VertexSumAggregator*)getAggregator(DA_AGG);
        
        //DoubleDenseVector finalDA = getAggregatedValue(DA_AGG);
        double weightedInDegree = vertexState->weightedInDegree;// vertex.getValue().getWeightedInDegree();
        double lsAggValue = finalDA->getAggregatedValue(shard(), key()) * weightedInDegree;
        
        VertexSumAggregator *vecLS = (VertexSumAggregator*)getAggregator(LS_AGG);
        vecLS->aggregate(this->shard(), this->key(), lsAggValue);

        //finalDA->aggregateValue(shard(), key(), );
        //int vertexID = (int) vertex.getId().get();
        //DoubleDenseVector tmpLS = new DoubleDenseVector((int) getTotalNumVertices());
        //tmpLS.set(vertexID, (weightedInDegree * finalDA.get(vertexID)));
        //aggregate(LS_AGG, tmpLS);
      }
      
      /**
       * SUPERSTEP RW_IT+6: iterate over received messages. Determine if this
       * vertex has more influence on the sender than the sender has on this
       * vertex. If that is the case the sender is a possible follower of this
       * vertex and therefore vertex sends a message back containing the influence
       * value on the sender. The influence v-i has on v-j is (LS-i * w-ji) where
       * w-ji is the weight of the edge from v-j to v-i.
       * */
      private void superstep6(MessageIterator<DMIDMessage> const& messages) {
        
        //DoubleDenseVector vecLS = getAggregatedValue(LS_AGG);
        VertexSumAggregator *vecLS = (VertexSumAggregator*)getAggregator(LS_AGG);
        for (DMIDMessage const* message : messages) {
          
          PregelID senderID = message->senderId();
          /** Weight= weightedInDegree */

          float senderWeight = message->value();
          
          float myInfluence = senderWeight * vecLS->getAggregatedValue(this->shard(), this->key());
          
          /**
           * hasEdgeToSender determines if sender has influence on this vertex
           */
          bool hasEdgeToSender = false;
          for (Edge<float> *edge : getEdges()) {
            if (edge->targetShard() == senderID.shard && edge->key() == senderID.key) {
              
              hasEdgeToSender = true;
              /**
               * Has this vertex more influence on the sender than the
               * sender on this vertex?
               */
              float senderInfluence = *(edge->data()) * vecLS->getAggregatedValue(senderID.shard, senderID.key);
              if (myInfluence > senderInfluence) {
                /** send new message */
                DMIDMessage message(pregelId(), myInfluence);
                sendMessage(edge, message);
              }
            }
          }
          // WTF isn't that the same thing as above??!!
          if (!hasEdgeToSender) {
            /** send new message */
            DMIDMessage message(pregelId(), myInfluence);
            sendMessage(senderID, message);
          }
          
        }
      }
      
      /**
       * SUPERSTEP RW_IT+7: Find the local leader of this vertex. The local leader
       * is the sender of the message with the highest influence on this vertex.
       * There may be more then one local leader. Add 1/k to the FollowerDegree
       * (aggregator) of the k local leaders found.
       **/
      private void superstep7(MessageIterator<DMIDMessage> const& messages) {
        
        /** maximum influence on this vertex */
        float maxInfValue = 0;
        
        /** Set of possible local leader for this vertex. Contains VertexID's */
        std::set<PregelID> leaderSet;
        
        /** Find possible local leader */
        for (DMIDMessage const* message : messages) {
          
          if (message->value >= maxInfValue) {
            if (message->value > maxInfValue) {
              /** new distinct leader found. Clear set */
              leaderSet.clear();
            }
            /**
             * has at least the same influence as the other possible leader.
             * Add to set
             */
            leaderSet.insert(message->senderId);
            
          }
        }
        
        double leaderInit =  1.0 / leaderSet.size();
        VertexSumAggregator *vecFD = (VertexSumAggregator*) getAggregator(FD_AGG);
        for (PregelID const& _id : leaderSet) {
          vecFD->aggregate(_id.shard, _id.key, leaderInit);
        }
      }
      
      /**
       * SUPERSTEP RW_IT+8: Startpoint and first iteration point of the cascading
       * behavior phase.
       **/
      
      void superstep8(MessageIterator<DMIDMessage> const& messages) {
        
        DMIDValue* vertexState = mutableVertexData();
        float const* profitability = getAggregatedValue<float>(PROFITABILITY_AGG);
        
        auto const& it = vertexState->membershipDegree.find(this->pregelId());
        
        /** Is this vertex a global leader? Global Leader do not change behavior */
        if (it == vertexState->membershipDegree.end() || *profitability < 0) {
          bool const* notAllAssigned = getAggregatedValue<bool>(NOT_ALL_ASSIGNED_AGG);
          bool const* newMember = getAggregatedValue<bool>(NEW_MEMBER_AGG);
          if (*notAllAssigned) {
            /** There are vertices that are not part of any community */
            
            if (*newMember == false) {
              /**
               * There are no changes in the behavior cascade but not all
               * vertices are assigned
               */
              /** RESTART */
              /** set MemDeg back to initial value */
              initilaizeMemDeg();
            }
            /** ANOTHER ROUND */
            /**
             * every 0 entry means vertex is not part of this community
             * request all successors to send their behavior to these
             * specific communities.
             **/
            
            it = vertexState->membershipDegree.find(this->pregelId());
            /** In case of first init test again if vertex is leader */
            if (it == vertexState->membershipDegree.end()) {
              
              for (auto const& pair : vertexState->membershipDegree) {
                /**
                 * message of the form (ownID, community ID of interest)
                 */
                if(pair.second == 0) {
                  DMIDMessage message(pregelId(), pair.first);
                  sendMessageToAllEdges(message);
                }
                
              }
            } else {
              voteToHalt();
            }
          } else {
            
            /** All vertices are assigned to at least one community */
            /** TERMINATION */
            voteToHalt();
          }
        } else {
          voteToHalt();
        }
      }
      
      /**
       * SUPERSTEP RW_IT+9: Second iteration point of the cascading behavior
       * phase.
       **/
      private void superstep9(MessageIterator<DMIDMessage> const& messages) {
        DMIDValue* vertexState = mutableVertexData();

        /**
         * iterate over the requests to send this vertex behavior to these
         * specific communities
         */
        for (DMIDMessage const* message : messages) {
          PregelID const leaderID = message->leaderId;
          /**
           * send a message back with the same double entry if this vertex is
           * part of this specific community
           */
          
          if (vertexState->membershipDegree[leaderID] != 0.0) {
            DMIDMessage message(pregelId(), leaderID);
            sendMessage(message->senderId, leaderID);

            //LongDoubleMessage answerMsg = new LongDoubleMessage(vertex
            //                                                   .getId().get(), leaderID);
            //sendMessage(new LongWritable(msg.getSourceVertexId()),
            //            answerMsg);
          }
        }
      }
      
      /**
       * SUPERSTEP RW_IT+10: Third iteration point of the cascading behavior
       * phase.
       **/
      abstract void superstep10(MessageIterator<DMIDMessage> const& messages);
      
      /**
       * Initialize the MembershipDegree vector.
       **/
      private void initilaizeMemDeg() {
        DMIDValue* vertexState = mutableVertexData();
        
        VertexSumAggregator *vecGL = (VertexSumAggregator*)getAggregator(GL_AGG);
        //DoubleSparseVector vecGL = getAggregatedValue(GL_AGG);
        std::map<PregelID, float> newMemDeg;
        
        vecGL->forEach([&](PregelID const& _id, double entry) {
          if (entry != 0.0) {
            /** is entry _id a global leader?*/
            if (_id == this->pregelId()) {
              /**
               * This vertex is a global leader. Set Membership degree to
               * 100%
               */
              vertexState->membershipDegree.emplace(_id, 1.0f);
            } else {
              vertexState->membershipDegree.emplace(_id, 0.0f);
            }
          }
        });
        
        //double memDegree = vecGL->getAggregatedValue(shard(), key());
        //if (memDegree != 0.0) {
        //  vertexState->membershipDegree[this->pregelId] = 1.0;
        //}
      }
    }
};

VertexComputation<DMIDValue, int32_t, int64_t>*
DMID::createComputation(WorkerConfig const* config) const {
  return new DMIDComputation();
}

struct SCCGraphFormat : public GraphFormat<DMIDValue, int32_t> {
  const std::string _resultField;
  uint64_t vertexIdRange = 0;

  SCCGraphFormat(std::string const& result) : _resultField(result) {}

  void willLoadVertices(uint64_t count) override {
    // if we aren't running in a cluster it doesn't matter
    if (arangodb::ServerState::instance()->isRunningInCluster()) {
      arangodb::ClusterInfo* ci = arangodb::ClusterInfo::instance();
      if (ci) {
        vertexIdRange = ci->uniqid(count);
      }
    }
  }

  size_t estimatedEdgeSize() const override { return 0; };

  size_t copyVertexData(std::string const& documentId,
                        arangodb::velocypack::Slice document,
                        SCCValue* targetPtr, size_t maxSize) override {
    SCCValue* senders = (SCCValue*)targetPtr;
    senders->vertexID = vertexIdRange++;
    return sizeof(SCCValue);
  }

  size_t copyEdgeData(arangodb::velocypack::Slice document, int32_t* targetPtr,
                      size_t maxSize) override {
    return 0;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const SCCValue* ptr, size_t size) const override {
    SCCValue* senders = (SCCValue*)ptr;
    b.add(_resultField, VPackValue(senders->color));
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const int32_t* ptr,
                         size_t size) const override {
    return false;
  }
};

GraphFormat<SCCValue, int32_t>* SCC::inputFormat() const {
  return new DMIDValueGraphFormat(_resultField);
}

struct DMIDValueMasterContext : public MasterContext {
  DMIDValueMasterContext() {}  // TODO use _threashold
  void preGlobalSuperstep() override {
    
    /**
     * setAggregatorValue sets the value for the aggregator after master
     * compute, before starting vertex compute of the same superstep. Does
     * not work with OverwriteAggregators
     */
    
    
    int64_t* iterCount = getAggregatedValue<int64_t>(ITERATION_AGG);
    int64_t newIterCount = *iterCount + 1;
    bool hasCascadingStarted = false;
    if (*iterCount != 0) {
      /** Cascading behavior started increment the iteration count */
      aggregateValue<int64_t>(ITERATION_AGG, newIterCount);
      hasCascadingStarted = true;
    }
    
    if (getSuperstep() ==  RW_ITERATIONBOUND+ 8) {
      aggregateValue<bool>(NEW_MEMBER_AGG, false);
      aggregateValue<bool>(NOT_ALL_ASSIGNED_AGG, true);
      aggregateValue<int64_t>(ITERATION_AGG, 1);
      hasCascadingStarted = true;
      initializeGL();
    }
    if (hasCascadingStarted && (newIterCount % 3 == 1)) {
      /** first step of one iteration */
      int64_t* restartCountWritable = getAggregatedValue<int64_t>(RESTART_COUNTER_AGG);
      Long restartCount = restartCountWritable.get();
      BooleanWritable newMember = getAggregatedValue(DMIDComputation.NEW_MEMBER_AGG);
      BooleanWritable notAllAssigned = getAggregatedValue(DMIDComputation.NOT_ALL_ASSIGNED_AGG);
      
      if ((notAllAssigned.get() == true) && (newMember.get() == false)) {
        /**
         * RESTART Cascading Behavior with lower profitability threshold
         */
        
        float newThreshold = 1 - (PROFTIABILITY_DELTA * (restartCount + 1));
        setAggregatedValue<int64_t>(RESTART_COUNTER_AGG, restartCount + 1);
        setAggregatedValue<float>(PROFITABILITY_AGG, newThreshold);
        setAggregatedValue<int64_t>(ITERATION_AGG, 1);
      }
      
    }
    
    if (hasCascadingStarted && (*iterCount % 3 == 2)) {
      /** Second step of one iteration */
      /**
       * Set newMember aggregator and notAllAssigned aggregator back to
       * initial value
       */
      
      setAggregatedValue<bool>(NEW_MEMBER_AGG, false);
      setAggregatedValue<bool>(NOT_ALL_ASSIGNED_AGG, false);
    }
    
    if (LOG_AGGS) {
      if (getSuperstep() <= DMIDComputation.RW_ITERATIONBOUND + 4) {
        DoubleDenseVector convergedDA = getAggregatedValue(DMIDComputation.DA_AGG);
        System.out.print("Aggregator DA at step: "+getSuperstep()+" \nsize="
                         + getTotalNumVertices() + "\n{ ");
        for (int i = 0; i < getTotalNumVertices(); ++i) {
          System.out.print(convergedDA.get(i));
          if (i != getTotalNumVertices() - 1) {
            System.out.print(" , ");
          } else {
            System.out.println(" }\n");
          }
        }
      }
      if (getSuperstep() == DMIDComputation.RW_ITERATIONBOUND +6) {
        DoubleDenseVector leadershipVector = getAggregatedValue(DMIDComputation.LS_AGG);
        System.out.print("Aggregator LS: \nsize="
                         + getTotalNumVertices() + "\n{ ");
        for (int i = 0; i < getTotalNumVertices(); ++i) {
          System.out.print(leadershipVector.get(i));
          if (i != getTotalNumVertices() - 1) {
            System.out.print(" , ");
          } else {
            System.out.println(" }\n");
          }
        }
      }
    }
  }
  
  /**
   * Initilizes the global leader aggregator with 1 for every vertex with a
   * higher number of followers than the average.
   */
  void initializeGL() {
    DoubleSparseVector initGL = new DoubleSparseVector(
                                                       (int) getTotalNumVertices());
    VertexSumAggregator *vecFD = (VertexSumAggregator*)getAggregator(FD_AGG);

    double averageFD = 0.0;
    int numLocalLeader = 0;
    /** get averageFollower degree */
    f
    for (int i = 0; i < getTotalNumVertices(); ++i) {
      averageFD += vecFD.get(i);
      if (vecFD.get(i) != 0) {
        numLocalLeader++;
      }
    }
    if (numLocalLeader != 0) {
      averageFD = (double) averageFD / numLocalLeader;
    }
    /** set flag for globalLeader */
    if (LOG_AGGS) {
      System.out.print("Global Leader:");
    }
    for (int i = 0; i < vertexCount(); ++i) {
      if (vecFD.get(i) > averageFD) {
        initGL.set(i, 1.0);
        if (LOG_AGGS) {
          System.out.print("  " + i + "  ");
        }
      }
    }
    if (LOG_AGGS) {
      System.out.println("\n");
    }
    /** set Global Leader aggregator */
    setAggregatedValue(DMIDComputation.GL_AGG, initGL);
    
    /** set not all vertices assigned aggregator to true */
    setAggregatedValue(DMIDComputation.NOT_ALL_ASSIGNED_AGG,
                       new BooleanWritable(true));
    
  }
  
};

MasterContext* SCC::masterContext(VPackSlice userParams) const {
  return new SCCMasterContext();
}

IAggregator* SCC::aggregator(std::string const& name) const {
  if (name == DA_AGG) {  // permanent value
    return new VertexSumAggregator(false);// non perm
  } else if (name == LS_AGG) {
    return new VertexSumAggregator(true);// perm
  } else if (name == FD_AGG) {
    return new VertexSumAggregator(true);// perm
  } else if (name == GL_AGG) {
    return new VertexSumAggregator(true);// perm
  } else if (name == NEW_MEMBER_AGG) {
    return new BooleanOrAggregator(false); // non perm
  } else if (name == NOT_ALL_ASSIGNED_AGG) {
    return new BooleanOrAggregator(false); // non perm
  } else if (name == ITERATION_AGG) {
    return new MaxAggregator<int64_t>(0, true); // perm
  } else if (name == PROFITABILITY_AGG) {
    return new MaxAggregator<float>(0.5, true); // perm
  } else if (name == RESTART_COUNTER_AGG) {
    return new MaxAggregator<int64_t>(1, true); // perm
  }
  
  return nullptr;
}

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
    : public VertexComputation<DMIDValue, int32_t, int64_t> {
  DMIDComputation() {}

  void compute(
      MessageIterator<int64_t> const& messages) override {
    sup
    if (globalSuperstep() == 0) {
      superstep0(vertex, messages);
    }
    
    if (globalSuperstep() == 1) {
      superstep1(vertex, messages);
    }
    
    if (globalSuperstep() == 2) {
      superstep2(vertex, messages);
    }
    
    if ((globalSuperstep() >= 3 && globalSuperstep() <= RW_ITERATIONBOUND + 3)) {
      /**
       * TODO: Integrate a precision factor for the random walk phase. The
       * phase ends when the infinity norm of the difference between the
       * updated vector and the previous one is smaller than this factor.
       */
      superstepRW(vertex, messages);
    }
    
    long rwFinished = RW_ITERATIONBOUND + 4;
    if (getSuperstep() == rwFinished) {
      superstep4(vertex, messages);
    }
    
    if (getSuperstep() == rwFinished +1) {
      /**
       * Superstep 0 and RW_ITERATIONBOUND + 5 are identical. Therefore
       * call superstep0
       */
      superstep0(vertex, messages);
    }
    
    if (getSuperstep() == rwFinished+2) {
      superstep6(vertex, messages);
    }
    
    if (getSuperstep() == rwFinished + 3) {
      superstep7(vertex, messages);
      
    }
    
    LongWritable iterationCounter = getAggregatedValue(ITERATION_AGG);
    double it = iterationCounter.get();
    
    if (getSuperstep() >= rwFinished +4
        && (it % 3 == 1 )) {
      superstep8(vertex, messages);
    }
    if (getSuperstep() >= rwFinished +5
        && (it % 3 == 2 )) {
      superstep9(vertex, messages);
    }
    if (getSuperstep() >= rwFinished +6
        && (it % 3 == 0 )) {
      superstep10(vertex, messages);
    }
  }
      
      
      * SUPERSTEP 0: send a message along all outgoing edges. Message contains
      * own VertexID and the edge weight.
      */
      private void superstep0(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        long vertexID = vertex.getId().get();
        
        for (Edge<LongWritable, DoubleWritable> edge : vertex.getEdges()) {
          LongDoubleMessage msg = new LongDoubleMessage(vertexID, edge
                                                        .getValue().get());
          sendMessage(edge.getTargetVertexId(), msg);
        }
      }
      
      /**
       * SUPERSTEP 1: Calculate and save new weightedInDegree. Send a message of
       * the form (ID,weightedInDegree) along all incoming edges (send every node
       * a reply)
       */
      private void superstep1(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        double weightedInDegree = 0.0;
        
        /** vertices that need a reply containing this vertexs weighted indegree */
        HashSet<Long> predecessors = new HashSet<Long>();
        
        for (LongDoubleMessage msg : messages) {
          /**
           * sum of all incoming edge weights (weightedInDegree).
           * msg.getValue() contains the edgeWeight of an incoming edge. msg
           * was send by msg.getSourceVertexId()
           *
           */
          predecessors.add(msg.sourceVertexId);
          weightedInDegree += msg.getValue();
        }
        /** update new weightedInDegree */
        DMIDVertexValue vertexValue = vertex.getValue();
        vertexValue.setWeightedInDegree(weightedInDegree);
        vertex.setValue(vertexValue);
        
        LongDoubleMessage msg = new LongDoubleMessage(vertex.getId().get(),
                                                      weightedInDegree);
        for (Long msgTargetID : predecessors) {
          sendMessage(new LongWritable(msgTargetID), msg);
        }
      }
      
      /**
       * SUPERSTEP 2: Iterate over all messages. Set the entries of the
       * disassortativity matrix column with index vertexID. Normalize the column.
       * Save the column as a part of the vertexValue. Aggregate DA with value 1/N
       * to initialize the Random Walk.
       */
      private void superstep2(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        /** Weight= weightedInDegree */
        double senderWeight = 0.0;
        double ownWeight = vertex.getValue().getWeightedInDegree();
        long senderID;
        
        /** disValue = disassortativity value of senderID and ownID */
        double disValue = 0;
        DoubleSparseVector disVector = new DoubleSparseVector(
                                                              (int) getTotalNumVertices());
        
        /** Sum of all disVector entries */
        double disSum = 0;
        
        /** Set up new disCol */
        for (LongDoubleMessage msg : messages) {
          
          senderID = msg.getSourceVertexId();
          senderWeight = msg.getValue();
          
          disValue = Math.abs(ownWeight - senderWeight);
          disSum += disValue;
          
          disVector.set((int) senderID, disValue);
        }
        /**
         * Normalize the new disCol (Note: a new Vector is automatically
         * initialized 0.0f entries)
         */
        
        for (int i = 0; disSum != 0 && i < (int) getTotalNumVertices(); ++i) {
          disVector.set(i, (disVector.get(i) / disSum));
          
        }
        
        /** save the new disCol in the vertexValue */
        vertex.getValue().setDisCol(disVector, getTotalNumVertices());
        /**
         * Initialize DA for the RW steps with 1/N for your own entry
         * (aggregatedValue will be(1/N,..,1/N) in the next superstep)
         * */
        DoubleDenseVector init = new DoubleDenseVector(
                                                       (int) getTotalNumVertices());
        init.set((int) vertex.getId().get(), (double) 1.0
                 / getTotalNumVertices());
        
        aggregate(DA_AGG, init);
      }
      
      /**
       * SUPERSTEP 3 - RW_ITERATIONBOUND+3: Calculate entry DA^(t+1)_ownID using
       * DA^t and disCol. Save entry in the DA aggregator.
       */
      private void superstepRW(
                               Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                               Iterable<LongDoubleMessage> messages) {
        
        DoubleDenseVector curDA = getAggregatedValue(DA_AGG);
        DoubleSparseVector disCol = vertex.getValue().getDisCol();
        
        /**
         * Calculate DA^(t+1)_ownID by multiplying DA^t (=curDA) and column
         * vertexID of T (=disCol)
         */
        /** (corresponds to vector matrix multiplication R^1xN * R^NxN) */
        double newEntryDA = 0.0;
        for (int i = 0; i < getTotalNumVertices(); ++i) {
          newEntryDA += (curDA.get(i) * disCol.get(i));
        }
        
        DoubleDenseVector newDA = new DoubleDenseVector(
                                                        (int) getTotalNumVertices());
        newDA.set((int) vertex.getId().get(), newEntryDA);
        aggregate(DA_AGG, newDA);
        
      }
      
      /**
       * SUPERSTEP RW_ITERATIONBOUND+4: Calculate entry LS_ownID using DA^t* and
       * weightedInDegree. Save entry in the LS aggregator.
       */
      private void superstep4(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        DoubleDenseVector finalDA = getAggregatedValue(DA_AGG);
        double weightedInDegree = vertex.getValue().getWeightedInDegree();
        int vertexID = (int) vertex.getId().get();
        
        DoubleDenseVector tmpLS = new DoubleDenseVector(
                                                        (int) getTotalNumVertices());
        tmpLS.set(vertexID, (weightedInDegree * finalDA.get(vertexID)));
        
        aggregate(LS_AGG, tmpLS);
      }
      
      /**
       * SUPERSTEP RW_IT+6: iterate over received messages. Determine if this
       * vertex has more influence on the sender than the sender has on this
       * vertex. If that is the case the sender is a possible follower of this
       * vertex and therefore vertex sends a message back containing the influence
       * value on the sender. The influence v-i has on v-j is (LS-i * w-ji) where
       * w-ji is the weight of the edge from v-j to v-i.
       * */
      private void superstep6(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        /** Weight= weightedInDegree */
        double senderWeight = 0.0;
        long senderID;
        
        boolean hasEdgeToSender = false;
        
        for (LongDoubleMessage msg : messages) {
          
          senderID = msg.getSourceVertexId();
          senderWeight = msg.getValue();
          
          DoubleDenseVector vecLS = getAggregatedValue(LS_AGG);
          
          /**
           * hasEdgeToSender determines if sender has influence on this vertex
           */
          hasEdgeToSender = false;
          for (Edge<LongWritable, DoubleWritable> edge : vertex.getEdges()) {
            if (edge.getTargetVertexId().get() == senderID) {
              
              hasEdgeToSender = true;
              /**
               * Has this vertex more influence on the sender than the
               * sender on this vertex?
               */
              if (senderWeight * vecLS.get((int) vertex.getId().get()) > edge
                  .getValue().get() * vecLS.get((int) senderID)) {
                /** send new message */
                LongDoubleMessage newMsg = new LongDoubleMessage(vertex
                                                                 .getId().get(), senderWeight
                                                                 * vecLS.get((int) vertex.getId().get()));
                
                sendMessage(new LongWritable(senderID), newMsg);
              }
            }
          }
          if (!hasEdgeToSender) {
            /** send new message */
            LongDoubleMessage newMsg = new LongDoubleMessage(vertex.getId()
                                                             .get(), senderWeight
                                                             * vecLS.get((int) vertex.getId().get()));
            
            sendMessage(new LongWritable(senderID), newMsg);
          }
          
        }
      }
      
      /**
       * SUPERSTEP RW_IT+7: Find the local leader of this vertex. The local leader
       * is the sender of the message with the highest influence on this vertex.
       * There may be more then one local leader. Add 1/k to the FollowerDegree
       * (aggregator) of the k local leaders found.
       **/
      private void superstep7(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        /** maximum influence on this vertex */
        double maxInfValue = 0;
        
        /** Set of possible local leader for this vertex. Contains VertexID's */
        HashSet<Long> leaderSet = new HashSet<Long>();
        
        /** Find possible local leader */
        for (LongDoubleMessage msg : messages) {
          
          if (msg.getValue() >= maxInfValue) {
            if (msg.getValue() > maxInfValue) {
              /** new distinct leader found. Clear set */
              leaderSet.clear();
              
            }
            /**
             * has at least the same influence as the other possible leader.
             * Add to set
             */
            leaderSet.add(msg.getSourceVertexId());
            
          }
        }
        
        int leaderSetSize = leaderSet.size();
        DoubleSparseVector newFD = new DoubleSparseVector(
                                                          (int) getTotalNumVertices());
        
        for (Long leaderID : leaderSet) {
          newFD.set(leaderID.intValue(), (double) 1.0 / leaderSetSize);
          
        }
        
        aggregate(FD_AGG, newFD);
      }
      
      /**
       * SUPERSTEP RW_IT+8: Startpoint and first iteration point of the cascading
       * behavior phase.
       **/
      
      void superstep8(
                      Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                      Iterable<LongDoubleMessage> messages) {
        
        Long vertexID = vertex.getId().get();
        DoubleWritable profitability = getAggregatedValue(DMIDComputation.PROFITABILITY_AGG);
        /** Is this vertex a global leader? Global Leader do not change behavior */
        if (!vertex.getValue().getMembershipDegree().containsKey(vertexID)||profitability.get()<0) {
          BooleanWritable notAllAssigned = getAggregatedValue(NOT_ALL_ASSIGNED_AGG);
          BooleanWritable newMember = getAggregatedValue(NEW_MEMBER_AGG);
          if (notAllAssigned.get()) {
            /** There are vertices that are not part of any community */
            
            if (!newMember.get()) {
              /**
               * There are no changes in the behavior cascade but not all
               * vertices are assigned
               */
              /** RESTART */
              /** set MemDeg back to initial value */
              initilaizeMemDeg(vertex);
            }
            /** ANOTHER ROUND */
            /**
             * every 0 entry means vertex is not part of this community
             * request all successors to send their behavior to these
             * specific communities.
             **/
            
            /** In case of first init test again if vertex is leader */
            if (!vertex.getValue().getMembershipDegree()
                .containsKey(vertexID)) {
              
              for (Long leaderID : vertex.getValue()
                   .getMembershipDegree().keySet()) {
                /**
                 * message of the form (ownID, community ID of interest)
                 */
                if(vertex.getValue()
                   .getMembershipDegree().get(leaderID)==0){
                  LongDoubleMessage msg = new LongDoubleMessage(vertexID,
                                                                leaderID);
                  sendMessageToAllEdges(vertex, msg);
                }
                
              }
            } else {
              vertex.voteToHalt();
            }
          } else {
            
            /** All vertices are assigned to at least one community */
            /** TERMINATION */
            vertex.voteToHalt();
          }
        } else {
          vertex.voteToHalt();
        }
      }
      
      /**
       * SUPERSTEP RW_IT+9: Second iteration point of the cascading behavior
       * phase.
       **/
      private void superstep9(
                              Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                              Iterable<LongDoubleMessage> messages) {
        
        /**
         * iterate over the requests to send this vertex behavior to these
         * specific communities
         */
        for (LongDoubleMessage msg : messages) {
          
          long leaderID = ((long) msg.getValue());
          /**
           * send a message back with the same double entry if this vertex is
           * part of this specific community
           */
          
          if (vertex.getValue().getMembershipDegree().get(leaderID) != 0.0) {
            LongDoubleMessage answerMsg = new LongDoubleMessage(vertex
                                                                .getId().get(), leaderID);
            sendMessage(new LongWritable(msg.getSourceVertexId()),
                        answerMsg);
          }
        }
      }
      
      /**
       * SUPERSTEP RW_IT+10: Third iteration point of the cascading behavior
       * phase.
       **/
      abstract void superstep10(
                                Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex,
                                Iterable<LongDoubleMessage> messages); 
      
      /**
       * Initialize the MembershipDegree vector.
       **/
      private void initilaizeMemDeg(
                                    Vertex<LongWritable, DMIDVertexValue, DoubleWritable> vertex) {
        
        DoubleSparseVector vecGL = getAggregatedValue(GL_AGG);
        HashMap<Long, Double> newMemDeg = new HashMap<Long, Double>();
        
        for (long i = 0; i < getTotalNumVertices(); ++i) {
          /** only global leader have entries 1.0 the rest will return 0*/
          if(vecGL.get((int) i)!=0){
            /** is entry i a global leader?*/
            if(i == vertex.getId().get()){
              /**
               * This vertex is a global leader. Set Membership degree to
               * 100%
               */
              newMemDeg.put(new Long(i), new Double(1.0));
              
            }
            else{
              newMemDeg.put(new Long(i), new Double(0.0));
              
            }
            
          }
        }
        /** is entry i a global leader? */
        if (vecGL.get((int) vertex.getId().get())!=0 ) {
          /**
           * This vertex is a global leader. Set Membership degree to
           * 100%
           */
          newMemDeg.put(new Long(vertex.getId().get()), new Double(1.0));
        }
        
        vertex.getValue().setMembershipDegree(newMemDeg);
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
    
  };
  
  @Override
  public void compute() {
    /**
     * setAggregatorValue sets the value for the aggregator after master
     * compute, before starting vertex compute of the same superstep. Does
     * not work with OverwriteAggregators
     */
    
    
    LongWritable iterCount = getAggregatedValue(DMIDComputation.ITERATION_AGG);
    
    boolean hasCascadingStarted = false;
    LongWritable newIterCount = new LongWritable((iterCount.get() + 1));
    
    
    if (iterCount.get() != 0) {
      /** Cascading behavior started increment the iteration count */
      setAggregatedValue(DMIDComputation.ITERATION_AGG, newIterCount);
      hasCascadingStarted = true;
    }
    
    if (getSuperstep() ==  DMIDComputation.RW_ITERATIONBOUND+ 8) {
      setAggregatedValue(DMIDComputation.NEW_MEMBER_AGG,
                         new BooleanWritable(false));
      setAggregatedValue(DMIDComputation.NOT_ALL_ASSIGNED_AGG,
                         new BooleanWritable(true));
      setAggregatedValue(DMIDComputation.ITERATION_AGG, new LongWritable(1));
      hasCascadingStarted = true;
      initializeGL();
    }
    if (hasCascadingStarted && (newIterCount.get() % 3 == 1)) {
      /** first step of one iteration */
      LongWritable restartCountWritable = getAggregatedValue(RESTART_COUNTER_AGG);
      Long restartCount=restartCountWritable.get();
      BooleanWritable newMember = getAggregatedValue(DMIDComputation.NEW_MEMBER_AGG);
      BooleanWritable notAllAssigned = getAggregatedValue(DMIDComputation.NOT_ALL_ASSIGNED_AGG);
      
      if ((notAllAssigned.get() == true) && (newMember.get() == false)) {
        /**
         * RESTART Cascading Behavior with lower profitability threshold
         */
        
        
        double newThreshold = 1 - (PROFTIABILITY_DELTA * (restartCount + 1));
        
        setAggregatedValue(RESTART_COUNTER_AGG, new LongWritable(
                                                                 restartCount + 1));
        setAggregatedValue(DMIDComputation.PROFITABILITY_AGG,
                           new DoubleWritable(newThreshold));
        setAggregatedValue(DMIDComputation.ITERATION_AGG,
                           new LongWritable(1));
        
      }
      
    }
    
    if (hasCascadingStarted && (iterCount.get() % 3 == 2)) {
      /** Second step of one iteration */
      /**
       * Set newMember aggregator and notAllAssigned aggregator back to
       * initial value
       */
      
      setAggregatedValue(DMIDComputation.NEW_MEMBER_AGG,
                         new BooleanWritable(false));
      setAggregatedValue(DMIDComputation.NOT_ALL_ASSIGNED_AGG,
                         new BooleanWritable(false));
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
  private void initializeGL() {
    DoubleSparseVector initGL = new DoubleSparseVector(
                                                       (int) getTotalNumVertices());
    DoubleSparseVector vecFD = getAggregatedValue(DMIDComputation.FD_AGG);
    
    double averageFD = 0.0;
    int numLocalLeader = 0;
    /** get averageFollower degree */
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
    for (int i = 0; i < getTotalNumVertices(); ++i) {
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
    return new ValueAggregator<uint32_t>(SCCPhase::TRANSPOSE, true);
  } else if (name == kFoundNewMax) {
    return new BoolOrAggregator(false);  // non perm
  } else if (name == kConverged) {
    return new BoolOrAggregator(false);  // non perm
  }
  
  registerAggregator(DMIDComputation.DA_AGG,
                     DoubleDenseVectorSumAggregator.class);
		registerPersistentAggregator(DMIDComputation.LS_AGG,
                                 DoubleDenseVectorSumAggregator.class);
		registerPersistentAggregator(DMIDComputation.FD_AGG,
                                 DoubleSparseVectorSumAggregator.class);
		registerPersistentAggregator(DMIDComputation.GL_AGG,
                                 DoubleSparseVectorSumAggregator.class);
  
		registerAggregator(DMIDComputation.NEW_MEMBER_AGG,
                       BooleanOrAggregator.class);
		registerAggregator(DMIDComputation.NOT_ALL_ASSIGNED_AGG,
                       BooleanOrAggregator.class);
  
		registerPersistentAggregator(DMIDComputation.ITERATION_AGG,
                                 LongMaxAggregator.class);
  
		registerPersistentAggregator(DMIDComputation.PROFITABILITY_AGG,
                                 DoubleMaxAggregator.class);
		registerPersistentAggregator(RESTART_COUNTER_AGG,
                                 LongMaxAggregator.class);
		//registerAggregator(DMIDComputation.RW_INFINITYNORM_AGG,
    //	DoubleMaxAggregator.class);
    //registerAggregator(DMIDComputation.RW_FINISHED_AGG,
				//LongMaxAggregator.class);
  
		setAggregatedValue(DMIDComputation.PROFITABILITY_AGG,
                       new DoubleWritable(0.5));
		setAggregatedValue(RESTART_COUNTER_AGG, new LongWritable(1));
  setAggregatedValue(DMIDComputation.ITERATION_AGG, new LongWritable(0));
  
  return nullptr;
}

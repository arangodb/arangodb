////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Algos/DMID/DMIDMessageFormat.h"
#include "Pregel/Algos/DMID/VertexSumAggregator.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"

#include <cmath>

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

static std::string const RESTART_COUNTER_AGG = "aggRestart";

/** Maximum steps for the random walk, corresponds to t*. Default = 1000 */
static uint64_t const RW_ITERATIONBOUND = 10;

static const float PROFTIABILITY_DELTA = 0.1f;

static const bool LOG_AGGS = false;

struct DMIDComputation : public VertexComputation<DMIDValue, float, DMIDMessage> {
  DMIDComputation() {}

  void compute(MessageIterator<DMIDMessage> const& messages) override {
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

    uint64_t rwFinished = RW_ITERATIONBOUND + 4;
    if (globalSuperstep() == rwFinished) {
      superstep4(messages);
    }

    if (globalSuperstep() == rwFinished + 1) {
      /**
       * Superstep 0 and RW_ITERATIONBOUND + 5 are identical. Therefore
       * call superstep0
       */
      superstep0(messages);
    }

    if (globalSuperstep() == rwFinished + 2) {
      superstep6(messages);
    }

    if (globalSuperstep() == rwFinished + 3) {
      superstep7(messages);
    }

    int64_t const* iterationCounter = getAggregatedValue<int64_t>(ITERATION_AGG);
    int64_t it = *iterationCounter;

    if (globalSuperstep() >= rwFinished + 4 && (it % 3 == 1)) {
      superstep8(messages);
    }
    if (globalSuperstep() >= rwFinished + 5 && (it % 3 == 2)) {
      superstep9(messages);
    }
    if (globalSuperstep() >= rwFinished + 6 && (it % 3 == 0)) {
      superstep10(messages);
    }
  }

  /**
   * SUPERSTEP 0: send a message along all outgoing edges. Message contains
   * own VertexID and the edge weight.
   */
  void superstep0(MessageIterator<DMIDMessage> const& messages) {
    DMIDMessage message(pregelId(), 0);
    RangeIterator<Edge<float>> edges = getEdges();
    for(; edges.hasMore(); ++edges) {
      Edge<float>* edge = *edges;
      message.weight = edge->data();  // edge weight
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
    std::unordered_set<PregelID> predecessors;

    for (DMIDMessage const* message : messages) {
      /**
       * sum of all incoming edge weights (weightedInDegree).
       * msg.getValue() contains the edgeWeight of an incoming edge. msg
       * was send by msg.getSourceVertexId()
       *
       */
      predecessors.insert(message->senderId);
      weightedInDegree += message->weight;
    }
    /** update new weightedInDegree */
    mutableVertexData()->weightedInDegree = weightedInDegree;

    // send weighted degree to all predecessors
    DMIDMessage message(pregelId(), weightedInDegree);
    for (PregelID const& pid : predecessors) {
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
      float senderWeight = message->weight;
      float disValue = fabs(ownWeight - senderWeight);
      disSum += disValue;
      /** disValue = disassortativity value of senderID and ownID */
      vertexState->disCol[message->senderId] = disValue;
    }
    /**
     * Normalize the new disCol (Note: a new Vector is automatically
     * initialized 0.0f entries)
     */
    for (auto& pair : vertexState->disCol) {
      pair.second = pair.second / disSum;
      /** save the new disCol in the vertexValue */
    }

    // vertex.getValue().setDisCol(disVector, getTotalNumVertices());
    /**
     * Initialize DA for the RW steps with 1/N for your own entry
     * (aggregatedValue will be(1/N,..,1/N) in the next superstep)
     * */

    VertexSumAggregator* agg = (VertexSumAggregator*)getWriteAggregator(DA_AGG);
    agg->aggregate(this->shard(), this->key().toString(), 1.0 / context()->vertexCount());
    // DoubleDenseVector init = new DoubleDenseVector(
    //                                               (int)
    //                                               getTotalNumVertices());
    // init.set((int) vertex.getId().get(), (double) 1.0
    //         / getTotalNumVertices());

    // aggregate(DA_AGG, init);
  }

  /**
   * SUPERSTEP 3 - RW_ITERATIONBOUND+3: Calculate entry DA^(t+1)_ownID using
   * DA^t and disCol. Save entry in the DA aggregator.
   */
  void superstepRW(MessageIterator<DMIDMessage> const& messages) {
    DMIDValue* vertexState = mutableVertexData();
    VertexSumAggregator const* curDA = (VertexSumAggregator*)getReadAggregator(DA_AGG);

    // DoubleDenseVector curDA = getAggregatedValue(DA_AGG);
    // DoubleSparseVector disCol = vertex.getValue().getDisCol();

    /**
     * Calculate DA^(t+1)_ownID by multiplying DA^t (=curDA) and column
     * vertexID of T (=disCol)
     */
    /** (corresponds to vector matrix multiplication R^1xN * R^NxN) */
    double newEntryDA = 0.0;
    curDA->forEach([&](PregelID const& _id, double entry) {
      auto const& it = vertexState->disCol.find(_id);
      if (it != vertexState->disCol.end()) {  // sparse vector in the original
        newEntryDA += entry * it->second;
      }
    });
    VertexSumAggregator* newDA = (VertexSumAggregator*)getWriteAggregator(DA_AGG);
    newDA->aggregate(this->shard(), this->key().toString(), newEntryDA);
  }

  /**
   * SUPERSTEP RW_ITERATIONBOUND+4: Calculate entry LS_ownID using DA^t* and
   * weightedInDegree. Save entry in the LS aggregator.
   */
  void superstep4(MessageIterator<DMIDMessage> const& messages) {
    DMIDValue* vertexState = mutableVertexData();
    VertexSumAggregator const* finalDA = (VertexSumAggregator*)getReadAggregator(DA_AGG);

    // DoubleDenseVector finalDA = getAggregatedValue(DA_AGG);
    // vertex.getValue().getWeightedInDegree();
    double weightedInDegree = vertexState->weightedInDegree;
    double lsAggValue = finalDA->getAggregatedValue(shard(), key().toString()) * weightedInDegree;

    VertexSumAggregator* tmpLS = (VertexSumAggregator*)getWriteAggregator(LS_AGG);
    tmpLS->aggregate(this->shard(), this->key().toString(), lsAggValue);

    // finalDA->aggregateValue(shard(), key(), );
    // int vertexID = (int) vertex.getId().get();
    // DoubleDenseVector tmpLS = new DoubleDenseVector((int)
    // getTotalNumVertices());
    // tmpLS.set(vertexID, (weightedInDegree * finalDA.get(vertexID)));
    // aggregate(LS_AGG, tmpLS);
  }

  /**
   * SUPERSTEP RW_IT+6: iterate over received messages. Determine if this
   * vertex has more influence on the sender than the sender has on this
   * vertex. If that is the case the sender is a possible follower of this
   * vertex and therefore vertex sends a message back containing the influence
   * value on the sender. The influence v-i has on v-j is (LS-i * w-ji) where
   * w-ji is the weight of the edge from v-j to v-i.
   * */
  void superstep6(MessageIterator<DMIDMessage> const& messages) {
    // DoubleDenseVector vecLS = getAggregatedValue(LS_AGG);
    VertexSumAggregator const* vecLS = (VertexSumAggregator*)getReadAggregator(LS_AGG);
    for (DMIDMessage const* message : messages) {
      PregelID senderID = message->senderId;
      /** Weight= weightedInDegree */

      float senderWeight = message->weight;

      float myInfluence = (float)vecLS->getAggregatedValue(this->shard(), this->key().toString());
      myInfluence *= senderWeight;

      /**
       * hasEdgeToSender determines if sender has influence on this vertex
       */
      bool hasEdgeToSender = false;
      
      for (auto edges = getEdges(); edges.hasMore(); ++edges) {
        Edge<float>* edge = *edges;
        
        if (edge->targetShard() == senderID.shard && edge->toKey() == senderID.key) {
          hasEdgeToSender = true;
          /**
           * Has this vertex more influence on the sender than the
           * sender on this vertex?
           */
          float senderInfluence =
              (float)vecLS->getAggregatedValue(senderID.shard, senderID.key);
          senderInfluence *= edge->data();

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
  void superstep7(MessageIterator<DMIDMessage> const& messages) {
    /** maximum influence on this vertex */
    float maxInfValue = 0;

    /** Set of possible local leader for this vertex. Contains VertexID's */
    std::set<PregelID> leaderSet;

    /** Find possible local leader */
    for (DMIDMessage const* message : messages) {
      if (message->weight >= maxInfValue) {
        if (message->weight > maxInfValue) {
          /** new distinct leader found. Clear set */
          leaderSet.clear();
        }
        /**
         * has at least the same influence as the other possible leader.
         * Add to set
         */
        leaderSet.insert(message->senderId);
        maxInfValue = message->weight;
      }
    }

    double leaderInit = 1.0 / leaderSet.size();
    VertexSumAggregator* vecFD = (VertexSumAggregator*)getWriteAggregator(FD_AGG);
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
         * every 0 entry means vertex is not part of this community (yet)
         * request all successors to send their behavior to these
         * specific communities.
         **/

        auto const& it2 = vertexState->membershipDegree.find(this->pregelId());
        /** In case of first init test again if vertex is leader,
         or if we do not have connections */
        if (it2 == vertexState->membershipDegree.end() && getEdgeCount() > 0) {  // no
          for (auto const& pair : vertexState->membershipDegree) {
            /**
             * message of the form (ownID, community ID of interest)
             */
            if (pair.second == 0) {
              DMIDMessage message(pregelId(), pair.first);
              sendMessageToAllNeighbours(message);
            }
          }
        } else {
          voteHalt();
        }
      } else {
        /** All vertices are assigned to at least one community */
        /** TERMINATION */
        voteHalt();
      }
    } else {
      voteHalt();
    }
  }

  /**
   * SUPERSTEP RW_IT+9: Second iteration point of the cascading behavior
   * phase.
   **/
  void superstep9(MessageIterator<DMIDMessage> const& messages) {
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
        DMIDMessage data(pregelId(), leaderID);
        sendMessage(message->senderId, data);

        // LongDoubleMessage answerMsg = new LongDoubleMessage(vertex
        //                                                   .getId().get(),
        //                                                   leaderID);
        // sendMessage(new LongWritable(msg.getSourceVertexId()),
        //            answerMsg);
      }
    }
  }

  /**
   * SUPERSTEP RW_IT+10: Third iteration point of the cascading behavior
   * phase.
   **/
  void superstep10(MessageIterator<DMIDMessage> const& messages) {
    // long vertexID = vertex.getId().get();
    DMIDValue* vertexState = mutableVertexData();
    auto const& it = vertexState->membershipDegree.find(this->pregelId());

    /** Is this vertex a global leader? */
    if (it == vertexState->membershipDegree.end()) {  // no
      //! vertex.getValue().getMembershipDegree().containsKey(vertexID)
      /** counts per communities the number of successors which are member */
      std::map<PregelID, float> membershipCounter;
      // double previousCount = 0.0;

      for (DMIDMessage const* message : messages) {
        /**
         * the msg value is the index of the community the sender is a
         * member of
         */
        // Long leaderID = ((long) msg.getValue());
        PregelID const& leaderID = message->leaderId;
        // .containsKey(leaderID)
        if (membershipCounter.find(leaderID) != membershipCounter.end()) {
          /** increase count by 1 */
          membershipCounter[leaderID] += 1;  //.get(leaderID);
          // membershipCounter.put(leaderID, previousCount + 1);
        } else {
          membershipCounter[leaderID] = 1.0;
        }
      }
      /** profitability threshold */
      float const* threshold = getAggregatedValue<float>(PROFITABILITY_AGG);

      int64_t const* iterationCounter = getAggregatedValue<int64_t>(ITERATION_AGG);

      size_t m = std::min(getEdgeCount(), messages.size());
      for (auto const& pair : membershipCounter) {
        // FIXME
        // float const ttt = pair.second / getEdges().size();
        float const ttt = pair.second / m;
        if (ttt > *threshold) {
          /** its profitable to become a member, set value */
          float deg = 1.0f / std::pow(*iterationCounter / 3.0f, 2.0f);
          vertexState->membershipDegree[pair.first] = deg;
          aggregate<bool>(NEW_MEMBER_AGG, true);
        }
      }
      bool isPartOfAnyCommunity = false;
      // Map.Entry<Long, Double> entry :
      // vertex.getValue().getMembershipDegree().entrySet()
      for (auto const& pair : vertexState->membershipDegree) {
        if (pair.second != 0.0) {
          isPartOfAnyCommunity = true;
        }
      }
      if (!isPartOfAnyCommunity) {
        aggregate<bool>(NOT_ALL_ASSIGNED_AGG, true);
      }
    } else {
      voteHalt();
    }
  }

  /**
   * Initialize the MembershipDegree vector.
   **/
  void initilaizeMemDeg() {
    DMIDValue* vertexState = mutableVertexData();

    VertexSumAggregator const* vecGL = (VertexSumAggregator*)getReadAggregator(GL_AGG);
    // DoubleSparseVector vecGL = getAggregatedValue(GL_AGG);
    // std::map<PregelID, float> newMemDeg;

    vecGL->forEach([&](PregelID const& _id, double entry) {
      if (entry != 0.0) {
        /** is entry _id a global leader?*/
        if (_id == this->pregelId()) {
          /**
           * This vertex is a global leader. Set Membership degree to
           * 100%
           */
          vertexState->membershipDegree.try_emplace(_id, 1.0f);
        } else {
          vertexState->membershipDegree.try_emplace(_id, 0.0f);
        }
      }
    });

    // double memDegree = vecGL->getAggregatedValue(shard(), key());
    // if (memDegree != 0.0) {
    //  vertexState->membershipDegree[this->pregelId] = 1.0;
    //}
  }
};

VertexComputation<DMIDValue, float, DMIDMessage>* DMID::createComputation(WorkerConfig const* config) const {
  return new DMIDComputation();
}

struct DMIDGraphFormat : public GraphFormat<DMIDValue, float> {
  const std::string _resultField;
  unsigned _maxCommunities;

  explicit DMIDGraphFormat(application_features::ApplicationServer& server,
                           std::string const& result, unsigned mc)
      : GraphFormat<DMIDValue, float>(server),
        _resultField(result),
        _maxCommunities(mc) {}

  void copyVertexData(std::string const& documentId, arangodb::velocypack::Slice document,
                        DMIDValue& value) override {
    // SCCValue* senders = (SCCValue*)targetPtr;
    // senders->vertexID = _vertexIdRange++;
  }

  void copyEdgeData(arangodb::velocypack::Slice document, float& targetPtr) override {
    targetPtr = 1.0f;
  }

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           const DMIDValue* ptr, size_t size) const override {
    if (ptr->membershipDegree.size() > 0) {
      std::vector<std::pair<PregelID, float>> communities;
      for (std::pair<PregelID, float> pair : ptr->membershipDegree) {
        communities.push_back(pair);
      }
      std::sort(communities.begin(), communities.end(),
                [ptr](std::pair<PregelID, float> a, std::pair<PregelID, float> b) {
                  return ptr->membershipDegree.at(a.first) >
                         ptr->membershipDegree.at(b.first);
                });
      if (communities.empty()) {
        b.add(_resultField, VPackSlice::nullSlice());
      } else if (_maxCommunities == 1) {
        b.add(_resultField, VPackValuePair(communities[0].first.key.data(),
                                           communities[0].first.key.size(),
                                           VPackValueType::String));
      } else {
        // Output for DMID modularity calculator
        b.add(_resultField, VPackValue(VPackValueType::Array));
        for (auto const& pair : ptr->membershipDegree) {
          size_t i = arangodb::basics::StringUtils::uint64_trusted(pair.first.key.data(),
                                                                   pair.first.key.size());
          b.openArray();
          b.add(VPackValue(i));
          b.add(VPackValue(pair.second));
          b.close();
        }
        b.close();
        /*unsigned i = _maxCommunities;
        b.add(_resultField, VPackValue(VPackValueType::Object));
        for (std::pair<PregelID, float> const& pair : ptr->membershipDegree) {
          b.add(pair.first.key, VPackValue(pair.second));
          if (--i == 0) {
            break;
          }
        }
        b.close();*/
      }
    }
    return true;
  }

  bool buildEdgeDocument(arangodb::velocypack::Builder& b, const float* ptr,
                         size_t size) const override {
    return false;
  }
};

GraphFormat<DMIDValue, float>* DMID::inputFormat() const {
  return new DMIDGraphFormat(_server, _resultField, _maxCommunities);
}

struct DMIDMasterContext : public MasterContext {
  DMIDMasterContext() {}  // TODO use _threshold

  void preGlobalSuperstep() override {
    /**
     * setAggregatorValue sets the value for the aggregator after master
     * compute, before starting vertex compute of the same superstep. Does
     * not work with OverwriteAggregators
     */

    int64_t const* iterCount = getAggregatedValue<int64_t>(ITERATION_AGG);
    int64_t newIterCount = *iterCount + 1;
    bool hasCascadingStarted = false;
    if (*iterCount != 0) {  // will happen after GSS > RW_ITERATIONBOUND + 8
      /** Cascading behavior started increment the iteration count */
      aggregate<int64_t>(ITERATION_AGG, newIterCount);  // max aggregator
      hasCascadingStarted = true;
    }

    if (globalSuperstep() == RW_ITERATIONBOUND + 8) {
      setAggregatedValue<bool>(NEW_MEMBER_AGG, false);
      setAggregatedValue<bool>(NOT_ALL_ASSIGNED_AGG, true);
      setAggregatedValue<int64_t>(ITERATION_AGG, 1);
      hasCascadingStarted = true;
      initializeGL();  // initialize global leaders
    }
    if (hasCascadingStarted && (newIterCount % 3 == 1)) {
      /** first step of one iteration */
      int64_t const* restartCountWritable = getAggregatedValue<int64_t>(RESTART_COUNTER_AGG);
      int64_t restartCount = *restartCountWritable;
      bool const* newMember = getAggregatedValue<bool>(NEW_MEMBER_AGG);
      bool const* notAllAssigned = getAggregatedValue<bool>(NOT_ALL_ASSIGNED_AGG);

      if ((*notAllAssigned == true) && (*newMember == false)) {
        /**
         * RESTART Cascading Behavior with lower profitability threshold
         */

        float newThreshold = 1.05f - (PROFTIABILITY_DELTA * (restartCount + 1));
        newThreshold = std::max(0.05f, std::min(newThreshold, 0.95f));
        setAggregatedValue<int64_t>(RESTART_COUNTER_AGG, restartCount + 1);
        setAggregatedValue<float>(PROFITABILITY_AGG, newThreshold);
        setAggregatedValue<int64_t>(ITERATION_AGG, 1);
        LOG_TOPIC("99eb1", INFO, Logger::PREGEL) << "Restarting with threshold " << newThreshold;
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
      if (globalSuperstep() <= RW_ITERATIONBOUND + 4) {
        VertexSumAggregator* convergedDA = getAggregator<VertexSumAggregator>(DA_AGG);

        LOG_TOPIC("db510", INFO, Logger::PREGEL) << "Aggregator DA at step: " << globalSuperstep();
        convergedDA->forEach([&](PregelID const& _id, double entry) {
          LOG_TOPIC("df98d", INFO, Logger::PREGEL) << _id.key;
        });
      }
      if (globalSuperstep() == RW_ITERATIONBOUND + 6) {
        VertexSumAggregator* leadershipVector = getAggregator<VertexSumAggregator>(LS_AGG);
        leadershipVector->forEach([&](PregelID const& _id, double entry) {
          LOG_TOPIC("c82d2", INFO, Logger::PREGEL) << "Aggregator LS:" << _id.key;
        });
      }
    }
  }

  /**
   * Initilizes the global leader aggregator with 1 for every vertex with a
   * higher number of followers than the average.
   */
  void initializeGL() {
    /** set Global Leader aggregator */
    VertexSumAggregator* initGL = getAggregator<VertexSumAggregator>(GL_AGG);
    VertexSumAggregator* vecFD = getAggregator<VertexSumAggregator>(FD_AGG);

    double averageFD = 0.0;
    int numLocalLeader = 0;
    /** get averageFollower degree */
    vecFD->forEach([&](PregelID const& _id, double entry) {
      averageFD += entry;
      if (entry != 0) {
        numLocalLeader++;
      }
    });

    if (numLocalLeader != 0) {
      averageFD = (double)averageFD / numLocalLeader;
    }
    /** set flag for globalLeader */
    vecFD->forEach([&](PregelID const& _id, double entry) {
      if (entry > averageFD) {
        initGL->aggregate(_id.shard, _id.key, 1.0);
        LOG_TOPIC("a3665", INFO, Logger::PREGEL) << "Global Leader " << _id.key;
      }
    });
    // setAggregatedValue(DMIDComputation.GL_AGG, initGL);

    /** set not all vertices assigned aggregator to true */
    aggregate<bool>(NOT_ALL_ASSIGNED_AGG, true);
  }
};

MasterContext* DMID::masterContext(VPackSlice userParams) const {
  return new DMIDMasterContext();
}

IAggregator* DMID::aggregator(std::string const& name) const {
  if (name == DA_AGG) {                     // permanent value
    return new VertexSumAggregator(false);  // non perm
  } else if (name == LS_AGG) {
    return new VertexSumAggregator(true);  // perm
  } else if (name == FD_AGG) {
    return new VertexSumAggregator(true);  // perm
  } else if (name == GL_AGG) {
    return new VertexSumAggregator(true);  // perm
  } else if (name == NEW_MEMBER_AGG) {
    return new BoolOrAggregator(false);  // non perm
  } else if (name == NOT_ALL_ASSIGNED_AGG) {
    return new BoolOrAggregator(false);  // non perm
  } else if (name == ITERATION_AGG) {
    return new MaxAggregator<int64_t>(0, true);  // perm
  } else if (name == PROFITABILITY_AGG) {
    return new MaxAggregator<float>(0.95f, true);  // perm
  } else if (name == RESTART_COUNTER_AGG) {
    return new MaxAggregator<int64_t>(1, true);  // perm
  }

  return nullptr;
}

MessageFormat<DMIDMessage>* DMID::messageFormat() const {
  return new DMIDMessageFormat();
}

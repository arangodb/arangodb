#include <iostream>

#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "VertexComputation.hpp"
#include <random>
#include <vector>
#include <map>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;



template <typename V>
struct MSG {
    char type;
    vector <V> vids;
    V value;
    float p, q;
};


template <typename V, typename E, typename M>
struct N2VComputation : public VertexComputation<int64_t, int64_t, int64_t> {
    N2VComputation() {}

    void compute(MessageIterator<MSG> const& messages, int64_t walkLength) override {

        int64_t s = localSuperstep();
        vector<V>* state = mutableVertexData();
        vector <V> s_neigh, s_1_neigh;
        V s_1_id;
        float p, q;
        pair <Edge<E>, vector <V>>
        if (s==0) {
            map<V, float> probs = getNewProbs(this);
            V x = sample_V(probs); //sample 1st step x based on static edge weights
            state.push_back(x); //save x as step[0] in vertex value
            pair <Edge<E>, MSG> resN = sendN(this, x);
            sendMessage(resN->first, resN->second); //send (NEIG, this.vid, neighbor vids) to vertex x
            s_neigh = (resN->second)->vids;
        }
        else if(s < walkLength) {
            map <V, vector <MSG>> G;
            for (const MSG* msg : messages) {
                  p = msg->p;
                  q = msg->q;
                  if (MSG->type=='S') {
                      state.push_back(msg->value);
                  }
                  else {
                      int64_t vid = state.front();
                      if (does_exist(G, vid)){
                          for (row = G.begin(); row != G.end(); row++) {
                              if(row->first == vid){
                                  (row->second).push_back(msg);
                              }
                              if(row->first == msg->value) {
                                  pair <Edge<E>, MSG> resN = sendN(msg->value, this);
                                  s_1_neigh = (resN->second)->vids;
                                  s_1_id = (resN->second)->value;
                              }
                          }
                      }
                      else {
                          vector<MSG> vec;
                          vec.push_back(msg);
                          G.insert(pair<V, vector <MSG>>(vid, vec));
                          s_1_id = msg->value;
                      }
                  }
            }
            for (const map <V, vector <MSG>>* group : G){
                map<V, float> probs = getProbs(V, s_1_id, s_1_neigh, p, q); //compute transition probabilities
                V x = sample_V(probs); //sample x for next step
                V src = group->first;
                sendS(src, x);
                if (s+1<walkLength) {
                    pair <Edge<E>, MSG> resN = sendN(src, x);
                    sendMessage(resN->first, resN->second); //send (NEIG, this.vid, neighbor vids) to vertex x
                }
            }
            else {
                voteHalt();
            }
        }
    };

V sample_V(map<V, float> probs) {
    default_random_engine generator;
    vector<V> vids;
    vector<float> dist;
    for (; probs.hasMore(); ++probs) {
        pair<V, float> const* vp = *probs;
        vids.push_back(vp->first);
        dist.push_back(vp->second);
    }
    discrete_distribution<int> distribution dist;
    return vids[distribution(generator)];
}

map<V, float> getNewProbs(V v){
    map<V, float> probs;
    RangeIterator<Edge<E>> edges = v->getEdges();
    for (; edges.hasMore(); ++edges) {
        Edge<E> const* edge = *edges;
        pair<V, float> tmp;
        vector<V>::iterator it = find(s_1_neigh.begin(), s_1_neigh.end(), edge->toKey())
            probs.push_back(pair<V, float> (edge->toKey(), 1));// (1/q)*edge->weight
    }
    return probs;
}

map<V, float> getProbs(V v, V s_1_id, vector<V> s_1_neigh, float p, float q){
    map<V, float> probs;
    float sm=0;
    RangeIterator<Edge<E>> edges = v->getEdges();
    for (; edges.hasMore(); ++edges) {
        Edge<E> const* edge = *edges;
        pair<V, float> tmp;
        vector<V>::iterator it = find(s_1_neigh.begin(), s_1_neigh.end(), edge->toKey())
        if (it != s_1_neigh.end()) {
            probs.push_back(pair<V, float> (edge->toKey(), 1)); //1*edge->weight
//            sm+=1;
        }
        else {
            probs.push_back(pair<V, float> (edge->toKey(), 1/q));// (1/q)*edge->weight
//            sm+=1/q;
        }
    }
    probs.push_back(pair<V, float> (s_1_id, 1/p));// (1/p)*edge->weight
//    sm+=1/p;
//    for (; probs.hasMore(); ++probs) {
//        map<V, float> const* prob = *probs;
//        (prob->second->value) /= sm;
//    }
    return probs;
}

pair <Edge<E>, MSG> sendN(V v, V x) {
    RangeIterator<Edge<E>> edges = v->getEdges();
    vector <V> data;
    Edge<E> send_edge;
    for (; edges.hasMore(); ++edges) {
        Edge<E> const* edge = *edges;
        data.push_back(edge->toKey());
        if (edge->toKey()==x) {
            send_edge = edge; //send (NEIG, this.vid, neighbor vids) to vertex x
        }
    }
    MSG new_data = new MSG();
    new_data->type = 'N';
    new_data-> vids = data;
    new_data-> value = V;
    return pair <Edge<E>, MSG>(send_edge, new_data);
}

void sendS(V src, V x) {
    RangeIterator<Edge<E>> edges = src->getEdges();
    Edge<E> send_edge;
    for (; edges.hasMore(); ++edges) {
        Edge<E> const* edge = *edges;
        if (edge->toKey()==src) {
            send_edge = edge; //send (NEIG, this.vid, neighbor vids) to vertex x
        }
    }
    MSG new_data = new MSG()
    new_data->type = 'S';
    new_data-> value = x;
    sendMessage(send_edge, new_data); //send (STEP, src, x) to the vertex src
}

bool does_exist(const map <int64_t, vector <MSG>> &  v, int64_t item){
     map <V, vector <MSG>>::const_iterator row;
    for (row = v.begin(); row != v.end(); row++) {
        if(row->first == item)
            return true;
    }
    return false;
}

#include <utility>

#include "Graph/Providers/BaseStep.h"
#include "Graph/Providers/TypeAliases.h"
#include "Transaction/Methods.h"

namespace arangodb::graph {

template<class StepImpl>
class ClusterProvider;

class ClusterProviderStep
    : public arangodb::graph::BaseStep<ClusterProviderStep> {
 public:
  using StepType = EdgeType;

  class Vertex {
   public:
    explicit Vertex(VertexType v) : _vertex(std::move(v)) {}

    [[nodiscard]] VertexType const& getID() const;

    bool operator<(Vertex const& other) const noexcept {
      return _vertex < other._vertex;
    }

    bool operator>(Vertex const& other) const noexcept {
      return _vertex > other._vertex;
    }

    void setVertex(VertexType thisIsATest) { _vertex = std::move(thisIsATest); }

   private:
    VertexType _vertex;
  };

  class Edge {
   public:
    explicit Edge(EdgeType tkn) : _edge(std::move(tkn)) {}
    Edge() : _edge() {}

    void addToBuilder(ClusterProvider<ClusterProviderStep>& provider,
                      arangodb::velocypack::Builder& builder) const;
    [[nodiscard]] StepType const& getID()
        const;  // TODO: Performance Test compare EdgeType
                // <-> EdgeDocumentToken
    [[nodiscard]] bool isValid() const;

   private:
    EdgeType _edge;
  };

 private:
  explicit ClusterProviderStep(const VertexType& v);
  ClusterProviderStep(const VertexType& v, const EdgeType& edge, size_t prev);
  ClusterProviderStep(VertexType v, EdgeType edge, size_t prev, bool fetched);

 public:
  ~ClusterProviderStep();

  bool operator<(ClusterProviderStep const& other) const noexcept {
    return _vertex < other._vertex;
  }

  [[nodiscard]] Vertex const& getVertex() const { return _vertex; }
  [[nodiscard]] Edge const& getEdge() const { return _edge; }

  [[nodiscard]] std::string toString() const {
    return "<Step><Vertex>: " + _vertex.getID().toString();
  }
  [[nodiscard]] bool isProcessable() const { return !isLooseEnd(); }
  [[nodiscard]] bool isLooseEnd() const { return !_fetched; }

  [[nodiscard]] VertexType getVertexIdentifier() const {
    return _vertex.getID();
  }
  [[nodiscard]] StepType getEdgeIdentifier() const { return _edge.getID(); }

  [[nodiscard]] std::string getCollectionName() const {
    auto collectionNameResult = extractCollectionName(_vertex.getID());
    if (collectionNameResult.fail()) {
      THROW_ARANGO_EXCEPTION(collectionNameResult.result());
    }
    return collectionNameResult.get().first;
  };

  static bool isResponsible(transaction::Methods* trx);

  friend auto operator<<(std::ostream& out, ClusterProviderStep const& step)
      -> std::ostream&;

 private:
  void setFetched() { _fetched = true; }

 private:
  Vertex _vertex;
  Edge _edge;
  bool _fetched;
};

}  // namespace arangodb::graph
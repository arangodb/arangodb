using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static bool isValidId(VPackSlice id) {
  TRI_ASSERT(id.isString());
  return id.stringView().find('/') != std::string_view::npos;
}
}  // namespace

template<class FinderType>
ShortestPathExecutorInfos<FinderType>::ShortestPathExecutorInfos(
    QueryContext& query, std::unique_ptr<FinderType>&& finder,
    RegisterMapping&& registerMapping, GraphNode::InputVertex&& source,
    GraphNode::InputVertex&& target)
    : _query(query),
      _finder(std::move(finder)),
      _registerMapping(std::move(registerMapping)),
      _source(std::move(source)),
      _target(std::move(target)) {}

template<class FinderType>
FinderType& ShortestPathExecutorInfos<FinderType>::finder() const {
  TRI_ASSERT(_finder);
  return *_finder.get();
}

template<class FinderType>
QueryContext& ShortestPathExecutorInfos<FinderType>::query() noexcept {
  return _query;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::useRegisterForSourceInput() const {
  return _source.type == GraphNode::InputVertex::Type::REGISTER;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::useRegisterForTargetInput() const {
  return _target.type == GraphNode::InputVertex::Type::REGISTER;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getSourceInputRegister()
    const {
  // cppcheck-suppress ignoredReturnValue
  TRI_ASSERT(useRegisterForSourceInput());
  return _source.reg;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getTargetInputRegister()
    const {
  // cppcheck-suppress ignoredReturnValue
  TRI_ASSERT(useRegisterForTargetInput());
  return _target.reg;
}

template<class FinderType>
std::string const& ShortestPathExecutorInfos<FinderType>::getSourceInputValue()
    const {
  TRI_ASSERT(!useRegisterForSourceInput());
  return _source.value;
}

template<class FinderType>
std::string const& ShortestPathExecutorInfos<FinderType>::getTargetInputValue()
    const {
  TRI_ASSERT(!useRegisterForTargetInput());
  return _target.value;
}

template<class FinderType>
bool ShortestPathExecutorInfos<FinderType>::usesOutputRegister(
    OutputName type) const {
  return _registerMapping.find(type) != _registerMapping.end();
}

template<class FinderType>
GraphNode::InputVertex ShortestPathExecutorInfos<FinderType>::getSourceVertex()
    const noexcept {
  return _source;
}

template<class FinderType>
GraphNode::InputVertex ShortestPathExecutorInfos<FinderType>::getTargetVertex()
    const noexcept {
  return _target;
}

template<class FinderType>
static std::string typeToString(
    typename ShortestPathExecutorInfos<FinderType>::OutputName type) {
  switch (type) {
    case ShortestPathExecutorInfos<FinderType>::VERTEX:
      return std::string{"VERTEX"};
    case ShortestPathExecutorInfos<FinderType>::EDGE:
      return std::string{"EDGE"};
    default:
      return std::string{"<INVALID("} + std::to_string(type) + ")>";
  }
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::findRegisterChecked(
    OutputName type) const {
  auto const& it = _registerMapping.find(type);
  if (ADB_UNLIKELY(it == _registerMapping.end())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Logic error: requested unused register type " +
                                typeToString<FinderType>(type));
  }
  return it->second;
}

template<class FinderType>
RegisterId ShortestPathExecutorInfos<FinderType>::getOutputRegister(
    OutputName type) const {
  // cppcheck-suppress ignoredReturnValue
  TRI_ASSERT(usesOutputRegister(type));
  return findRegisterChecked(type);
}

template<class FinderType>
graph::TraverserCache* ShortestPathExecutorInfos<FinderType>::cache() const {
  TRI_ASSERT(false);  // TODO [GraphRefactor]:remove me later
  return nullptr;
}

template<class FinderType>
ShortestPathExecutor<FinderType>::ShortestPathExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _trx(infos.query().newTrxContext(), infos.query().getTrxTypeHint()),
      _inputRow{CreateInvalidInputRowHint{}},
      _finder{infos.finder()},
      _pathBuilder{&_trx},
      _posInPath(0),
      _pathLength(0),
      _sourceBuilder{},
      _targetBuilder{} {
  if (!_infos.useRegisterForSourceInput()) {
    _sourceBuilder.add(VPackValue(_infos.getSourceInputValue()));
  }
  if (!_infos.useRegisterForTargetInput()) {
    _targetBuilder.add(VPackValue(_infos.getTargetInputValue()));
  }
  // Make sure the finder does not contain any leftovers in case of
  // the executor being reconstructed.
  _finder.clear();
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::doSkipPath(AqlCall& call) -> size_t {
  auto skip = size_t{0};

  // call.getOffset() > 0 means we're in SKIP mode
  if (call.getOffset() > 0) {
    if (call.getOffset() < pathLengthAvailable()) {
      skip = call.getOffset();
    } else {
      skip = pathLengthAvailable();
    }
  } else {
    // call.getOffset() == 0, we might be in SKIP, PRODUCE, or
    // FASTFORWARD/FULLCOUNT, but we only FASTFORWARD/FULLCOUNT if
    // call.getLimit() == 0 as well.
    if (call.needsFullCount() && call.getLimit() == 0) {
      skip = pathLengthAvailable();
    }
  }
  _posInPath += skip;
  call.didSkip(skip);
  return skip;
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::getPathLength() const -> size_t {
  TRI_ASSERT(_pathBuilder->slice().hasKey(StaticStrings::GraphQueryVertices));
  return _pathBuilder->slice().get(StaticStrings::GraphQueryVertices).length();
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::doOutputPath(OutputAqlItemRow& output)
    -> void {
  // TODO [GraphRefactor]: This can be optimized. In case we only need to
  // write into the vertex or edge output register, we do not need to build
  // the whole complete path (as we do now). This will introduce an API change
  // We're planning this to achieve with version 4.0.
  TRI_ASSERT(_pathBuilder->slice().hasKey(StaticStrings::GraphQueryVertices));
  TRI_ASSERT(
      _pathBuilder->slice().get(StaticStrings::GraphQueryVertices).isArray());
  TRI_ASSERT(_pathBuilder->slice().hasKey(StaticStrings::GraphQueryEdges));
  TRI_ASSERT(
      _pathBuilder->slice().get(StaticStrings::GraphQueryEdges).isArray());
  VPackSlice verticesSlice{
      _pathBuilder->slice().get(StaticStrings::GraphQueryVertices)};
  VPackSlice edgesSlice{
      _pathBuilder->slice().get(StaticStrings::GraphQueryEdges)};
  _pathLength = verticesSlice.length();

  while (pathLengthAvailable() > 0 && !output.isFull()) {
    if (_infos.usesOutputRegister(
            ShortestPathExecutorInfos<FinderType>::VERTEX)) {
      AqlValue v{verticesSlice.at(_posInPath)};
      AqlValueGuard vertexGuard{v, true};

      output.moveValueInto(_infos.getOutputRegister(
                               ShortestPathExecutorInfos<FinderType>::VERTEX),
                           _inputRow, vertexGuard);
    }
    if (_infos.usesOutputRegister(
            ShortestPathExecutorInfos<FinderType>::EDGE)) {
      if (_posInPath == 0) {
        // First Edge is defined as NULL
        AqlValue e{AqlValueHintNull()};
        AqlValueGuard edgeGuard{e, true};
        output.moveValueInto(_infos.getOutputRegister(
                                 ShortestPathExecutorInfos<FinderType>::EDGE),
                             _inputRow, edgeGuard);
      } else {
        AqlValue e{edgesSlice.at(_posInPath - 1)};
        AqlValueGuard edgeGuard{e, true};

        output.moveValueInto(_infos.getOutputRegister(
                                 ShortestPathExecutorInfos<FinderType>::EDGE),
                             _inputRow, edgeGuard);
      }
    }
    output.advanceRow();
    _posInPath++;
  }
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::pathLengthAvailable() -> size_t {
  // Subtraction must not underflow
  TRI_ASSERT(_posInPath <= _pathLength);
  return _pathLength - _posInPath;
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::fetchPath(AqlItemBlockInputRange& input)
    -> bool {
  TRI_ASSERT(_finder.isDone());
  _finder.clear();
  _posInPath = 0;
  _pathLength = 0;
  _pathBuilder->clear();

  while (input.hasDataRow()) {
    auto source = VPackSlice{};
    auto target = VPackSlice{};
    std::tie(std::ignore, _inputRow) =
        input.nextDataRow(AqlItemBlockInputRange::HasDataRow{});

    // Ordering important here.
    // Read source and target vertex, then try to find a shortest path (if both
    // worked).
    if (getVertexId(_infos.getSourceVertex(), _inputRow, _sourceBuilder,
                    source) &&
        getVertexId(_infos.getTargetVertex(), _inputRow, _targetBuilder,
                    target)) {
      _finder.reset(arangodb::velocypack::HashedStringRef(source),
                    arangodb::velocypack::HashedStringRef(target));
      if (_finder.getNextPath(*_pathBuilder.builder())) {
        _pathLength = getPathLength();
        _posInPath = 0;
        return true;
      }
    }
  }
  // Note that we only return false if
  // the input does not have a data row, so if we return false
  // here, we are DONE (we cannot produce any output anymore).
  return false;
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::produceRows(
    AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  while (!output.isFull()) {
    if (pathLengthAvailable() == 0) {
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), _finder.stealStats(), AqlCall{}};
      }
    } else {
      doOutputPath(output);
    }
  }

  if (pathLengthAvailable() == 0) {
    return {input.upstreamState(), _finder.stealStats(), AqlCall{}};
  } else {
    return {ExecutorState::HASMORE, _finder.stealStats(), AqlCall{}};
  }
}

template<class FinderType>
auto ShortestPathExecutor<FinderType>::skipRowsRange(
    AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  auto skipped = size_t{0};

  while (true) {
    skipped += doSkipPath(call);

    if (pathLengthAvailable() == 0) {
      if (!fetchPath(input)) {
        TRI_ASSERT(!input.hasDataRow());
        return {input.upstreamState(), _finder.stealStats(), skipped,
                AqlCall{}};
      }
    } else {
      // if we end up here there is path available, but
      // we have skipped as much as we were asked to.
      TRI_ASSERT(call.getOffset() == 0);
      return {ExecutorState::HASMORE, _finder.stealStats(), skipped, AqlCall{}};
    }
  }
}

template<class FinderType>
bool ShortestPathExecutor<FinderType>::getVertexId(
    GraphNode::InputVertex const& vertex, InputAqlItemRow& row,
    VPackBuilder& builder, VPackSlice& id) {
  switch (vertex.type) {
    case GraphNode::InputVertex::Type::REGISTER: {
      AqlValue const& in = row.getValue(vertex.reg);
      if (in.isObject()) {
        try {
          auto idString = _trx.extractIdString(in.slice());
          builder.clear();
          builder.add(VPackValue(idString));
          id = builder.slice();
          // Guaranteed by extractIdValue
          TRI_ASSERT(::isValidId(id));
        } catch (...) {
          // _id or _key not present... ignore this error and fall through
          // returning no path
          return false;
        }
        return true;
      } else if (in.isString()) {
        id = in.slice();
        // Validation
        if (!::isValidId(id)) {
          _infos.query().warnings().registerWarning(
              TRI_ERROR_BAD_PARAMETER,
              "Invalid input for Shortest Path: "
              "Only id strings or objects with "
              "_id are allowed");
          return false;
        }
        return true;
      } else {
        _infos.query().warnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for Shortest Path: "
            "Only id strings or objects with "
            "_id are allowed");
        return false;
      }
    }
    case GraphNode::InputVertex::Type::CONSTANT: {
      id = builder.slice();
      if (!::isValidId(id)) {
        _infos.query().warnings().registerWarning(
            TRI_ERROR_BAD_PARAMETER,
            "Invalid input for Shortest Path: "
            "Only id strings or objects with "
            "_id are allowed");
        return false;
      }
      return true;
    }
  }
  return false;
}

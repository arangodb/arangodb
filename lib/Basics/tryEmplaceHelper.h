#ifndef ARANGODB_BASICS_TRYEMPLACEHELPER_H
#define ARANGODB_BASICS_TRYEMPLACEHELPER_H 1

#include <type_traits>
#include <memory>
namespace arangodb {
template <class Lambda>
struct lazyConstruct {
  using type = std::invoke_result_t<const Lambda&>;
  constexpr lazyConstruct(Lambda&& factory) : factory_(std::move(factory)) {}
  constexpr operator type() const noexcept(std::is_nothrow_invocable_v<const Lambda&>) {
    return factory_();
  }
  Lambda factory_;
};
}
#endif

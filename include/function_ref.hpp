#pragma once
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

template <typename> class function_ref;

template <typename R, typename... Args> class function_ref<R(Args...)> {
public:
  template <typename F>
    requires(!std::same_as<std::remove_cvref_t<F>, function_ref> &&
             std::invocable<F &, Args...>)
  function_ref(F &&callable) noexcept
      : m_object{const_cast<void *>(
            static_cast<const void *>(std::addressof(callable)))},
        m_invoke{[](void *const object, Args... args) -> R {
          return (*static_cast<std::remove_reference_t<F> *>(object))(
              std::forward<Args>(args)...);
        }} {}

  R operator()(Args... args) const {
    return m_invoke(m_object, std::forward<Args>(args)...);
  }

private:
  void *m_object{nullptr};
  R (*m_invoke)(void *, Args...){nullptr};
};

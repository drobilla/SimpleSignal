// CC0 Public Domain: http://creativecommons.org/publicdomain/zero/1.0/
#pragma once

#include <functional>
#include <vector>
#include <list>

namespace Simple {

namespace Lib {

/// ProtoSignal is the template implementation for callback list.
template<typename, typename> class ProtoSignal;  // undefined

/// Base class for collectors
template<typename Result>
struct Collector {
  using CollectorResult = Result;
  explicit Collector() : result_() {}
  const CollectorResult& result() const { return result_; }
protected:
  Result result_;
};

/// CollectorLast returns the result of the last signal handler from a signal emission.
template<typename Result>
struct CollectorLast : public Collector<Result> {
  inline bool operator() (Result r) { this->result_ = r; return true; }
};

/// CollectorLast specialisation for signals with void return type.
template<>
struct CollectorLast<void> {
  using CollectorResult = void;
  void        result()         {}
  inline bool operator()(void) { return true; }
};

template<class R, class ... Args>
struct ProtoConnection {
  using CbFunction = std::function<R(Args ...)>;
  typename std::list<CbFunction>::iterator iter;
public:
  ProtoConnection(typename std::list<CbFunction>::iterator iter) : iter(iter) {}
  ProtoConnection(const ProtoConnection&& move) : iter(move.iter) {}
  ProtoConnection(const ProtoConnection& copy)       = delete;
  ProtoConnection& operator=(const ProtoConnection&) = delete;
};

/// ProtoSignal template specialised for the callback signature and collector.
template<class Collector, class R, class ... Args>
class ProtoSignal<R(Args ...), Collector>
{
protected:
  using CbFunction      = std::function<R(Args ...)>;
  using Result          = typename CbFunction::result_type;
  using CollectorResult = typename Collector::CollectorResult;
private:
  std::list<CbFunction> callbacks_;
  ProtoSignal (const ProtoSignal& copy) = delete;
  ProtoSignal& operator=(const ProtoSignal& assign) = delete;
public:
  using Connection = ProtoConnection<R, Args ...>;

  ProtoSignal () {}

  /// Add a new function as signal handler, return connection handle.
  Connection connect(const CbFunction&cb) {
    callbacks_.push_back(cb);
    return Connection(--callbacks_.end());
  }

  /// Remove a signal handler through its handle, return true if a handler was removed.
  bool disconnect(Connection& connection) {
    if (connection.iter == callbacks_.end()) {
      return false;
    }
    callbacks_.erase(connection.iter);
    connection.iter = callbacks_.end();
    return true;
  }

  /// invoke for callbacks with a return value
  template<class IR, class... IArgs>
  inline bool invoke(Collector&                          collector,
                     const std::function<IR(IArgs ...)>& callback,
                     Args...                             args) {
    return collector(callback(args...));
  }

  /// invoke specialization for callbacks with void return type
  template<class... IArgs>
  inline bool invoke(Collector&                            collector,
                     const std::function<void(IArgs ...)>& callback,
                     Args...                               args) {
    callback(args...);
    return collector();
  }

  /// Emit a signal, i.e. invoke all its callbacks and collect return types with the Collector.
  CollectorResult emit(Args... args) {
    Collector collector;
    for (auto f = callbacks_.begin(); f != callbacks_.end(); ) {
      auto next = f;
      ++next;
      if (!this->invoke(collector, *f, args ...)) {
        break;
      }
      f = next;
    }
    return collector.result();
  }

  /// Number of connected slots.
  size_t size() {
    return callbacks_.size();
  }
};

}   // Lib
// namespace Simple

/**
 * Signal is a template type providing an interface for arbitrary callback lists.
 * A signal type needs to be declared with the function signature of its callbacks,
 * and optionally a return result collector class type.
 * Signal callbacks can be added with operator+= to a signal and removed with operator-=, using
 * a callback connection ID return by operator+= as argument.
 * The callbacks of a signal are invoked with the emit() method and arguments according to the signature.
 * The result returned by emit() depends on the signal collector class. By default, the result of
 * the last callback is returned from emit(). Collectors can be implemented to accumulate callback
 * results or to halt a running emissions in correspondance to callback results.
 * The signal implementation is safe against recursion, so callbacks may be removed and
 * added during a signal emission and recursive emit() calls are also safe.
 * The overhead of an unused signal is intentionally kept very low, around the size of a single pointer.
 * Note that the Signal template types is non-copyable.
 */
template <typename Signature,
          class Collector = Lib::CollectorLast<typename std::function<Signature>::result_type> >
struct Signal : Lib::ProtoSignal<Signature, Collector>
{
  using ProtoSignal = Lib::ProtoSignal<Signature, Collector>;
  using CbFunction  = typename ProtoSignal::CbFunction;
};

/// Create a std::function by binding `object` to `method`.
template<class Instance, class Class, class R, class... Args>
std::function<R (Args...)>
slot(Instance& object, R (Class::* method)(Args...))
{
  return [&object, method] (Args... args) { return (object.*method)(args...); };
}

/// Create a std::function by binding `object` to `method`.
template<class Class, class R, class... Args>
std::function<R (Args...)>
slot(Class* object, R (Class::* method)(Args...))
{
  return [object, method] (Args... args) { return (object->*method)(args...); };
}

/// Keep signal emissions going until a given test returns false.
template<typename Result, bool (*test)(Result)>
struct CollectorUntil : public Lib::Collector<Result> {
  inline bool operator()(Result r) {
    this->result_ = r;
    return test(this->result_ ) ? true : false;
  }
};

/// CollectorVector returns the result of the all signal handlers from a signal emission in a std::vector.
template<typename Result>
struct CollectorVector : public Lib::Collector< std::vector<Result> >{
  inline bool operator() (Result r) {
    this->result_.push_back (r);
    return true;
  }
};

/// CollectorReduce allows custom reduction of results
template<typename Result, typename Reducer>
struct CollectorReduce : public Lib::Collector<Result>{
  Reducer reducer_;
  inline bool operator()(Result r) {
    this->result_ = reducer_(this->result_, r);
    return true;
  }
};

} // Simple

#define TOKENPASTE(x, y) x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)

// An object that calls a given function when it's destroyed.
template <typename Cleanup> struct Defer {
    Cleanup _dtor_func;

    Defer(Cleanup dtor_func)
        : _dtor_func(std::move(dtor_func))
    {
    }

    Defer(Defer &&) = default;
    Defer(const Defer &) = default;

    ~Defer() { _dtor_func(); }
};

template <typename Cleanup> inline Defer<Cleanup> make_deferred(Cleanup dtor_func)
{
    return Defer<Cleanup>(dtor_func);
}

// Convenience macro. Creates a variable name like deferred_statement_<line_number>
#define DEFER(C) auto TOKENPASTE2(deferred_statement_, __LINE__) = make_deferred(C)

// Another convenience macro. Wraps the given statement in a lambda.
#define DEFERSTAT(statement) DEFER([&]() { statement; })
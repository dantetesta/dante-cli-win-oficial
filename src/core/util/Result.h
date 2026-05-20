#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace dante::core {

struct Error {
    int code{0};
    std::string message;

    Error() = default;
    Error(int c, std::string m) : code(c), message(std::move(m)) {}
};

template <typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)) {}
    Result(Error err) : m_value(std::move(err)) {}

    static Result<T> ok(T v) { return Result<T>(std::move(v)); }
    static Result<T> fail(int code, std::string msg) {
        return Result<T>(Error{code, std::move(msg)});
    }

    bool isOk() const { return std::holds_alternative<T>(m_value); }
    bool isError() const { return std::holds_alternative<Error>(m_value); }
    explicit operator bool() const { return isOk(); }

    const T& value() const& { return std::get<T>(m_value); }
    T& value() & { return std::get<T>(m_value); }
    T&& value() && { return std::move(std::get<T>(m_value)); }

    const Error& error() const { return std::get<Error>(m_value); }

private:
    std::variant<T, Error> m_value;
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(Error err) : m_error(std::move(err)) {}

    static Result<void> ok() { return Result<void>(); }
    static Result<void> fail(int code, std::string msg) {
        return Result<void>(Error{code, std::move(msg)});
    }

    bool isOk() const { return !m_error.has_value(); }
    bool isError() const { return m_error.has_value(); }
    explicit operator bool() const { return isOk(); }

    const Error& error() const { return *m_error; }

private:
    std::optional<Error> m_error;
};

}  // namespace dante::core

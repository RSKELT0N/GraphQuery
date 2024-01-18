#pragma once

#include <functional>

template<typename... T>
class CStatement
{
  public:
    CStatement(void (*func)(const T &...)) { this->process = func; }

    ~CStatement()                                  = default;
    CStatement(const CStatement &)                 = delete;
    CStatement & operator=(CStatement &&) noexcept = delete;
    CStatement & operator=(const CStatement &)     = delete;

  private:
    std::function<void(const T &...)> process;
};
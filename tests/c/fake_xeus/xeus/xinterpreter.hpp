#ifndef CANVAS_HOST_TEST_XEUS_INTERPRETER_HPP
#define CANVAS_HOST_TEST_XEUS_INTERPRETER_HPP

// Minimal xeus interpreter used to capture display_html() output in host tests.
// The implementation lives in fakes.cpp.

#include <nlohmann/json.hpp>

namespace xeus
{

class xinterpreter
{
public:
    void display_data(nlohmann::json data, nlohmann::json metadata, nlohmann::json transient);
};

xinterpreter& get_interpreter();

} // namespace xeus

#endif /* CANVAS_HOST_TEST_XEUS_INTERPRETER_HPP */

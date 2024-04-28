#pragma once

#include "formula.hpp"
#include <vector>

namespace geyser
{

// TODO: Do we need to store vars/primed vars?
class transition_system
{
    cnf_formula _init;
    cnf_formula _trans;
    cnf_formula _prop;

public:
    transition_system( cnf_formula init, cnf_formula trans, cnf_formula prop )
    : _init{ std::move( init ) }, _trans{ std::move( trans ) }, _prop{ std::move( prop ) } {}
};

}
#pragma once

#include <iostream>

namespace cpppromise {

struct Empty {};

std::ostream &operator<<(std::ostream &os, const Empty &e);

}  // namespace cpppromise

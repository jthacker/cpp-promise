#include "empty.h"

namespace cpppromise {

inline std::ostream &operator<<(std::ostream &os, const Empty &e) {
  os << "Empty";
  return os;
}

}  // namespace cpppromise

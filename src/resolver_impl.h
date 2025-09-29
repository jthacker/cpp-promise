#pragma once

#include "resolver.h"

namespace cpppromise {

template <typename T>
Resolver<T>::Resolver(std::shared_ptr<PromiseControlBlock<T>> pcb)
    : pcb_(pcb) {}

template <typename T>
void Resolver<T>::Resolve(T result) {
  pcb_->Resolve(result);
}

}  // namespace cpppromise

#pragma once

// abstract class for which user needs to provide concrete implementation for
// interested lifecycle event of promise
class PromiseListener {
 public:
  virtual void OnResolved() = 0;
};
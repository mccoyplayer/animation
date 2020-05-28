#pragma once

#include <string>
#include "Scheduler.h"

struct ErrorWrapper {
  std::string message = "";
  bool handled = false;
};

class ErrorHandler {
  public:
    void raise(const char *message) {
      std::string str = message;
      this->getScheduler()->scheduleOnUI([this, str]() mutable {
        this->raiseSpec(str.c_str());
      });
    }
    virtual std::shared_ptr<Scheduler> getScheduler() = 0;
    virtual std::shared_ptr<ErrorWrapper> getError() = 0;
    virtual void handleError() = 0;
    virtual ~ErrorHandler() {}
  protected:
    virtual void raiseSpec(const char *message) = 0;
};

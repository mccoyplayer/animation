#include "Mapper.h"
#include "SharedParent.h"
#include "MutableValue.h"

namespace reanimated {

Mapper::Mapper(NativeReanimatedModule *module,
               unsigned long id,
               jsi::Function &&mapper,
               std::vector<std::shared_ptr<MutableValue>> inputs,
               std::vector<std::shared_ptr<MutableValue>> outputs):
id(id),
module(module),
mapper(std::move(mapper)),
inputs(inputs),
outputs(outputs) {
  auto markDirty = [this, module]() {
    this->dirty = true;
    module->maybeRequestRender();
  };
  for (auto input : inputs) {
    input->addListener(markDirty);
  }
}

void Mapper::execute(jsi::Runtime &rt) {
  dirty = false;
  try {
    mapper.callWithThis(rt, mapper);
  }
  catch(...) {
    if (!module->errorHandler->raise()) {
      throw;
    }
  }
}

}

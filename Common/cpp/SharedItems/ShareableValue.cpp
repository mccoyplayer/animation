#include "ShareableValue.h"
#include "SharedParent.h"
#include "NativeReanimatedModule.h"
#include "Logger.h"
#include "MutableValue.h"
#include "MutableValueSetterProxy.h"
#include "RemoteObject.h"
#include "FrozenObject.h"

namespace reanimated {

const char *HIDDEN_HOST_OBJECT_PROP = "__reanimatedHostObjectRef";
const char *ALREADY_CONVERTED= "__alreadyConverted";
std::string CALLBACK_ERROR_SUFFIX = R"(

Possible solutions are:
a) If you want to synchronously execute this method, mark it as a Worklet
b) If you want to execute this method on the JS thread, wrap it using runOnJS )";
  
void addHiddenProperty(jsi::Runtime &rt,
                       jsi::Value &&value,
                       jsi::Object &obj,
                       const char *name) {
  jsi::Object globalObject = rt.global().getPropertyAsObject(rt, "Object");
  jsi::Function defineProperty = globalObject.getPropertyAsFunction(rt, "defineProperty");
  jsi::String internalPropName = jsi::String::createFromUtf8(rt, name);
  jsi::Object paramForDefineProperty(rt);
  paramForDefineProperty.setProperty(rt, "enumerable", false);
  paramForDefineProperty.setProperty(rt, "value", value);
  defineProperty.call(rt, obj, internalPropName, paramForDefineProperty);
}

void freeze(jsi::Runtime &rt, jsi::Object &obj) {
  jsi::Object globalObject = rt.global().getPropertyAsObject(rt, "Object");
  jsi::Function freeze = globalObject.getPropertyAsFunction(rt, "freeze");
  freeze.call(rt, obj);
}

void ShareableValue::adaptCache(jsi::Runtime &rt, const jsi::Value &value) {
  // when adapting from host object we can assign cached value immediately such that we avoid
  // running `toJSValue` in the future when given object is accessed
  if (module->isUIRuntime(rt)) {
    if (remoteValue.expired()) {
      remoteValue = getWeakRef(rt);
    }
    (*remoteValue.lock()) = jsi::Value(rt, value);
  } else {
    hostValue = std::make_unique<jsi::Value>(rt, value);
  }
}

void ShareableValue::adapt(jsi::Runtime &rt, const jsi::Value &value, ValueType objectType) {
  bool isRNRuntime = !(module->isUIRuntime(rt));
  if (value.isObject()) {
    jsi::Object object = value.asObject(rt);
    jsi::Value hiddenValue = object.getProperty(rt, HIDDEN_HOST_OBJECT_PROP);
    if (!(hiddenValue.isUndefined())) {
      jsi::Object hiddenProperty = hiddenValue.asObject(rt);
      if (hiddenProperty.isHostObject<FrozenObject>(rt)) {
        type = ValueType::ObjectType;
        if (object.hasProperty(rt, "__worklet") && object.isFunction(rt)) {
          type = ValueType::WorkletFunctionType;
        }
        valueContainer = std::make_unique<FrozenObjectWrapper>(
          hiddenProperty.getHostObject<FrozenObject>(rt)
        );
        if (object.hasProperty(rt, ALREADY_CONVERTED)) {
          adaptCache(rt, value);
        }
        return;
      }
    }
  }
  
  if (objectType == ValueType::MutableValueType) {
    type = ValueType::MutableValueType;
    valueContainer = std::make_unique<MutableValueWrapper>(
      std::make_shared<MutableValue>(rt, value, module, module->scheduler)
    );
  } else if (value.isUndefined()) {
    type = ValueType::UndefinedType;
  } else if (value.isNull()) {
    type = ValueType::NullType;
  } else if (value.isBool()) {
    type = ValueType::BoolType;
    valueContainer = std::make_unique<BooleanValueWrapper>(value.getBool());
  } else if (value.isNumber()) {
    type = ValueType::NumberType;
    valueContainer = std::make_unique<NumberValueWrapper>(value.asNumber());
  } else if (value.isString()) {
    type = ValueType::StringType;
    valueContainer = std::make_unique<StringValueWrapper>(value.asString(rt).utf8(rt));
  } else if (value.isObject()) {
    auto object = value.asObject(rt);
    if (object.isFunction(rt)) {
      if (object.getProperty(rt, "__worklet").isUndefined()) {
        // not a worklet, we treat this as a host function
        type = ValueType::HostFunctionType;
        containsHostFunction = true;
        valueContainer = std::make_unique<HostFunctionWrapper>(
          std::make_shared<HostFunctionHandler>(std::make_shared<jsi::Function>(object.asFunction(rt)), rt),
          &rt
        );
      } else {
        // a worklet
        type = ValueType::WorkletFunctionType;
        valueContainer = std::make_unique<FrozenObjectWrapper>(std::make_shared<FrozenObject>(rt, object, module));
        auto& frozenObject = ValueWrapper::asFrozenObject(valueContainer);
        containsHostFunction |= frozenObject->containsHostFunction;
        if (isRNRuntime && !containsHostFunction) {
          addHiddenProperty(rt, createHost(rt, frozenObject), object, HIDDEN_HOST_OBJECT_PROP);
        }
      }
    } else if (object.isArray(rt)) {
      type = ValueType::ArrayType;
      auto array = object.asArray(rt);
      valueContainer = std::make_unique<FrozenArrayWrapper>();
      auto& frozenArray = ValueWrapper::asFrozenArray(valueContainer);
      for (size_t i = 0, size = array.size(rt); i < size; i++) {
        auto sv = adapt(rt, array.getValueAtIndex(rt, i), module);
        containsHostFunction |= sv->containsHostFunction;
        frozenArray.push_back(sv);
      }
    } else if (object.isHostObject<MutableValue>(rt)) {
      type = ValueType::MutableValueType;
      valueContainer = std::make_unique<MutableValueWrapper>(object.getHostObject<MutableValue>(rt));
      adaptCache(rt, value);
    } else if (object.isHostObject<RemoteObject>(rt)) {
      type = ValueType::RemoteObjectType;
      valueContainer = std::make_unique<RemoteObjectWrapper>(
         object.getHostObject<RemoteObject>(rt)
      );
      adaptCache(rt, value);
    } else if (objectType == ValueType::RemoteObjectType) {
      type = ValueType::RemoteObjectType;
      valueContainer = std::make_unique<RemoteObjectWrapper>(
        std::make_shared<RemoteObject>(rt, object, module, module->scheduler)
      );
    } else {
      // create frozen object based on a copy of a given object
      type = ValueType::ObjectType;
      valueContainer = std::make_unique<FrozenObjectWrapper>(
        std::make_shared<FrozenObject>(rt, object, module)
      );
      auto& frozenObject = ValueWrapper::asFrozenObject(valueContainer);
      containsHostFunction |= frozenObject->containsHostFunction;
      if (isRNRuntime) {
        if (!containsHostFunction) {
          addHiddenProperty(rt, createHost(rt, frozenObject), object, HIDDEN_HOST_OBJECT_PROP);
        }
        freeze(rt, object);
      }
    }
  } else if (value.isSymbol()) {
    type = ValueType::StringType;
    valueContainer = std::make_unique<StringValueWrapper>(value.asSymbol(rt).toString(rt));
  } else {
    throw "Invalid value type";
  }
}

std::shared_ptr<ShareableValue> ShareableValue::adapt(jsi::Runtime &rt, const jsi::Value &value, NativeReanimatedModule *module, ValueType valueType) {
  auto sv = std::shared_ptr<ShareableValue>(new ShareableValue(module, module->scheduler));
  sv->adapt(rt, value, valueType);
  return sv;
}

jsi::Value ShareableValue::getValue(jsi::Runtime &rt) {
  // TODO: maybe we can cache toJSValue results on a per-runtime basis, need to avoid ref loops
  if (module->isUIRuntime(rt)) {
    if (remoteValue.expired()) {
      auto ref = getWeakRef(rt);
      remoteValue = ref;
    }
    
    if (remoteValue.lock()->isUndefined()) {
      (*remoteValue.lock()) = jsi::Value(rt, toJSValue(rt));
    }
    return jsi::Value(rt, *remoteValue.lock());
  } else {
    if (hostValue.get() == nullptr) {
      hostValue = std::make_unique<jsi::Value>(rt, toJSValue(rt));
    }
    return jsi::Value(rt, *hostValue);
  }
}

jsi::Object ShareableValue::createHost(jsi::Runtime &rt, std::shared_ptr<jsi::HostObject> host) {
  return jsi::Object::createFromHostObject(rt, host);
}

jsi::Value createFrozenWrapper(jsi::Runtime &rt, std::shared_ptr<FrozenObject> frozenObject) {
  jsi::Object __reanimatedHiddenHost = jsi::Object::createFromHostObject(rt, frozenObject);
  jsi::Object obj = frozenObject->shallowClone(rt);
  jsi::Object globalObject = rt.global().getPropertyAsObject(rt, "Object");
  jsi::Function freeze = globalObject.getPropertyAsFunction(rt, "freeze");
  if (!frozenObject->containsHostFunction) {
    addHiddenProperty(rt, std::move(__reanimatedHiddenHost), obj, HIDDEN_HOST_OBJECT_PROP);
    addHiddenProperty(rt, true, obj, ALREADY_CONVERTED);
  }
  return freeze.call(rt, obj);
}

jsi::Value ShareableValue::toJSValue(jsi::Runtime &rt) {
  switch (type) {
    case ValueType::UndefinedType:
      return jsi::Value::undefined();
    case ValueType::NullType:
      return jsi::Value::null();
    case ValueType::BoolType:
      return jsi::Value(ValueWrapper::asBoolean(valueContainer));
    case ValueType::NumberType:
      return jsi::Value(ValueWrapper::asNumber(valueContainer));
    case ValueType::StringType: {
      auto& stringValue = ValueWrapper::asString(valueContainer);
      return jsi::Value(rt, jsi::String::createFromAscii(rt, stringValue));
    }
    case ValueType::ObjectType: {
      auto& frozenObject = ValueWrapper::asFrozenObject(valueContainer);
      return createFrozenWrapper(rt, frozenObject);
    }
    case ValueType::ArrayType: {
      auto& frozenArray = ValueWrapper::asFrozenArray(valueContainer);
      jsi::Array array(rt, frozenArray.size());
      for (size_t i = 0; i < frozenArray.size(); i++) {
        array.setValueAtIndex(rt, i, frozenArray[i]->toJSValue(rt));
      }
      return array;
    }
    case ValueType::RemoteObjectType: {
      auto& remoteObject = ValueWrapper::asRemoteObject(valueContainer);
      if (module->isUIRuntime(rt)) {
        remoteObject->maybeInitializeOnUIRuntime(rt);
      }
      return createHost(rt, remoteObject);
    }
    case ValueType::MutableValueType: {
      auto& mutableObject = ValueWrapper::asMutableValue(valueContainer);
      return createHost(rt, mutableObject);
    }
    case ValueType::HostFunctionType: {
      auto hostFunctionWrapper = ValueWrapper::asHostFunctionWrapper(valueContainer);
      if (hostFunctionWrapper->hostRuntime == &rt) {
        // function is accessed from the same runtime it was crated, we just return same function obj
        return jsi::Value(rt, *hostFunctionWrapper->value->get());
      } else {
        // function is accessed from a different runtime, we wrap function in host func that'd enqueue
        // call on an appropriate thread
        
        auto module = this->module;
        auto hostFunction = hostFunctionWrapper->value;
        
        auto warnFunction = [module, hostFunction](
            jsi::Runtime &rt,
            const jsi::Value &thisValue,
            const jsi::Value *args,
            size_t count
            ) -> jsi::Value {
            
          jsi::Value jsThis = rt.global().getProperty(rt, "jsThis");
          std::string workletLocation = jsThis.asObject(rt).getProperty(rt, "__location").toString(rt).utf8(rt);
          std::string exceptionMessage = "Tried to synchronously call ";
          if(hostFunction->functionName.empty()) {
            exceptionMessage += "anonymous function";
          } else {
            exceptionMessage += "function {" + hostFunction->functionName + "}";
          }
          exceptionMessage += " from a different thread.\n\nOccurred in worklet location: ";
          exceptionMessage += workletLocation;
          exceptionMessage += CALLBACK_ERROR_SUFFIX;
          module->errorHandler->setError(exceptionMessage);
          module->errorHandler->raise();
          
          return jsi::Value::undefined();
        };
        
        auto hostRuntime = hostFunctionWrapper->hostRuntime;
        auto clb = [module, hostFunction, hostRuntime](
            jsi::Runtime &rt,
            const jsi::Value &thisValue,
            const jsi::Value *args,
            size_t count
            ) -> jsi::Value {
          // TODO: we should find thread based on runtime such that we could also call UI methods
          // from RN and not only RN methods from UI
          
          std::vector<std::shared_ptr<ShareableValue>> params;
          for (int i = 0; i < count; ++i) {
            params.push_back(ShareableValue::adapt(rt, args[i], module));
          }
          
          std::function<void()> job = [hostFunction, hostRuntime, params] {
            jsi::Value * args = new jsi::Value[params.size()];
            for (int i = 0; i < params.size(); ++i) {
              args[i] = params[i]->getValue(*hostRuntime);
            }
             
            jsi::Value returnedValue = hostFunction->get()->call(*hostRuntime,
                                                static_cast<const jsi::Value*>(args),
                                                (size_t)params.size());
             
            delete [] args;
            // ToDo use returned value to return promise
          };
          
          module->scheduler->scheduleOnJS(job);
          return jsi::Value::undefined();
        };
        jsi::Function wrapperFunction = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, "hostFunction"), 0, warnFunction);
        jsi::Function res = jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, "hostFunction"), 0, clb);
        addHiddenProperty(rt, std::move(res), wrapperFunction, "__callAsync");
        return wrapperFunction;
      }
    }
    case ValueType::WorkletFunctionType:
      auto module = this->module;
      auto& frozenObject = ValueWrapper::asFrozenObject(this->valueContainer);
      if (module->isUIRuntime(rt)) {
        // when running on UI thread we prep a function

        auto jsThis = std::make_shared<jsi::Object>(frozenObject->shallowClone(*module->runtime));
        std::shared_ptr<jsi::Function> funPtr(module->workletsCache->getFunction(rt, frozenObject));
        auto name = funPtr->getProperty(rt, "name").asString(rt).utf8(rt);

        auto clb = [=](
                   jsi::Runtime &rt,
                   const jsi::Value &thisValue,
                   const jsi::Value *args,
                   size_t count
                   ) mutable -> jsi::Value {
           jsi::Value oldJSThis = rt.global().getProperty(rt, "jsThis");
           rt.global().setProperty(rt, "jsThis", *jsThis); //set jsThis

           jsi::Value res = jsi::Value::undefined();
           try {
             if (thisValue.isObject()) {
               res = funPtr->callWithThis(rt, thisValue.asObject(rt), args, count);
             } else {
               res = funPtr->call(rt, args, count);
             }
           } catch(jsi::JSError &e) {
              throw e;
           } catch(...) {
              // TODO find out a way to get the error's message on hermes
              jsi::Value location = jsThis->getProperty(rt, "__location");
              std::string str = "Javascript worklet error";
              if (location.isString()) {
                str += "\nIn file: " + location.asString(rt).utf8(rt);
              }
              module->errorHandler->setError(str);
              module->errorHandler->raise();
            }

           rt.global().setProperty(rt, "jsThis", oldJSThis); //clean jsThis
           return res;
        };
        return jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, name.c_str()), 0, clb);
      } else {
        // when run outside of UI thread we enqueue a call on the UI thread
        auto clb = [=](
            jsi::Runtime &rt,
            const jsi::Value &thisValue,
            const jsi::Value *args,
            size_t count
            ) -> jsi::Value {
          // TODO: we should find thread based on runtime such that we could also call UI methods
          // from RN and not only RN methods from UI

          std::vector<std::shared_ptr<ShareableValue>> params;
          for (int i = 0; i < count; ++i) {
            params.push_back(ShareableValue::adapt(rt, args[i], module));
          }
          
          module->scheduler->scheduleOnUI([=] {
            jsi::Runtime &rt = *module->runtime.get();
            auto jsThis = createFrozenWrapper(rt, frozenObject).getObject(rt);
            auto code = jsThis.getProperty(rt, "asString").asString(rt).utf8(rt);
            std::shared_ptr<jsi::Function> funPtr(module->workletsCache->getFunction(rt, frozenObject));
            
            jsi::Value * args = new jsi::Value[params.size()];
            for (int i = 0; i < params.size(); ++i) {
              args[i] = params[i]->getValue(rt);
            }
            
            jsi::Value returnedValue;
            
            jsi::Value oldJSThis = rt.global().getProperty(rt, "jsThis");
            rt.global().setProperty(rt, "jsThis", jsThis); //set jsThis
            try {
              returnedValue = funPtr->call(rt,
                                             static_cast<const jsi::Value*>(args),
                                             (size_t)params.size());
            
            } catch(std::exception &e) {
              std::string str = e.what();
              module->errorHandler->setError(str);
              module->errorHandler->raise();
            } catch(...) {
              // TODO find out a way to get the error's message on hermes
              jsi::Value location = jsThis.getProperty(rt, "__location");
              std::string str = "Javascript worklet error";
              if (location.isString()) {
                str += "\nIn file: " + location.asString(rt).utf8(rt);
              }
              module->errorHandler->setError(str);
              module->errorHandler->raise();
            }
            rt.global().setProperty(rt, "jsThis", oldJSThis); //clean jsThis
            
            delete [] args;
            // ToDo use returned value to return promise
          });
          return jsi::Value::undefined();
        };
        return jsi::Function::createFromHostFunction(rt, jsi::PropNameID::forAscii(rt, "_workletFunction"), 0, clb);
      }
  }
  throw "convert error";
}

}

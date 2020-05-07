#pragma once

#include <map>
#include <string>
#include <limits>
#include <vector>
#include <utility>
#include <variant>
#include <exception>
#include <functional>
#include "json.hpp"

namespace jsonrpccxx {
  typedef nlohmann::json json;
  typedef std::vector<std::string> NamedParamMapping;
  static NamedParamMapping NAMED_PARAM_MAPPING;

  typedef std::vector<json> Parameters;
  typedef std::function<json(const Parameters &)> MethodHandle;
  typedef std::function<void(const Parameters &)> NotificationHandle;

  enum class version { v1, v2 };
  typedef std::vector<json> positional_parameter;
  typedef std::map<std::string, json> named_parameter;
  typedef std::variant<int, std::string> id_type;

  static inline bool has_key(const json &v, const std::string &key) { return v.find(key) != v.end(); }
  static inline bool has_key_type(const json &v, const std::string &key, json::value_t type) { return has_key(v, key) && v.at(key).type() == type; }
  static inline bool valid_id(const json &request) {
    return has_key(request, "id") && (request["id"].is_number() || request["id"].is_string() || request["id"].is_null());
  }
  static inline bool valid_id_not_null(const json &request) { return has_key(request, "id") && (request["id"].is_number() || request["id"].is_string()); }

  class JsonRpcException : public std::exception {
  public:
    JsonRpcException(int code, const std::string &message) noexcept : code(code), message(message), data(nullptr), err(std::to_string(code) + ": " + message) {}
    JsonRpcException(int code, const std::string &message, const json &data) noexcept
        : code(code), message(message), data(data), err(std::to_string(code) + ": " + message + ", data: " + data.dump()) {}

    int Code() const { return code; }
    const std::string &Message() const { return message; }
    const json &Data() const { return data; }

    const char *what() const noexcept override { return err.c_str(); }

    static inline JsonRpcException fromJson(const json &value) {
      bool has_code = has_key_type(value, "code", json::value_t::number_integer);
      bool has_message = has_key_type(value, "message", json::value_t::string);
      bool has_data = has_key(value, "data");
      if (has_code && has_message) {
        if (has_data) {
          return JsonRpcException(value["code"], value["message"], value["data"].get<json>());
        } else {
          return JsonRpcException(value["code"], value["message"]);
        }
      }
      return JsonRpcException(-32603, R"(invalid error response: "code" (negative number) and "message" (string) are required)");
    }

  private:
    int code;
    std::string message;
    json data;
    std::string err;
  };
  
  struct JsonRpcResponse {
    id_type id;
    json result;
  };

  class IClientConnector {
  public:
      virtual ~IClientConnector() = default;
      virtual std::string Send(const std::string &request) = 0;
  };

  class JsonRpcClient {
  public:
    JsonRpcClient(IClientConnector &connector, version v) : connector(connector), v(v) {}
    virtual ~JsonRpcClient() = default;

    template <typename T>
    T CallMethod(const id_type &id, const std::string &name, const positional_parameter &params = {}) { return call_method(id, name, params).result.get<T>(); }
    template <typename T>
    T CallMethodNamed(const id_type &id, const std::string &name, const named_parameter &params = {}) { return call_method(id, name, params).result.get<T>(); }

    void CallNotification(const std::string &name, const positional_parameter &params = {}) { call_notification(name, params); }
    void CallNotificationNamed(const std::string &name, const named_parameter &params = {}) { call_notification(name, params); }

  protected:
    IClientConnector &connector;

  private:
    version v;

    JsonRpcResponse call_method(const id_type &id, const std::string &name, const json &params) {
      json j = {{"method", name}};
      if (std::get_if<int>(&id) != nullptr) {
        j["id"] = std::get<int>(id);
      } else {
        j["id"] = std::get<std::string>(id);
      }
      if (v == version::v2) {
        j["jsonrpc"] = "2.0";
      }
      if (!params.empty() && !params.is_null()) {
        j["params"] = params;
      } else if (v == version::v1) {
        j["params"] = nullptr;
      }
      try {
        json response = json::parse(connector.Send(j.dump()));
        if (has_key_type(response, "error", json::value_t::object)) {
          throw JsonRpcException::fromJson(response["error"]);
        }
        if (has_key(response, "result") && has_key(response, "id")) {
          if (response["id"].type() == json::value_t::string)
            return JsonRpcResponse{response["id"].get<std::string>(), response["result"].get<json>()};
          else
            return JsonRpcResponse{response["id"].get<int>(), response["result"].get<json>()};
        }
        throw JsonRpcException(-32603, R"(invalid server response: neither "result" nor "error" fields found)");
      } catch (json::parse_error &e) {
        throw JsonRpcException(-32700, std::string("invalid JSON response from server: ") + e.what());
      }
    }

    void call_notification(const std::string &name, const nlohmann::json &params) {
      nlohmann::json j = {{"method", name}};
      if (v == version::v2) {
        j["jsonrpc"] = "2.0";
      } else {
        j["id"] = nullptr;
      }
      if (!params.empty() && !params.is_null()) {
        j["params"] = params;
      } else if (v == version::v1) {
        j["params"] = nullptr;
      }
      connector.Send(j.dump());
    }
  };

  class BatchRequest {
  public:
    BatchRequest() : call(json::array()) {}
    BatchRequest &AddMethodCall(const id_type &id, const std::string &name, const positional_parameter &params = {}) {
      json request = {{"method", name}, {"params", params}, {"jsonrpc", "2.0"}};
      if (std::get_if<int>(&id) != nullptr) {
        request["id"] = std::get<int>(id);
      } else {
        request["id"] = std::get<std::string>(id);
      }
      call.push_back(request);
      return *this;
    }

    BatchRequest &AddNamedMethodCall(const id_type &id, const std::string &name, const named_parameter &params = {}) {
      json request = {{"method", name}, {"params", params}, {"jsonrpc", "2.0"}};
      if (std::get_if<int>(&id) != nullptr) {
        request["id"] = std::get<int>(id);
      } else {
        request["id"] = std::get<std::string>(id);
      }
      call.push_back(request);
      return *this;
    }

    BatchRequest &AddNotificationCall(const std::string &name, const positional_parameter &params = {}) {
      call.push_back({{"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
      return *this;
    }

    BatchRequest &AddNamedNotificationCall(const std::string &name, const named_parameter &params = {}) {
      call.push_back({{"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
      return *this;
    }

    const json &Build() const { return call; }

  private:
    json call;
  };

  class BatchResponse {
  public:
    explicit BatchResponse(json &&response) : response(response) {
      for (auto &[key, value] : response.items()) {
        if (value.is_object() && valid_id_not_null(value) && has_key(value, "result")) {
          results[value["id"]] = std::stoi(key);
        } else if (value.is_object() && valid_id_not_null(value) && has_key(value, "error")) {
          errors[value["id"]] = std::stoi(key);
        } else {
          nullIds.push_back(std::stoi(key));
        }
      }
    }

    template <typename T>
    T Get(const json &id) {
      if (results.find(id) != results.end()) {
        try {
          return response[results[id]]["result"].get<T>();
        } catch (json::type_error &e) {
          throw JsonRpcException(-32700, "invalid return type: " + std::string(e.what()));
        }
      } else if (errors.find(id) != errors.end()) {
        throw JsonRpcException::fromJson(response[errors[id]]["error"]);
      }
      throw JsonRpcException(-32700, std::string("no result found for id ") + id.dump());
    }

    bool HasErrors() { return !errors.empty() || !nullIds.empty(); }
    const std::vector<size_t> GetInvalidIndexes() { return nullIds; }
    const json& GetResponse() { return response; }

  private:
    json response;
    std::map<json, size_t> results;
    std::map<json, size_t> errors;
    std::vector<size_t> nullIds;
  };

  class BatchClient : public JsonRpcClient {
  public:
    explicit BatchClient(IClientConnector &connector) : JsonRpcClient(connector, version::v2) {}
    BatchResponse BatchCall(const BatchRequest &request) {
      try {
        json response = json::parse(connector.Send(request.Build().dump()));
        if (!response.is_array()) {
          throw JsonRpcException(-32700, std::string("invalid JSON response from server: expected array"));
        }
        return BatchResponse(std::move(response));
      } catch (json::parse_error &e) {
        throw JsonRpcException(-32700, std::string("invalid JSON response from server: ") + e.what());
      }
    }
  };

  // Workaround due to forbidden partial template function specialisation
  template <typename T>
  struct type {};

  template <typename T>
  constexpr json::value_t GetType(type<std::vector<T>>) {
    return json::value_t::array;
  }
  template <typename T>
  constexpr json::value_t GetType(type<T>) {
    return json::value_t::object;
  }
  constexpr json::value_t GetType(type<std::string>) { return json::value_t::string; }
  constexpr json::value_t GetType(type<bool>) { return json::value_t::boolean; }
  constexpr json::value_t GetType(type<float>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<double>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<long double>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<short>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<int>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<long>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<unsigned short>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<unsigned int>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<unsigned long>) { return json::value_t::number_unsigned; }

  inline std::string type_name(json::value_t t) {
    switch (t) {
    case json::value_t::number_integer:
      return "integer";
    case json::value_t::boolean:
      return "boolean";
    case json::value_t::number_float:
      return "float";
    case json::value_t::number_unsigned:
      return "unsigned integer";
    case json::value_t::object:
      return "object";
    case json::value_t::array:
      return "array";
    case json::value_t::string:
      return "string";
    default:
      return "null";
    }
  }

  template <typename T>
  inline void check_param_type(size_t index, const json &x, json::value_t expectedType, typename std::enable_if<std::is_arithmetic<T>::value>::type * = 0) {
    if (expectedType == json::value_t::number_unsigned && x.type() == json::value_t::number_integer) {
      if (x.get<long long int>() < 0)
        throw JsonRpcException(-32602, "invalid parameter: must be " + type_name(expectedType) + ", but is " + type_name(x.type()), index);
    } else if (x.type() == json::value_t::number_unsigned && expectedType == json::value_t::number_integer) {
      if (x.get<long long unsigned>() > std::numeric_limits<T>::max()) {
        throw JsonRpcException(-32602, "invalid parameter: exceeds value range of " + type_name(expectedType), index);
      }
    } else if (x.type() != expectedType) {
      throw JsonRpcException(-32602, "invalid parameter: must be " + type_name(expectedType) + ", but is " + type_name(x.type()), index);
    }
  }

  template <typename T>
  inline void check_param_type(size_t index, const json &x, json::value_t expectedType, typename std::enable_if<!std::is_arithmetic<T>::value>::type * = 0) {
    if (x.type() != expectedType) {
      throw JsonRpcException(-32602, "invalid parameter: must be " + type_name(expectedType) + ", but is " + type_name(x.type()), index);
    }
  }

  //
  // Mapping for methods
  //
  template <typename ReturnType, typename... ParamTypes, std::size_t... index>
  MethodHandle createMethodHandle(std::function<ReturnType(ParamTypes...)> method, std::index_sequence<index...>) {
    MethodHandle handle = [method](const Parameters &params) -> json {
      size_t actualSize = params.size();
      size_t formalSize = sizeof...(ParamTypes);
      // TODO: add lenient mode for backwards compatible additional params
      if (actualSize != formalSize) {
        throw JsonRpcException(-32602, "invalid parameter: expected " + std::to_string(formalSize) + " argument(s), but found " + std::to_string(actualSize));
      }
      (check_param_type<typename std::decay<ParamTypes>::type>(index, params[index], GetType(type<typename std::decay<ParamTypes>::type>())), ...);
      return method(params[index].get<typename std::decay<ParamTypes>::type>()...);
    };
    return handle;
  }

  template <typename ReturnType, typename... ParamTypes>
  MethodHandle methodHandle(std::function<ReturnType(ParamTypes...)> method) {
    return createMethodHandle(method, std::index_sequence_for<ParamTypes...>{});
  }

  template <typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(std::function<ReturnType(ParamTypes...)> f) {
    return methodHandle(f);
  }
  // Mapping for c-style function pointers
  template <typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(ReturnType (*f)(ParamTypes...)) {
    return GetHandle(std::function<ReturnType(ParamTypes...)>(f));
  }

  //
  // Notification mapping
  //
  template <typename... ParamTypes, std::size_t... index>
  NotificationHandle createNotificationHandle(std::function<void(ParamTypes...)> method, std::index_sequence<index...>) {
    NotificationHandle handle = [method](const Parameters &params) -> void {
      size_t actualSize = params.size();
      size_t formalSize = sizeof...(ParamTypes);
      // TODO: add lenient mode for backwards compatible additional params
      // if ((!allow_unkown_params && actualSize != formalSize) || (allow_unkown_params && actualSize < formalSize)) {
      if (actualSize != formalSize) {
        throw JsonRpcException(-32602, "invalid parameter: expected " + std::to_string(formalSize) + " argument(s), but found " + std::to_string(actualSize));
      }
      (check_param_type<typename std::decay<ParamTypes>::type>(index, params[index], GetType(type<typename std::decay<ParamTypes>::type>())), ...);
      method(params[index].get<typename std::decay<ParamTypes>::type>()...);
    };
    return handle;
  }

  template <typename... ParamTypes>
  NotificationHandle notificationHandle(std::function<void(ParamTypes...)> method) {
    return createNotificationHandle(method, std::index_sequence_for<ParamTypes...>{});
  }

  template <typename... ParamTypes>
  NotificationHandle GetHandle(std::function<void(ParamTypes...)> f) {
    return notificationHandle(f);
  }

  template <typename... ParamTypes>
  NotificationHandle GetHandle(void (*f)(ParamTypes...)) {
    return GetHandle(std::function<void(ParamTypes...)>(f));
  }

  //
  // Mapping for classes
  //
  template <typename T, typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(ReturnType (T::*method)(ParamTypes...), T &instance) {
    std::function<ReturnType(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> ReturnType {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return GetHandle(function);
  }

  template <typename T, typename... ParamTypes>
  NotificationHandle GetHandle(void (T::*method)(ParamTypes...), T &instance) {
    std::function<void(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> void {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return GetHandle(function);
  }

  class Dispatcher {
  public:
    bool Add(const std::string &name, MethodHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (contains(name))
        return false;
      methods[name] = std::move(callback);
      if (!mapping.empty()) {
        this->mapping[name] = mapping;
      }
      return true;
    }

    bool Add(const std::string &name, NotificationHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (contains(name))
        return false;
      notifications[name] = std::move(callback);
      if (!mapping.empty()) {
        this->mapping[name] = mapping;
      }
      return true;
    }

    JsonRpcException process_type_error(const std::string &name, JsonRpcException &e) {
      if (e.Code() == -32602 && !e.Data().empty()) {
        std::string message = e.Message() + " for parameter ";
        if (this->mapping.find(name) != this->mapping.end()) {
          message += "\"" + mapping[name][e.Data().get<unsigned int>()] + "\"";
        } else {
          message += std::to_string(e.Data().get<unsigned int>());
        }
        return JsonRpcException(e.Code(), message);
      }
      return e;
    }

    json InvokeMethod(const std::string &name, const json &params) {
      auto method = methods.find(name);
      if (method == methods.end()) {
        throw JsonRpcException(-32601, "method not found: " + name);
      }
      try {
        return method->second(normalize_parameter(name, params));
      } catch (json::type_error &e) {
        throw JsonRpcException(-32602, "invalid parameter: " + std::string(e.what()));
      } catch (JsonRpcException &e) {
        throw process_type_error(name, e);
      }
    }

    void InvokeNotification(const std::string &name, const json &params) {
      auto notification = notifications.find(name);
      if (notification == notifications.end()) {
        throw JsonRpcException(-32601, "notification not found: " + name);
      }
      try {
        notification->second(normalize_parameter(name, params));
      } catch (json::type_error &e) {
        throw JsonRpcException(-32602, "invalid parameter: " + std::string(e.what()));
      } catch (JsonRpcException &e) {
        throw process_type_error(name, e);
      }
    }

  private:
    std::map<std::string, MethodHandle> methods;
    std::map<std::string, NotificationHandle> notifications;
    std::map<std::string, NamedParamMapping> mapping;

    inline bool contains(const std::string &name) { return (methods.find(name) != methods.end() || notifications.find(name) != notifications.end()); }
    inline json normalize_parameter(const std::string &name, const json &params) {
      if (params.type() == json::value_t::array) {
        return params;
      } else if (params.type() == json::value_t::object) {
        if (mapping.find(name) == mapping.end()) {
          throw JsonRpcException(-32602, "invalid parameter: procedure doesn't support named parameter");
        }
        json result;
        for (auto const &p : mapping[name]) {
          if (params.find(p) == params.end()) {
            throw JsonRpcException(-32602, "invalid parameter: missing named parameter \"" + p + "\"");
          }
          result.push_back(params[p]);
        }
        return result;
      }
      throw JsonRpcException(-32600, "invalid request: params field must be an array, object");
    }
  };

  class JsonRpcServer {
  public:
    virtual ~JsonRpcServer() = default;
    virtual std::string HandleRequest(const std::string &request) = 0;

    bool Add(const std::string &name, MethodHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (name.rfind("rpc.", 0) == 0)
        return false;
      return dispatcher.Add(name, callback, mapping);
    }
    bool Add(const std::string &name, NotificationHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (name.rfind("rpc.", 0) == 0)
        return false;
      return dispatcher.Add(name, callback, mapping);
    }

  protected:
    Dispatcher dispatcher;
  };

  class JsonRpc2Server : public JsonRpcServer {
  public:
    JsonRpc2Server() = default;
    ~JsonRpc2Server() override = default;

    std::string HandleRequest(const std::string &requestString) override {
      try {
        json request = json::parse(requestString);
        if (request.is_array()) {
          json result = json::array();
          for (json &r : request) {
            json res = this->HandleSingleRequest(r);
            if (!res.is_null()) {
              result.push_back(std::move(res));
            }
          }
          return result.dump();
        } else if (request.is_object()) {
          json res = HandleSingleRequest(request);
          if (!res.is_null()) {
            return res.dump();
          } else {
            return "";
          }
        } else {
          return json{{"id", nullptr}, {"error", {{"code", -32600}, {"message", "invalid request: expected array or object"}}}, {"jsonrpc", "2.0"}}.dump();
        }
      } catch (json::parse_error &e) {
        return json{{"id", nullptr}, {"error", {{"code", -32700}, {"message", std::string("parse error: ") + e.what()}}}, {"jsonrpc", "2.0"}}.dump();
      }
    }

  private:
    json HandleSingleRequest(json &request) {
      json id = nullptr;
      if (valid_id(request)) {
        id = request["id"];
      }
      try {
        return ProcessSingleRequest(request);
      } catch (JsonRpcException &e) {
        json error = {{"code", e.Code()}, {"message", e.Message()}};
        if (!e.Data().is_null()) {
          error["data"] = e.Data();
        }
        return json{{"id", id}, {"error", error}, {"jsonrpc", "2.0"}};
      } catch (std::exception &e) {
        return json{{"id", id}, {"error", {{"code", -32603}, {"message", std::string("internal server error: ") + e.what()}}}, {"jsonrpc", "2.0"}};
      } catch (...) {
        return json{{"id", id}, {"error", {{"code", -32603}, {"message", std::string("internal server error")}}}, {"jsonrpc", "2.0"}};
      }
    }

    json ProcessSingleRequest(json &request) {
      if (!has_key_type(request, "jsonrpc", json::value_t::string) || request["jsonrpc"] != "2.0") {
        throw JsonRpcException(-32600, R"(invalid request: missing jsonrpc field set to "2.0")");
      }
      if (!has_key_type(request, "method", json::value_t::string)) {
        throw JsonRpcException(-32600, "invalid request: method field must be a string");
      }
      if (has_key(request, "id") && !valid_id(request)) {
        throw JsonRpcException(-32600, "invalid request: id field must be a number, string or null");
      }
      if (has_key(request, "params") && !(request["params"].is_array() || request["params"].is_object() || request["params"].is_null())) {
        throw JsonRpcException(-32600, "invalid request: params field must be an array, object or null");
      }
      if (!has_key(request, "params") || has_key_type(request, "params", json::value_t::null)) {
        request["params"] = json::array();
      }
      if (!has_key(request, "id")) {
        try {
          dispatcher.InvokeNotification(request["method"], request["params"]);
          return json();
        } catch (std::exception &e) {
          return json{{"msg", e.what()}};
        }
      } else {
        return {{"jsonrpc", "2.0"}, {"id", request["id"]}, {"result", dispatcher.InvokeMethod(request["method"], request["params"])}};
      }
    }
  };
}
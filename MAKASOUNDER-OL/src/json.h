#pragma once

#include <map>
#include <string>
#include <vector>

#include "common.h"

namespace mk {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;

    bool IsObject() const { return type == Type::Object; }
    bool IsArray() const { return type == Type::Array; }
    bool IsString() const { return type == Type::String; }
    bool IsNumber() const { return type == Type::Number; }

    const JsonValue* Get(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        auto it = objectValue.find(key);
        if (it == objectValue.end()) return nullptr;
        return &it->second;
    }
    std::wstring String(const std::string& key) const {
        auto* v = Get(key);
        if (!v || v->type != Type::String) return {};
        return Utf8ToWide(v->stringValue);
    }
    std::string Raw(const std::string& key) const {
        auto* v = Get(key);
        if (!v || v->type != Type::String) return {};
        return v->stringValue;
    }
    double Number(const std::string& key, double fallback = 0.0) const {
        auto* v = Get(key);
        if (!v || v->type != Type::Number) return fallback;
        return v->numberValue;
    }
};

class JsonParser {
public:
    static JsonValue Parse(const std::string& text);

private:
    JsonParser(const std::string& text);
    JsonValue ParseValue();
    JsonValue ParseObject();
    JsonValue ParseArray();
    JsonValue ParseString();
    JsonValue ParseNumber();
    void SkipWhitespace();
    bool Consume(char c);
    char Peek() const;

    const std::string& text_;
    size_t pos_ = 0;
};

}  // namespace mk

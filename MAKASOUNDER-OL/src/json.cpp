#include "json.h"

#include "common.h"

#include <cctype>
#include <cstdlib>

namespace mk {

JsonParser::JsonParser(const std::string& text) : text_(text) {}

void JsonParser::SkipWhitespace() {
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                   text_[pos_] == '\r' || text_[pos_] == '\n'))
        ++pos_;
}

char JsonParser::Peek() const {
    return pos_ < text_.size() ? text_[pos_] : '\0';
}

bool JsonParser::Consume(char c) {
    if (pos_ < text_.size() && text_[pos_] == c) {
        ++pos_;
        return true;
    }
    return false;
}

JsonValue JsonParser::ParseString() {
    JsonValue value;
    value.type = JsonValue::Type::String;
    if (!Consume('"')) return value;
    std::string result;
    while (pos_ < text_.size()) {
        char c = text_[pos_++];
        if (c == '"') {
            value.stringValue = std::move(result);
            return value;
        }
        if (c == '\\' && pos_ < text_.size()) {
            char escaped = text_[pos_++];
            switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (pos_ + 4 <= text_.size()) {
                        char hex[5] = {};
                        for (int i = 0; i < 4; ++i) hex[i] = text_[pos_ + i];
                        pos_ += 4;
                        unsigned code = static_cast<unsigned>(strtoul(hex, nullptr, 16));
                        if (code < 0x80) {
                            result.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            result.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            result.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            result.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                    }
                    break;
                }
                default: result.push_back(escaped); break;
            }
        } else {
            result.push_back(c);
        }
    }
    return value;
}

JsonValue JsonParser::ParseNumber() {
    JsonValue value;
    value.type = JsonValue::Type::Number;
    size_t start = pos_;
    if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
    while (pos_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[pos_])) ||
                                   text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E' ||
                                   text_[pos_] == '+' || text_[pos_] == '-'))
        ++pos_;
    value.numberValue = std::strtod(text_.substr(start, pos_ - start).c_str(), nullptr);
    return value;
}

JsonValue JsonParser::ParseObject() {
    JsonValue value;
    value.type = JsonValue::Type::Object;
    if (!Consume('{')) return value;
    SkipWhitespace();
    if (Consume('}')) return value;
    for (;;) {
        SkipWhitespace();
        JsonValue key = ParseString();
        SkipWhitespace();
        Consume(':');
        JsonValue member = ParseValue();
        value.objectValue[key.stringValue] = std::move(member);
        SkipWhitespace();
        if (Consume(',')) continue;
        if (Consume('}')) break;
        SkipWhitespace();
        if (!Consume(',')) break;
    }
    return value;
}

JsonValue JsonParser::ParseArray() {
    JsonValue value;
    value.type = JsonValue::Type::Array;
    if (!Consume('[')) return value;
    SkipWhitespace();
    if (Consume(']')) return value;
    for (;;) {
        SkipWhitespace();
        value.arrayValue.push_back(ParseValue());
        SkipWhitespace();
        if (Consume(',')) continue;
        if (Consume(']')) break;
        SkipWhitespace();
        if (!Consume(',')) break;
    }
    return value;
}

JsonValue JsonParser::ParseValue() {
    SkipWhitespace();
    if (pos_ >= text_.size()) return JsonValue{};
    char c = Peek();
    if (c == '{') return ParseObject();
    if (c == '[') return ParseArray();
    if (c == '"') return ParseString();
    if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();
    if (text_.compare(pos_, 4, "true") == 0) {
        pos_ += 4;
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        v.boolValue = true;
        return v;
    }
    if (text_.compare(pos_, 5, "false") == 0) {
        pos_ += 5;
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        v.boolValue = false;
        return v;
    }
    if (text_.compare(pos_, 4, "null") == 0) {
        pos_ += 4;
        return JsonValue{};
    }
    return JsonValue{};
}

JsonValue JsonParser::Parse(const std::string& text) {
    JsonParser parser(text);
    return parser.ParseValue();
}

}  // namespace mk

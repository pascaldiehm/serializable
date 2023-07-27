#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <list>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace serializable {
class Serializable;

namespace detail {
using Address    = std::size_t;
using AddressMap = std::unordered_map<Address, Address>;

template <typename T> using Serializer   = std::string (*)(const T&);
template <typename T> using Deserializer = T (*)(const std::string&);

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept SerializableChild = std::is_base_of_v<Serializable, T>;

class Serialized {
  public:
    [[nodiscard]] std::string serialize() const;
    void deserialize(const std::string& data);

    void setObject(const std::string& name, Address address, unsigned int id);
    void addChild(const Serialized& serialized);
    void setPrimitive(const std::string& type, const std::string& name, const std::string& value);
    void setPointer(const std::string& name, Address address, unsigned int id);

    template <typename T>
    [[nodiscard]] bool checkType(const std::string& type, Serializer<T> serializer, Deserializer<T> deserializer) const;
    template <typename T> [[nodiscard]] T getValue(Deserializer<T> deserializer) const;
    std::optional<Serialized> getChild(const std::string& name);
    [[nodiscard]] bool isObject(unsigned int id) const;
    [[nodiscard]] bool isPointer(unsigned int id) const;

    [[nodiscard]] Address getAddress() const;
    bool setAddresses(AddressMap* map);

  private:
    unsigned int classId{};
    Address address{};
    std::string type, name, value;
    std::unordered_map<std::string, Serialized> children;

    static std::vector<std::string> parseChildrenData(const std::string& object);
};

inline std::string replaceAll(const std::string& str, const std::string& from, const std::string& to) {
    // Calculate matches
    std::size_t matches = 0;
    std::size_t pos     = 0;
    while((pos = str.find(from, pos)) != std::string::npos) {
        matches++;
        pos += from.length();
    }

    // Return original data if no replacement required
    if(matches == 0) return str;

    // Generate new string
    std::string replaced;
    replaced.reserve(str.size() + matches * (to.size() - from.size()));
    pos = 0;
    while(true) {
        const std::size_t match = str.find(from, pos);
        replaced.append(str.substr(pos, match - pos));

        if(match == std::string::npos) break;
        replaced.append(to);

        pos = match + from.size();
    }

    return replaced;
}

inline std::string createString(const std::initializer_list<std::string>& list) {
    // Calculate size
    std::size_t size = 0;
    for(const auto& s : list) size += s.size();

    // Create string
    std::string str;
    str.reserve(size);
    for(const auto& s : list) str.append(s);
    return str;
}

inline void appendString(std::string& str, const std::initializer_list<std::string>& list) {
    // Calculate size
    std::size_t size = str.size();
    for(const auto& s : list) size += s.size();

    // Extend string
    str.reserve(size);
    for(const auto& s : list) str.append(s);
}

inline std::string Serialized::serialize() const {
    // Construct object
    if(type == "OBJECT") {
        std::string data =
            createString({"OBJECT ", name, " = ", std::to_string(address), " (", std::to_string(classId), ") {\n"});

        for(const auto& [_, child] : children)
            appendString(data, {"\t", replaceAll(child.serialize(), "\n", "\n\t"), "\n"});

        return data.append("}");
    }

    // Construct primitive value
    if(type != "PTR") return createString({type, " ", name, " = ", value});

    // Construct pointer value
    return createString({type, " ", name, " = ", std::to_string(address), " (", std::to_string(classId), ")"});
}

inline void Serialized::deserialize(const std::string& data) {
    if(data.starts_with("OBJECT")) {
        // Parse data, REGEX: OBJECT (.+) = (\d+) (\(\d+\)) {
        const std::size_t bracket = data.find('\n') - 1;
        const std::string header  = data.substr(0, bracket - 1);

        const std::size_t equals  = header.find('=');
        const std::size_t opening = header.find('(');
        const std::size_t closing = header.find(')');
        const std::string name    = header.substr(7, equals - 8);
        const std::string address = header.substr(equals + 2, opening - equals - 3);
        const std::string id      = header.substr(opening + 1, closing - opening - 1);

        // Set as object
        setObject(name, std::stoul(address), std::stoul(id));

        // Parse children
        const auto childrenData = parseChildrenData(data.substr(bracket));
        for(const auto& childData : childrenData) {
            Serialized child;
            child.deserialize(childData);
            addChild(child);
        }
    } else if(data.starts_with("PTR")) {
        // Parse data, REGEX: PTR (.+) = (\d+) (\(\d+\))
        const std::size_t equals  = data.find('=');
        const std::size_t opening = data.find('(');
        const std::size_t closing = data.find(')');
        const std::string name    = data.substr(4, equals - 5);
        const std::string address = data.substr(equals + 2, opening - equals - 3);
        const std::string id      = data.substr(opening + 1, closing - opening - 1);

        // Set data
        setPointer(name, std::stoul(address), std::stoul(id));
    } else if(!data.empty()) {
        // Parse data, REGEX: ([A-Z]+) (.+) = (.+)
        const std::size_t space  = data.find(' ');
        const std::size_t equals = data.find('=');
        const std::string type   = data.substr(0, space);
        const std::string name   = data.substr(space + 1, equals - space - 2);
        const std::string value  = data.substr(equals + 2);

        // Set data
        setPrimitive(type, name, value);
    }
}

inline void Serialized::setObject(const std::string& name, Address address, unsigned int id) {
    this->name    = name;
    this->address = address;
    this->classId = id;
    this->type    = "OBJECT";
    this->children.clear();
}

inline void Serialized::addChild(const Serialized& serialized) { this->children[serialized.name] = serialized; }

inline void Serialized::setPrimitive(const std::string& type, const std::string& name, const std::string& value) {
    this->type  = type;
    this->name  = name;
    this->value = value;
}

inline void Serialized::setPointer(const std::string& name, Address address, unsigned int id) {
    this->name    = name;
    this->address = address;
    this->classId = id;
    this->type    = "PTR";
}

inline Address Serialized::getAddress() const { return address; }

inline bool Serialized::setAddresses(AddressMap* map) {
    // If pointer: Replace address
    if(type == "PTR") {
        if(!map->contains(address)) return false;
        address = (*map)[address];
        return true;
    }

    // If object: Apply to all children, fail if one fails
    if(type == "OBJECT") {
        for(auto& [_, child] : children)
            if(!child.setAddresses(map)) return false;
        return true;
    }

    // Otherwise ignore
    return true;
}

template <typename T>
inline bool Serialized::checkType(const std::string& type, Serializer<T> serializer,
                                  Deserializer<T> deserializer) const {
    // Check if type name matches
    if(this->type != type) return false;

    // Try to deserialize and check consistency
    try {
        const T t = deserializer(value);
        return value == serializer(t);
    } catch(std::exception& exception) { return false; }
}

template <typename T> inline T Serialized::getValue(Deserializer<T> deserializer) const { return deserializer(value); }

inline std::optional<Serialized> Serialized::getChild(const std::string& name) {
    if(children.contains(name)) return children[name];
    return std::nullopt;
}

inline bool Serialized::isObject(unsigned int id) const { return type == "OBJECT" && classId == id; }

inline bool Serialized::isPointer(unsigned int id) const { return type == "PTR" && classId == id; }

inline std::vector<std::string> Serialized::parseChildrenData(const std::string& object) {
    // Create vector
    std::vector<std::string> children;

    // Strip data
    std::string data = replaceAll(object, "\n\t", "\n"); // Remove one level of indentation
    if(data == "{\n}") return children;                  // Base case: Empty object
    data.erase(0, 2).erase(data.size() - 2);             // Remove brackets and newline on both sides

    // Parse lines
    std::size_t pos = 0;
    while(true) {
        // Get line
        const std::size_t end  = data.find('\n', pos);
        const std::string line = data.substr(pos, end - pos);

        // If line starts with tab or bracket, add to last child, otherwise create new child
        if(line.starts_with('\t') || line.starts_with('}')) appendString(children.back(), {"\n", line});
        else children.push_back(line);

        // Advance pos
        if(end == std::string::npos) break;
        pos = end + 1;
    }

    return children;
}

namespace check {
inline std::string getType(const std::string& str) {
    if(str.starts_with("U")) return getType(str.substr(1)) == "SIGNED" ? "UNSIGNED" : "INVALID";

    if(str == "CHAR") return "SIGNED";
    if(str == "SHORT") return "SIGNED";
    if(str == "INT") return "SIGNED";
    if(str == "LONG") return "SIGNED";
    if(str == "ENUM") return "UNSIGNED";
    if(str == "FLOAT") return "FLOATING";
    if(str == "DOUBLE") return "FLOATING";

    return "INVALID";
}

inline bool isName(const std::string& str) {
    return !str.empty() &&
           !std::any_of(str.begin(), str.end(), [](char c) { return c == '\n' || c == '=' || c == '(' || c == ')'; });
}

inline bool isBool(const std::string& str) { return str == "true" || str == "false"; }

inline bool isUnsignedNumber(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) { return std::isdigit(c) != 0; });
}

inline bool isSignedNumber(const std::string& str) {
    if(str.starts_with('-')) return isUnsignedNumber(str.substr(1));
    return isUnsignedNumber(str);
}

inline bool isFloatingNumber(const std::string& str) {
    const std::size_t point = str.find('.');
    if(point == std::string::npos) return false;
    if(str.size() < point + 2) return false;
    return isSignedNumber(str.substr(0, point)) && isUnsignedNumber(str.substr(point + 1));
}

inline bool atString(const std::string& str, std::size_t pos, const std::string& match) {
    if(str.size() < pos + match.size()) return false;
    return str.substr(pos, match.size()) == match;
}

inline std::string substring(const std::string& str, std::size_t start, std::size_t end) {
    return str.substr(start, end - start);
}

inline bool matchObject(const std::string& str) { // REGEX: OBJECT .+ = \d+ \(\d+\) \{
    const std::size_t equals  = str.find('=');
    const std::size_t opening = str.find('(');
    const std::size_t closing = str.find(')');

    if(equals == std::string::npos) return false;
    if(opening == std::string::npos) return false;
    if(closing == std::string::npos) return false;
    if(!atString(str, 0, "OBJECT ")) return false;                               // Match 'OBJECT '
    if(!isName(substring(str, 7, equals - 1))) return false;                     // Match name
    if(!atString(str, equals - 1, " = ")) return false;                          // Match ' = '
    if(!isUnsignedNumber(substring(str, equals + 2, opening - 1))) return false; // Match address
    if(!atString(str, opening - 1, " (")) return false;                          // Match ' ('
    if(!isUnsignedNumber(substring(str, opening + 1, closing))) return false;    // Match type
    if(!atString(str, closing, ") {")) return false;                             // Match ') {'
    if(str.size() > closing + 3) return false;                                   // Match end

    return true;
}

inline bool matchBoolean(const std::string& str) { // REGEX: BOOL .+ = (true|false)
    const std::size_t equals = str.find('=');

    if(equals == std::string::npos) return false;
    if(!atString(str, 0, "BOOL ")) return false;                      // Match 'BOOL '
    if(!isName(substring(str, 5, equals - 1))) return false;          // Match name
    if(!atString(str, equals - 1, " = ")) return false;               // Match ' = '
    if(!isBool(substring(str, equals + 2, str.size()))) return false; // Match value to end

    return true;
}

inline bool matchSigned(const std::string& str) { // REGEX: (CHAR|SHORT|INT|LONG) .+ = -?\d+
    const std::size_t space  = str.find(' ');
    const std::size_t equals = str.find('=');

    if(space == std::string::npos) return false;
    if(equals == std::string::npos) return false;
    if(getType(substring(str, 0, space)) != "SIGNED") return false;           // Match type
    if(!atString(str, space, " ")) return false;                              // Match ' '
    if(!isName(substring(str, space + 1, equals - 1))) return false;          // Match name
    if(!atString(str, equals - 1, " = ")) return false;                       // Match ' = '
    if(!isSignedNumber(substring(str, equals + 2, str.size()))) return false; // Match value to end

    return true;
}

inline bool matchUnsigned(const std::string& str) { // REGEX: (UCHAR|USHORT|UINT|ULONG|ENUM) .+ = \d+
    const std::size_t space  = str.find(' ');
    const std::size_t equals = str.find('=');

    if(space == std::string::npos) return false;
    if(equals == std::string::npos) return false;
    if(getType(substring(str, 0, space)) != "UNSIGNED") return false;           // Match type
    if(!atString(str, space, " ")) return false;                                // Match ' '
    if(!isName(substring(str, space + 1, equals - 1))) return false;            // Match name
    if(!atString(str, equals - 1, " = ")) return false;                         // Match ' = '
    if(!isUnsignedNumber(substring(str, equals + 2, str.size()))) return false; // Match value to end

    return true;
}

inline bool matchFloating(const std::string& str) { // REGEX: (FLOAT|DOUBLE) .+ = -?\d+\.\d+
    const std::size_t space  = str.find(' ');
    const std::size_t equals = str.find('=');

    if(space == std::string::npos) return false;
    if(equals == std::string::npos) return false;
    if(getType(substring(str, 0, space)) != "FLOATING") return false;           // Match type
    if(!atString(str, space, " ")) return false;                                // Match ' '
    if(!isName(substring(str, space + 1, equals - 1))) return false;            // Match name
    if(!atString(str, equals - 1, " = ")) return false;                         // Match ' = '
    if(!isFloatingNumber(substring(str, equals + 2, str.size()))) return false; // Match value to end

    return true;
}

inline bool matchString(const std::string& str) { // REGEX: STRING .+ = \".*\"
    const std::size_t equals = str.find('=');

    if(equals == std::string::npos) return false;
    if(!atString(str, 0, "STRING ")) return false;                                              // Match 'STRING '
    if(!isName(substring(str, 7, equals - 1))) return false;                                    // Match name
    if(!atString(str, equals - 1, " = \"")) return false;                                       // Match ' = "'
    if(substring(str, equals + 3, str.size() - 1).find('"') != std::string::npos) return false; // Match value
    if(!atString(str, str.size() - 1, "\"")) return false;                                      // Match '"' at end

    return true;
}

inline bool matchPointer(const std::string& str) { // REGEX: PTR .+ = \d+ \(\d+\)
    const std::size_t equals  = str.find('=');
    const std::size_t opening = str.find('(');
    const std::size_t closing = str.find(')');

    if(equals == std::string::npos) return false;
    if(opening == std::string::npos) return false;
    if(closing == std::string::npos) return false;
    if(!atString(str, 0, "PTR ")) return false;                                  // Match 'PTR '
    if(!isName(substring(str, 4, equals - 1))) return false;                     // Match name
    if(!atString(str, equals - 1, " = ")) return false;                          // Match ' = '
    if(!isUnsignedNumber(substring(str, equals + 2, opening - 1))) return false; // Match address
    if(!atString(str, opening - 1, " (")) return false;                          // Match ' ('
    if(!isUnsignedNumber(substring(str, opening + 1, closing))) return false;    // Match type
    if(!atString(str, closing, ")")) return false;                               // Match ') {'
    if(str.size() > closing + 1) return false;                                   // Match end

    return true;
}
} // namespace check
} // namespace detail

class Serializable {
  public:
    enum class Result { OK, FILE, STRUCTURE, TYPECHECK, INTEGRITY, POINTER };

    Serializable()                               = default;
    Serializable(const Serializable&)            = default;
    Serializable(Serializable&&)                 = delete;
    Serializable& operator=(const Serializable&) = default;
    Serializable& operator=(Serializable&&)      = delete;
    virtual ~Serializable()                      = default;

    std::pair<Result, std::string> serialize();
    static Result check(const std::string& data);
    Result deserialize(const std::string& data);
    Result save(const std::filesystem::path& file);
    Result load(const std::filesystem::path& file);

  protected:
    virtual void exposed() = 0;
    virtual unsigned int classID() const;

    void expose(const std::string& name, bool& value);
    void expose(const std::string& name, char& value);
    void expose(const std::string& name, unsigned char& value);
    void expose(const std::string& name, short& value);
    void expose(const std::string& name, unsigned short& value);
    void expose(const std::string& name, int& value);
    void expose(const std::string& name, unsigned int& value);
    void expose(const std::string& name, long& value);
    void expose(const std::string& name, unsigned long& value);
    void expose(const std::string& name, float& value);
    void expose(const std::string& name, double& value);
    void expose(const std::string& name, std::string& value);
    void expose(const std::string& name, Serializable& value);

    template <detail::Enum E> void expose(const std::string& name, E& value);
    template <detail::SerializableChild S> void expose(const std::string& name, S*& value);

  private:
    enum class Action { IDLE, SERIALIZE, DESERIALIZE } action{Action::IDLE};
    Result result{Result::OK};
    detail::Serialized serialized;
    detail::AddressMap *addressTable{}, *pointerTable{};
    std::unordered_map<detail::Address, unsigned int>* pointerTypes{};

    template <typename T>
    void expose(const std::string& type, const std::string& name, T& value, detail::Serializer<T> serializer,
                detail::Deserializer<T> deserializer);
};

template <typename T, std::size_t size> class Array : public Serializable, public std::array<T, size> {
  protected:
    inline void exposed() override {
        // Expose and check length
        std::size_t length = size;
        expose("length", length);
        if(length != size) return;

        // Expose elements
        for(std::size_t i = 0; i < size; i++) expose(std::to_string(i), this->at(i));
    }

    unsigned int classID() const override { return 1; }
};

template <typename T> class Vector : public Serializable, public std::vector<T> {
  protected:
    void exposed() override {
        // Expose length and resize
        std::size_t length = this->size();
        expose("length", length);
        this->resize(length);

        // Expose elements
        for(std::size_t i = 0; i < length; i++) expose(std::to_string(i), this->at(i));
    }

    unsigned int classID() const override { return 2; }
};

template <typename T> class List : public Serializable, public std::list<T> {
  protected:
    void exposed() override {
        // Expose length and resize
        std::size_t length = this->size();
        expose("length", length);
        this->resize(length);

        // Expose elements
        std::size_t index = 0;
        for(auto it = this->begin(); it != this->end(); it++) expose(std::to_string(index++), *it);
    }

    unsigned int classID() const override { return 3; }
};

inline std::pair<Serializable::Result, std::string> Serializable::serialize() {
    // Create address map
    detail::AddressMap addresses;
    addressTable = &addresses;

    // Register own address
    const auto address = std::bit_cast<detail::Address>(this);
    addresses[address] = 0;

    // Setup serialized data
    serialized.setObject("ROOT", 0, classID());

    // Expose fields
    action = Action::SERIALIZE;
    result = Result::OK;
    exposed();
    action = Action::IDLE;

    // Replace physical addresses with virtual addresses
    if(result == Result::OK && !serialized.setAddresses(addressTable)) result = Result::POINTER;
    addressTable = nullptr;

    // Return result and serialized data
    return {result, result == Result::OK ? serialized.serialize() : ""};
}

inline Serializable::Result Serializable::check(const std::string& data) { // NOLINT(*-cognitive-complexity)
    // Split string into vector of lines
    std::vector<std::string> lines;
    std::stringstream stream(data);
    std::string line;
    while(std::getline(stream, line)) lines.push_back(line);

    // Parse lines
    int depth = 0;
    for(auto& line : lines) {
        // Check if line is end of object
        if(depth > 0 && line == std::string(depth - 1, '\t').append("}")) {
            // Decrease depth and finish if root object closed and EOF
            if(--depth == 0) return &line == &lines.back() ? Result::OK : Result::STRUCTURE;
            continue;
        }

        // Check and remove tabs
        if(depth > 0) {
            if(!line.starts_with(std::string(depth, '\t'))) return Result::STRUCTURE;
            line.erase(0, depth);
        }

        // Parse objects
        if(line.starts_with("OBJECT")) {
            if(!detail::check::matchObject(line)) return Result::STRUCTURE;
            depth++;
            continue;
        }

        // Parse primitive types
        if(line.starts_with("BOOL") && detail::check::matchBoolean(line)) continue;
        if(line.starts_with("U") && detail::check::matchUnsigned(line)) continue;
        if(line.starts_with("ENUM") && detail::check::matchUnsigned(line)) continue;
        if(line.starts_with("STRING") && detail::check::matchString(line)) continue;
        if(line.starts_with("PTR") && detail::check::matchPointer(line)) continue;
        if(detail::check::matchSigned(line)) continue;
        if(detail::check::matchFloating(line)) continue;

        // Not a valid type
        return Result::STRUCTURE;
    }

    // Ending here means root object wasn't closed
    return Result::STRUCTURE;
}

inline Serializable::Result Serializable::deserialize(const std::string& data) {
    // Check syntax
    Result syntax = check(data);
    if(syntax != Result::OK) return syntax;

    // Setup serialized data
    serialized.deserialize(data);

    // Check for root object type
    if(!serialized.isObject(classID())) return Result::TYPECHECK;

    // Create address maps
    detail::AddressMap addresses;
    detail::AddressMap pointers;
    std::unordered_map<detail::Address, unsigned int> types;
    addressTable = &addresses;
    pointerTable = &pointers;
    pointerTypes = &types;

    // Register own address
    const auto address = std::bit_cast<detail::Address>(this);
    addresses[0]       = address;

    // Expose fields
    action = Action::DESERIALIZE;
    result = Result::OK;
    exposed();
    action = Action::IDLE;

    // Apply addresses (skip if error state)
    if(result == Result::OK)
        for(auto& [ptr, virt] : pointers) {
            // Check if virtual address is resolved
            if(!addresses.contains(virt)) {
                result = Result::POINTER;
                break;
            }

            // Get object and check classId
            auto* object = std::bit_cast<Serializable*>(addresses[virt]);
            if(types[ptr] != object->classID()) {
                result = Result::TYPECHECK;
                break;
            }

            // Assign pointer
            (*std::bit_cast<Serializable**>(ptr)) = object;
        }

    // Unset address and pointer map
    addressTable = nullptr;
    pointerTable = nullptr;
    pointerTypes = nullptr;

    return result;
}

inline Serializable::Result Serializable::save(const std::filesystem::path& file) {
    // Create parent path
    if(file.has_parent_path()) std::filesystem::create_directories(file.parent_path());

    // Open and check file
    std::ofstream stream(file);
    if(!stream) return Result::FILE;

    // Serialize data
    auto [result, serial] = serialize();
    if(result != Result::OK) return result;

    // Write serial data
    stream << serial;
    stream.close();

    // Finish
    return Result::OK;
}

inline Serializable::Result Serializable::load(const std::filesystem::path& file) {
    // Open and check file
    const std::ifstream stream(file);
    if(!stream) return Result::FILE;

    // Read serialized data
    std::stringstream str;
    str << stream.rdbuf();

    // Deserialize data
    return deserialize(str.str());
}

inline unsigned int Serializable::classID() const { return 0; }

inline void Serializable::expose(const std::string& name, bool& value) {
    static detail::Serializer<bool> S   = [](const auto& val) { return std::string(val ? "true" : "false"); };
    static detail::Deserializer<bool> D = [](const auto& val) { return val == "true"; };
    expose("BOOL", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, char& value) {
    static detail::Serializer<char> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<char> D = [](const auto& val) { return static_cast<char>(std::stoi(val)); };
    expose("CHAR", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, unsigned char& value) {
    using T = unsigned char;

    static detail::Serializer<T> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<T> D = [](const auto& val) { return static_cast<T>(std::stoi(val)); };
    expose("UCHAR", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, short& value) {
    static detail::Serializer<short> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<short> D = [](const auto& val) { return static_cast<short>(std::stoi(val)); };
    expose("SHORT", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, unsigned short& value) {
    using T = unsigned short;

    static detail::Serializer<T> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<T> D = [](const auto& val) { return static_cast<T>(std::stoi(val)); };
    expose("USHORT", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, int& value) {
    static detail::Serializer<int> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<int> D = [](const auto& val) { return std::stoi(val); };
    expose("INT", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, unsigned int& value) {
    using T = unsigned int;

    static detail::Serializer<T> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<T> D = [](const auto& val) { return static_cast<T>(std::stoul(val)); };
    expose("UINT", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, long& value) {
    static detail::Serializer<long> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<long> D = [](const auto& val) { return std::stol(val); };
    expose("LONG", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, unsigned long& value) {
    static detail::Serializer<unsigned long> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<unsigned long> D = [](const auto& val) { return std::stoul(val); };
    expose("ULONG", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, float& value) {
    static detail::Serializer<float> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<float> D = [](const auto& val) { return std::stof(val); };
    expose("FLOAT", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, double& value) {
    static detail::Serializer<double> S   = [](const auto& val) { return std::to_string(val); };
    static detail::Deserializer<double> D = [](const auto& val) { return std::stod(val); };
    expose("DOUBLE", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, std::string& value) {
    static detail::Serializer<std::string> S = [](const auto& val) {
        std::string safe = detail::replaceAll(val, "\"", "&quot;");
        safe             = detail::replaceAll(safe, "\n", "&newline;");
        return detail::createString({"\"", safe, "\""});
    };

    static detail::Serializer<std::string> D = [](const auto& val) {
        std::string unsafe = detail::replaceAll(val.substr(1, val.size() - 2), "&newline;", "\n");
        unsafe             = detail::replaceAll(unsafe, "&quot;", "\"");
        return unsafe;
    };

    expose("STRING", name, value, S, D);
}

template <detail::Enum E> inline void Serializable::expose(const std::string& name, E& value) {
    static detail::Serializer<E> S   = [](const auto& val) { return std::to_string(static_cast<int>(val)); };
    static detail::Deserializer<E> D = [](const auto& val) { return static_cast<E>(std::stoi(val)); };
    expose("ENUM", name, value, S, D);
}

inline void Serializable::expose(const std::string& name, Serializable& value) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    // Pass state to value
    value.action       = action;
    value.result       = result;
    value.addressTable = addressTable;
    value.pointerTable = pointerTable;
    value.pointerTypes = pointerTypes;

    // Get values address
    const auto address = std::bit_cast<detail::Address>(&value);

    switch(action) {
    case Action::IDLE: break;

    case Action::SERIALIZE: {
        (*addressTable)[address] = addressTable->size();
        value.serialized.setObject(name, (*addressTable)[address], value.classID());
        value.exposed();
        serialized.addChild(value.serialized);
        break;
    }

    case Action::DESERIALIZE: {
        const std::optional<detail::Serialized> child = serialized.getChild(name);
        if(!child) result = Result::INTEGRITY;
        else if(!child->isObject(value.classID())) result = Result::TYPECHECK;
        else {
            (*addressTable)[child->getAddress()] = address;
            value.serialized                     = *child;
            value.exposed();
        }
        break;
    }
    }

    // Take state from value
    result = value.result;

    // Reset state of value
    value.action       = Action::IDLE;
    result             = Result::OK;
    value.addressTable = nullptr;
    value.pointerTable = nullptr;
    value.pointerTypes = nullptr;
}

template <detail::SerializableChild S> inline void Serializable::expose(const std::string& name, S*& value) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    // Can't expose nullptr
    if(value == nullptr) {
        result = Result::POINTER;
        return;
    }

    // Get values address
    const auto address = std::bit_cast<detail::Address>(&value);

    switch(action) {
    case Action::IDLE: break;

    case Action::SERIALIZE: {
        detail::Serialized pointer;
        pointer.setPointer(name, std::bit_cast<detail::Address>(value), (value == nullptr ? 0 : value->classID()));
        serialized.addChild(pointer);
        break;
    }

    case Action::DESERIALIZE: {
        const std::optional<detail::Serialized> pointer = serialized.getChild(name);
        if(!pointer) result = Result::INTEGRITY;
        else if(!pointer->isPointer(value->classID())) result = Result::TYPECHECK;
        else {
            (*pointerTable)[address] = pointer->getAddress();
            (*pointerTypes)[address] = value == nullptr ? 0 : value->classID();
        }
        break;
    }
    }
}

template <typename T>
inline void Serializable::expose(const std::string& type, const std::string& name, T& value,
                                 detail::Serializer<T> serializer, detail::Deserializer<T> deserializer) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    switch(action) {
    case Action::IDLE: break;

    case Action::SERIALIZE: {
        detail::Serialized primitive;
        primitive.setPrimitive(type, name, serializer(value));
        serialized.addChild(primitive);
        break;
    }

    case Action::DESERIALIZE: {
        const std::optional<detail::Serialized> primitive = serialized.getChild(name);
        if(!primitive) result = Result::INTEGRITY;
        else if(!primitive->checkType(type, serializer, deserializer)) result = Result::TYPECHECK;
        else value = primitive->getValue(deserializer);
        break;
    }
    }
}
} // namespace serializable

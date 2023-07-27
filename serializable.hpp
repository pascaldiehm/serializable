#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace serializable {
class Serializable;

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
    void addChild(Serialized serialized);
    void setPrimitive(const std::string& type, const std::string& name, const std::string& value);
    void setPointer(const std::string& name, Address address, unsigned int id);

    template <typename T>
    [[nodiscard]] bool checkType(const std::string& type, Serializer<T> serializer, Deserializer<T> deserializer) const;
    template <typename T> [[nodiscard]] T getValue(Deserializer<T> deserializer) const;
    std::optional<Serialized> getChild(const std::string& name);
    [[nodiscard]] bool isObject(unsigned int id) const;
    [[nodiscard]] bool isPointer() const;
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

    void expose(const char* name, bool& value);
    void expose(const char* name, char& value);
    void expose(const char* name, unsigned char& value);
    void expose(const char* name, short& value);
    void expose(const char* name, unsigned short& value);
    void expose(const char* name, int& value);
    void expose(const char* name, unsigned int& value);
    void expose(const char* name, long& value);
    void expose(const char* name, unsigned long& value);
    void expose(const char* name, float& value);
    void expose(const char* name, double& value);
    void expose(const char* name, std::string& value);
    void expose(const char* name, Serializable& value);

    template <Enum E> void expose(const char* name, E& value);
    template <SerializableChild S> void expose(const char* name, S*& value);

  private:
    enum class Action { IDLE, SERIALIZE, DESERIALIZE } action{Action::IDLE};
    Result result{Result::OK};
    Serialized serialized;
    AddressMap *addressTable{}, *pointerTable{};
    std::unordered_map<Address, unsigned int>* pointerTypes{};

    template <typename T>
    void expose(const char* type, const char* name, T& value, Serializer<T> serializer, Deserializer<T> deserializer);
};

template <typename T, std::size_t size> class Array : public Serializable, public std::array<T, size> {
  protected:
    inline void exposed() override {
        // Expose and check length
        std::size_t length = size;
        expose("length", length);
        if(length != size) return;

        // Expose elements
        for(std::size_t i = 0; i < size; i++) expose(std::to_string(i).c_str(), this->at(i));
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
        for(std::size_t i = 0; i < length; i++) expose(std::to_string(i).c_str(), this->at(i));
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
        for(auto it = this->begin(); it != this->end(); it++) expose(std::to_string(index++).c_str(), *it);
    }

    unsigned int classID() const override { return 3; }
};

inline std::string Serialized::serialize() const {
    static const std::regex matchNewline("\n");

    // Construct object
    if(type == "OBJECT") {
        std::string data = std::string("OBJECT ")
                               .append(name)
                               .append(" = ")
                               .append(std::to_string(address))
                               .append(" (")
                               .append(std::to_string(classId))
                               .append(") {\n");
        for(const auto& [_, child] : children)
            data.append("\t").append(std::regex_replace(child.serialize(), matchNewline, "\n\t")).append("\n");
        return data.append("}");
    }

    // Construct primitive value
    if(type != "PTR") return std::string(type).append(" ").append(name).append(" = ").append(value);

    // Construct pointer value
    return std::string(type)
        .append(" ")
        .append(name)
        .append(" = ")
        .append(std::to_string(address))
        .append(" (")
        .append(std::to_string(classId))
        .append(")");
}

inline void Serialized::deserialize(const std::string& data) {
    // Create regex parsers
    static const std::regex parseObjectHeader(R"(OBJECT (.+) = (\d+) \((\d+)\))");
    static const std::regex parsePointerData(R"(PTR (.+) = (\d+) \((\d+)\))");
    static const std::regex parsePrimitiveData("([A-Z]+) (.+) = (.+)");

    if(data.starts_with("OBJECT")) {
        const std::size_t bracket = data.find('{');
        const std::string header  = data.substr(0, bracket - 1);
        const std::string name    = std::regex_replace(header, parseObjectHeader, "$1");
        const std::string address = std::regex_replace(header, parseObjectHeader, "$2");
        const std::string id      = std::regex_replace(header, parseObjectHeader, "$3");

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
        // Parse data
        const std::string name    = std::regex_replace(data, parsePointerData, "$1");
        const std::string address = std::regex_replace(data, parsePointerData, "$2");
        const std::string id      = std::regex_replace(data, parsePointerData, "$3");

        // Set data
        setPointer(name, std::stoul(address), std::stoul(id));
    } else {
        // Parse data
        const std::string type  = std::regex_replace(data, parsePrimitiveData, "$1");
        const std::string name  = std::regex_replace(data, parsePrimitiveData, "$2");
        const std::string value = std::regex_replace(data, parsePrimitiveData, "$3");

        // Set data
        setPrimitive(type, name, value);
    }
}

inline void Serialized::setObject(const std::string& name, Address address, unsigned int id) {
    this->name    = name;
    this->address = address;
    this->classId = id;
    this->type    = "OBJECT";
    this->value   = "";
    this->children.clear();
}

inline void Serialized::addChild(Serialized serialized) { this->children[serialized.name] = std::move(serialized); }

inline void Serialized::setPrimitive(const std::string& type, const std::string& name, const std::string& value) {
    this->type    = type;
    this->name    = name;
    this->value   = value;
    this->classId = 0;
    this->address = 0;
    this->children.clear();
}

inline void Serialized::setPointer(const std::string& name, Address address, unsigned int id) {
    this->name    = name;
    this->address = address;
    this->classId = id;
    this->type    = "PTR";
    this->value   = "";
    this->children.clear();
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
        T t = deserializer(value);
        return value == serializer(t);
    } catch(std::exception& exception) { return false; }
}

template <typename T> inline T Serialized::getValue(Deserializer<T> deserializer) const { return deserializer(value); }

inline std::optional<Serialized> Serialized::getChild(const std::string& name) {
    if(children.contains(name)) return children[name];
    return std::nullopt;
}

inline bool Serialized::isObject(unsigned int id) const { return type == "OBJECT" && classId == id; }

inline bool Serialized::isPointer() const { return type == "PTR"; }

inline bool Serialized::isPointer(unsigned int id) const { return type == "PTR" && classId == id; }

inline std::vector<std::string> Serialized::parseChildrenData(const std::string& object) {
    static const std::regex matchNewlineTab("\n\t");

    // Create vector
    std::vector<std::string> children;

    // Strip data
    std::string data = std::regex_replace(object, matchNewlineTab, "\n"); /// Remove one level of indentation
    if(data == "{\n}") return children;                                   // Base case: Empty object
    data = data.substr(2, data.size() - 4);                               // Remove brackets and newline on both sides

    // Parse lines
    std::size_t pos = 0;
    while(pos != std::string::npos) {
        // Get line
        const std::size_t end  = data.find('\n', pos);
        const std::string line = data.substr(pos, end - pos);

        // If line starts with tab or bracket, add to last child, otherwise create new child
        if(line.starts_with('\t') || line.starts_with('}')) children.back().append("\n").append(line);
        else children.push_back(line);

        // Advance pos
        if(end == std::string::npos) break;
        pos = end + 1;
    }

    return children;
}

inline std::pair<Serializable::Result, std::string> Serializable::serialize() {
    // Create address map
    AddressMap addresses;
    addressTable = &addresses;

    // Register own address
    const auto address = std::bit_cast<Address>(this);
    addresses[address] = 0;

    // Setup serialized data
    serialized.setObject("ROOT", 0, classID());

    // Expose fields
    action = Action::SERIALIZE;
    result = Result::OK;
    exposed();
    action = Action::IDLE;

    // Replace physical addresses with virtual addresses
    if(!serialized.setAddresses(addressTable)) result = Result::POINTER;
    addressTable = nullptr;

    // Write serialized data to target
    std::string data = serialized.serialize();

    return {result, data};
}

inline Serializable::Result Serializable::check(const std::string& data) { // NOLINT(*-cognitive-complexity)
    // Create regex matchers
    static const std::regex matchObject(R"(OBJECT .+ = (\d+) \(\d+\) \{)");
    static const std::regex matchBoolean("(BOOL) .+ = (true|false)");
    static const std::regex matchSigned("(CHAR|SHORT|INT|LONG) .+ = (-?\\d+)");
    static const std::regex matchUnsigned("(UCHAR|USHORT|UINT|ULONG|ENUM) .+ = (\\d+)");
    static const std::regex matchFloating(R"((FLOAT|DOUBLE) .+ = (-?\d+\.\d+))");
    static const std::regex matchString("(STRING) .+ = (\".*\")");
    static const std::regex matchPointer(R"(PTR .+ = (\d+) \(\d+\))");

    // Split string into vector of lines
    std::vector<std::string> lines;
    std::stringstream stream(data);
    std::string line;
    while(std::getline(stream, line)) lines.push_back(line);

    // Parse lines
    int depth = 0;
    int ends  = 0;
    for(auto& line : lines) {
        // Check if line is end of object
        if(depth > 0 && line == std::string(depth - 1, '\t').append("}")) {
            // Decrease depth and end if root object closed
            if(--depth == 0) ends++;
            continue;
        }

        // Check and remove tabs
        if(!line.starts_with(std::string(depth, '\t'))) return Result::STRUCTURE;
        line = line.substr(depth);

        // Parse objects
        if(line.starts_with("OBJECT")) {
            if(!std::regex_match(line, matchObject)) return Result::STRUCTURE;
            depth++;
            continue;
        }

        // Parse primitive types
        if(std::regex_match(line, matchBoolean)) continue;
        if(std::regex_match(line, matchSigned)) continue;
        if(std::regex_match(line, matchUnsigned)) continue;
        if(std::regex_match(line, matchFloating)) continue;
        if(std::regex_match(line, matchString)) continue;
        if(std::regex_match(line, matchPointer)) continue;

        // Not a valid type
        return Result::STRUCTURE;
    }

    // Expect exactly one top level end
    return ends == 1 ? Result::OK : Result::STRUCTURE;
}

inline Serializable::Result Serializable::deserialize(const std::string& data) {
    // Check syntax
    Result syntax = check(data);
    if(syntax != Result::OK) return syntax;

    // Setup serialized data
    serialized.deserialize(data);

    // Create address maps
    AddressMap addresses;
    AddressMap pointers;
    std::unordered_map<Address, unsigned int> types;
    addressTable = &addresses;
    pointerTable = &pointers;
    pointerTypes = &types;

    // Register own address
    const auto address = std::bit_cast<Address>(this);
    addresses[0]       = address;

    // Expose fields
    action = Action::DESERIALIZE;
    result = Result::OK;
    exposed();
    action = Action::IDLE;

    // Check for root object type
    if(!serialized.isObject(classID())) result = Result::TYPECHECK;

    // Apply addresses
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

inline void Serializable::expose(const char* name, bool& value) {
    const constexpr Serializer<bool> S   = [](const auto& val) { return std::string(val ? "true" : "false"); };
    const constexpr Deserializer<bool> D = [](const auto& val) { return val == "true"; };
    expose("BOOL", name, value, S, D);
}

inline void Serializable::expose(const char* name, char& value) {
    const constexpr Serializer<char> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<char> D = [](const auto& val) { return static_cast<char>(std::stoi(val)); };
    expose("CHAR", name, value, S, D);
}

inline void Serializable::expose(const char* name, unsigned char& value) {
    using uchar = unsigned char;

    const constexpr Serializer<uchar> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<uchar> D = [](const auto& val) { return static_cast<uchar>(std::stoi(val)); };
    expose("UCHAR", name, value, S, D);
}

inline void Serializable::expose(const char* name, short& value) {
    const constexpr Serializer<short> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<short> D = [](const auto& val) { return static_cast<short>(std::stoi(val)); };
    expose("SHORT", name, value, S, D);
}

inline void Serializable::expose(const char* name, unsigned short& value) {
    using ushort = unsigned short;

    const constexpr Serializer<ushort> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<ushort> D = [](const auto& val) { return static_cast<ushort>(std::stoi(val)); };
    expose("USHORT", name, value, S, D);
}

inline void Serializable::expose(const char* name, int& value) {
    const constexpr Serializer<int> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<int> D = [](const auto& val) { return std::stoi(val); };
    expose("INT", name, value, S, D);
}

inline void Serializable::expose(const char* name, unsigned int& value) {
    using uint = unsigned int;

    const constexpr Serializer<uint> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<uint> D = [](const auto& val) { return static_cast<uint>(std::stoul(val)); };
    expose("UINT", name, value, S, D);
}

inline void Serializable::expose(const char* name, long& value) {
    const constexpr Serializer<long> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<long> D = [](const auto& val) { return std::stol(val); };
    expose("LONG", name, value, S, D);
}

inline void Serializable::expose(const char* name, unsigned long& value) {
    const constexpr Serializer<unsigned long> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<unsigned long> D = [](const auto& val) { return std::stoul(val); };
    expose("ULONG", name, value, S, D);
}

inline void Serializable::expose(const char* name, float& value) {
    const constexpr Serializer<float> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<float> D = [](const auto& val) { return std::stof(val); };
    expose("FLOAT", name, value, S, D);
}

inline void Serializable::expose(const char* name, double& value) {
    const constexpr Serializer<double> S   = [](const auto& val) { return std::to_string(val); };
    const constexpr Deserializer<double> D = [](const auto& val) { return std::stod(val); };
    expose("DOUBLE", name, value, S, D);
}

inline void Serializable::expose(const char* name, std::string& value) {
    const constexpr Serializer<std::string> S = [](const auto& val) {
        static const std::regex matchQuote("\"");
        static const std::regex matchNewline("\n");

        std::string safe = std::regex_replace(val, matchQuote, "&quot;");
        safe             = std::regex_replace(safe, matchNewline, "&newline;");
        return "\"" + safe + "\"";
    };

    const constexpr Serializer<std::string> D = [](const auto& val) {
        static const std::regex matchNewline("&newline;");
        static const std::regex matchQuote("&quot;");

        std::string unsafe = std::regex_replace(val.substr(1, val.size() - 2), std::regex(matchNewline), "\n");
        unsafe             = std::regex_replace(unsafe, std::regex(matchQuote), "\"");
        return unsafe;
    };

    expose("STRING", name, value, S, D);
}

template <Enum E> inline void Serializable::expose(const char* name, E& value) {
    const constexpr Serializer<E> S   = [](const auto& val) { return std::to_string(static_cast<int>(val)); };
    const constexpr Deserializer<E> D = [](const auto& val) { return static_cast<E>(std::stoi(val)); };
    expose("ENUM", name, value, S, D);
}

inline void Serializable::expose(const char* name, Serializable& value) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    // Pass state to value
    value.action       = action;
    value.result       = result;
    value.addressTable = addressTable;
    value.pointerTable = pointerTable;
    value.pointerTypes = pointerTypes;

    // Get values address
    const auto address = std::bit_cast<Address>(&value);

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
        std::optional<Serialized> child = serialized.getChild(name);
        if(!child) result = Result::INTEGRITY;
        else if(!child->isObject(value.classID())) result = Result::TYPECHECK;
        else {
            (*addressTable)[child->getAddress()] = address;
            value.serialized                     = child.value();
            value.exposed();
        }
        break;
    }
    }

    // Take state from value
    value.action       = Action::IDLE;
    result             = value.result;
    value.addressTable = nullptr;
    value.pointerTable = nullptr;
    value.pointerTypes = nullptr;
}

template <SerializableChild S> inline void Serializable::expose(const char* name, S*& value) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    // Get values address
    const auto address = std::bit_cast<Address>(&value);

    switch(action) {
    case Action::IDLE: break;

    case Action::SERIALIZE: {
        Serialized pointer;
        pointer.setPointer(name, std::bit_cast<Address>(value), (value == nullptr ? 0 : value->classID()));
        serialized.addChild(pointer);
        break;
    }

    case Action::DESERIALIZE: {
        std::optional<Serialized> pointer = serialized.getChild(name);
        if(!pointer) result = Result::INTEGRITY;
        else if(!pointer->isPointer() || (value != nullptr && !pointer->isPointer(value->classID())))
            result = Result::TYPECHECK;
        else {
            (*pointerTable)[address] = pointer->getAddress();
            (*pointerTypes)[address] = value == nullptr ? 0 : value->classID();
        }
        break;
    }
    }
}

template <typename T>
inline void Serializable::expose(const char* type, const char* name, T& value, Serializer<T> serializer,
                                 Deserializer<T> deserializer) {
    // Only expose if result is OK
    if(result != Result::OK) return;

    switch(action) {
    case Action::IDLE: break;

    case Action::SERIALIZE: {
        Serialized primitive;
        primitive.setPrimitive(type, name, serializer(value));
        serialized.addChild(primitive);
        break;
    }

    case Action::DESERIALIZE: {
        std::optional<Serialized> primitive = serialized.getChild(name);
        if(!primitive) result = Result::INTEGRITY;
        else if(!primitive->checkType(type, serializer, deserializer)) result = Result::TYPECHECK;
        else value = primitive->getValue(deserializer);
        break;
    }
    }
}
} // namespace serializable

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
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
using Address = unsigned long;

template <typename T> concept SerializableObject = std::is_base_of_v<Serializable, T>;

template <typename T> concept Enum = std::is_enum_v<T>;

template <typename T> concept Number = requires(T t) {
    { std::to_string(t) } -> std::same_as<std::string>;
};

class Serial;
class SerialPrimitive;
class SerialObject;
class SerialPointer;

class Serial {
  public:
    Serial()                         = default;
    Serial(const Serial&)            = default;
    Serial(Serial&&)                 = delete;
    Serial& operator=(const Serial&) = default;
    Serial& operator=(Serial&&)      = delete;
    virtual ~Serial()                = default;

    [[nodiscard]] virtual std::string get() const           = 0;
    [[nodiscard]] virtual bool set(const std::string& data) = 0;
    [[nodiscard]] virtual std::string getName() const       = 0;

    [[nodiscard]] SerialPrimitive* asPrimitive();
    [[nodiscard]] SerialObject* asObject();
    [[nodiscard]] SerialPointer* asPointer();
};

class SerialPrimitive : public Serial {
  public:
    SerialPrimitive() = default;
    SerialPrimitive(std::string type, std::string name, std::string value);

    [[nodiscard]] std::string get() const override;
    [[nodiscard]] bool set(const std::string& data) override;
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::string getType() const;
    [[nodiscard]] std::string getValue() const;

  private:
    std::string type, name, value;
};

class SerialObject : public Serial {
  public:
    SerialObject() = default;
    SerialObject(unsigned int classID, std::string name, Address realAddress, Address virtualAddress);

    [[nodiscard]] std::string get() const override;
    [[nodiscard]] bool set(const std::string& data) override;
    [[nodiscard]] std::string getName() const override;

    void append(const std::shared_ptr<Serial>& child);
    [[nodiscard]] std::optional<std::shared_ptr<Serial>> getChild(const std::string& name) const;
    [[nodiscard]] unsigned int getClass() const;
    void virtualizeAddresses(std::unordered_map<Address, Address>& addressMap);
    void restoreAddresses(std::unordered_map<Address, Address>& addressMap) const;
    [[nodiscard]] bool virtualizePointers(const std::unordered_map<Address, Address>& addressMap);
    [[nodiscard]] bool restorePointers(const std::unordered_map<Address, Address>& addressMap);
    void setRealAddress(Address address);

  private:
    std::string name;
    unsigned int classID{};
    Address realAddress{}, virtualAddress{};
    std::unordered_map<std::string, std::shared_ptr<Serial>> children;
};

class SerialPointer : public Serial {
  public:
    SerialPointer() = default;
    SerialPointer(unsigned int classID, std::string name, void** location);

    [[nodiscard]] std::string get() const override;
    [[nodiscard]] bool set(const std::string& data) override;
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] unsigned int getClass() const;
    [[nodiscard]] bool virtualizePointers(const std::unordered_map<Address, Address>& addressMap);
    [[nodiscard]] bool restorePointers(const std::unordered_map<Address, Address>& addressMap);
    void setTarget(void** location);

  private:
    std::string name;
    unsigned int classID{};
    void** location{};
    Address address{};
};

namespace string {
std::string makeString(const std::initializer_list<std::string>& parts);
std::string substring(const std::string& str, std::size_t start, std::size_t end);
std::string replaceAll(const std::string& str, const std::string& from, const std::string& to);
std::string connect(const std::vector<std::string>& lines, char delimiter = '\n');
std::vector<std::string> split(const std::string& data, char delimiter = '\n');
std::string indent(const std::string& data);
std::string unindent(const std::string& data);

template <typename T> inline const constexpr auto TypeToString       = "VOID";
template <> inline const constexpr auto TypeToString<bool>           = "BOOL";
template <> inline const constexpr auto TypeToString<char>           = "CHAR";
template <> inline const constexpr auto TypeToString<unsigned char>  = "UCHAR";
template <> inline const constexpr auto TypeToString<short>          = "SHORT";
template <> inline const constexpr auto TypeToString<unsigned short> = "UCHAR";
template <> inline const constexpr auto TypeToString<int>            = "INT";
template <> inline const constexpr auto TypeToString<unsigned int>   = "UINT";
template <> inline const constexpr auto TypeToString<long>           = "LONG";
template <> inline const constexpr auto TypeToString<unsigned long>  = "ULONG";
template <> inline const constexpr auto TypeToString<float>          = "FLOAT";
template <> inline const constexpr auto TypeToString<double>         = "DOUBLE";
template <> inline const constexpr auto TypeToString<std::string>    = "STRING";
template <Enum E> inline const constexpr auto TypeToString<E>        = "ENUM";

template <typename T> std::string serializePrimitive(const T& val) = delete;
template <> std::string serializePrimitive<bool>(const bool& val);
template <> std::string serializePrimitive<std::string>(const std::string& val);
template <Enum E> std::string serializePrimitive(const E& val);
template <Number N> std::string serializePrimitive(const N& val);
template <typename T> std::optional<T> deserializePrimitive(const std::string& str) = delete;
template <> std::optional<bool> deserializePrimitive<bool>(const std::string& str);
template <> std::optional<char> deserializePrimitive<char>(const std::string& str);
template <> std::optional<unsigned char> deserializePrimitive<unsigned char>(const std::string& str);
template <> std::optional<short> deserializePrimitive<short>(const std::string& str);
template <> std::optional<unsigned short> deserializePrimitive<unsigned short>(const std::string& str);
template <> std::optional<int> deserializePrimitive<int>(const std::string& str);
template <> std::optional<unsigned int> deserializePrimitive<unsigned int>(const std::string& str);
template <> std::optional<long> deserializePrimitive<long>(const std::string& str);
template <> std::optional<unsigned long> deserializePrimitive<unsigned long>(const std::string& str);
template <> std::optional<float> deserializePrimitive<float>(const std::string& str);
template <> std::optional<double> deserializePrimitive<double>(const std::string& str);
template <> std::optional<std::string> deserializePrimitive<std::string>(const std::string& str);
template <Enum E> std::optional<E> deserializePrimitive(const std::string& str);

std::optional<std::array<std::string, 3>> parsePrimitive(const std::string& data);
std::optional<std::array<std::string, 4>> parseObject(const std::string& data);
std::optional<std::array<std::string, 3>> parsePointer(const std::string& data);
} // namespace string

template <typename T> concept SerializablePrimitive = requires(T t) {
    { string::serializePrimitive(t) } -> std::same_as<std::string>;
    { string::deserializePrimitive<T>("") } -> std::same_as<std::optional<T>>;
};
} // namespace detail

class Serializable {
    friend class detail::SerialPointer; // Allows SerialPointer to access classID for typechecking

  public:
    enum class Result { OK, FILE, STRUCTURE, INTEGRITY, TYPECHECK, POINTER };

    Serializable()                               = default;
    Serializable(const Serializable&)            = default;
    Serializable(Serializable&&)                 = delete;
    Serializable& operator=(const Serializable&) = default;
    Serializable& operator=(Serializable&&)      = delete;
    virtual ~Serializable()                      = default;

    [[nodiscard]] std::pair<Result, std::string> serialize();
    [[nodiscard]] Result deserialize(const std::string& data);
    [[nodiscard]] Result save(const std::filesystem::path& path);
    [[nodiscard]] Result load(const std::filesystem::path& path);

  protected:
    virtual void exposed() = 0;
    virtual unsigned int classID() const;

    template <detail::SerializablePrimitive S> void expose(const std::string& name, S& value);
    void expose(const std::string& name, Serializable& value);
    template <detail::SerializableObject S> void expose(const std::string& name, S*& value);

  private:
    enum class Mode { SERIALIZING, DESERIALIZING };

    Mode mode{};
    Result result{};
    detail::SerialObject serial;
};
} // namespace serializable

namespace serializable {
namespace detail {
inline SerialPrimitive* Serial::asPrimitive() { return dynamic_cast<SerialPrimitive*>(this); }

inline SerialObject* Serial::asObject() { return dynamic_cast<SerialObject*>(this); }

inline SerialPointer* Serial::asPointer() { return dynamic_cast<SerialPointer*>(this); }

inline SerialPrimitive::SerialPrimitive(std::string type, std::string name, std::string value)
    : type(std::move(type)), name(std::move(name)), value(std::move(value)) {}

inline std::string SerialPrimitive::get() const {
    // Return primitive data string
    return string::makeString({ type, " ", name, " = ", value });
}

inline bool SerialPrimitive::set(const std::string& data) {
    // Parse data
    const auto parsed = string::parsePrimitive(data);
    if(!parsed) return false;

    // Apply parsed data
    type  = parsed->at(0);
    name  = parsed->at(1);
    value = parsed->at(2);

    return true;
}

inline std::string SerialPrimitive::getName() const { return name; }

inline std::string SerialPrimitive::getType() const { return type; }

inline std::string SerialPrimitive::getValue() const { return value; }

inline SerialObject::SerialObject(unsigned int classID, std::string name, Address realAddress, Address virtualAddress)
    : name(std::move(name)), classID(classID), realAddress(realAddress), virtualAddress(virtualAddress) {}

inline std::string SerialObject::get() const {
    // Collect children data
    std::vector<std::string> children;
    children.reserve(this->children.size());
    for(const auto& [_, child] : this->children) children.push_back(child->get());

    // Connect and indent children data
    std::string childrenData = string::indent(string::connect(children));

    // Return object data string
    return string::makeString({ "OBJECT<", string::serializePrimitive(classID), "> ", name, " = ",
                                string::serializePrimitive(virtualAddress), " {\n", childrenData, "\n}" });
}

inline bool SerialObject::set(const std::string& data) {
    // Parse data
    const auto parsed = string::parseObject(data);
    if(!parsed) return false;

    // Parse class id and virtual address
    const auto parsedClassID        = string::deserializePrimitive<unsigned int>(parsed->at(0));
    const auto parsedVirtualAddress = string::deserializePrimitive<Address>(parsed->at(2));
    if(!parsedClassID) return false;
    if(!parsedVirtualAddress) return false;

    // Apply parsed data
    classID        = parsedClassID.value();
    name           = parsed->at(1);
    virtualAddress = parsedVirtualAddress.value();
    children.clear();

    // Parse children
    const std::vector<std::string> children = string::split(string::unindent(parsed->at(3)));
    for(const auto& child : children) {
        if(child.starts_with("OBJECT")) {
            auto object = std::make_shared<SerialObject>();
            if(!object->set(child)) return false;
            append(object);
        } else if(child.starts_with("PTR")) {
            auto pointer = std::make_shared<SerialPointer>();
            if(!pointer->set(child)) return false;
            append(pointer);
        } else {
            auto primitive = std::make_shared<SerialPrimitive>();
            if(!primitive->set(child)) return false;
            append(primitive);
        }
    }

    return true;
}

inline std::string SerialObject::getName() const { return name; }

inline void SerialObject::append(const std::shared_ptr<Serial>& child) { children[child->getName()] = child; }

inline std::optional<std::shared_ptr<Serial>> SerialObject::getChild(const std::string& name) const {
    // Check if name exists and return
    if(!children.contains(name)) return std::nullopt;
    return children.at(name);
}

inline unsigned int SerialObject::getClass() const { return classID; }

inline void SerialObject::virtualizeAddresses(std::unordered_map<Address, Address>& addressMap) {
    // Generate own virtual address
    virtualAddress = addressMap.size() + 1;

    // Register in address map
    addressMap[realAddress] = virtualAddress;

    // Apply to all children objects
    for(auto& [_, child] : children) {
        SerialObject* object = child->asObject();
        if(object != nullptr) object->virtualizeAddresses(addressMap);
    }
}

inline void SerialObject::restoreAddresses(std::unordered_map<Address, Address>& addressMap) const {
    // Register real address under virtual address
    addressMap[virtualAddress] = realAddress;

    // Apply to all children objects
    for(const auto& [_, child] : children) {
        SerialObject* object = child->asObject();
        if(object != nullptr) object->restoreAddresses(addressMap);
    }
}

inline bool SerialObject::virtualizePointers(const std::unordered_map<Address, Address>& addressMap) {
    // Apply to all children pointers
    for(const auto& [_, child] : children) {
        SerialPointer* pointer = child->asPointer();
        if(pointer == nullptr) {
            SerialObject* object = child->asObject();
            if(object != nullptr)
                if(!object->virtualizePointers(addressMap)) return false;
        } else if(!pointer->virtualizePointers(addressMap)) return false;
    }

    return true;
}

inline bool SerialObject::restorePointers(const std::unordered_map<Address, Address>& addressMap) {
    // Apply to all children pointers
    for(const auto& [_, child] : children) {
        SerialPointer* pointer = child->asPointer();
        if(pointer == nullptr) {
            SerialObject* object = child->asObject();
            if(object != nullptr)
                if(!object->restorePointers(addressMap)) return false;
        } else if(!pointer->restorePointers(addressMap)) return false;
    }

    return true;
}

inline void SerialObject::setRealAddress(Address address) { realAddress = address; }

inline SerialPointer::SerialPointer(unsigned int classID, std::string name, void** location)
    : name(std::move(name)), classID(classID), location(location) {
    if(location != nullptr) address = std::bit_cast<Address>(*location);
}

inline std::string SerialPointer::get() const {
    // Return pointer data string
    return string::makeString(
      { "PTR<", string::serializePrimitive(classID), "> ", name, " = ", string::serializePrimitive(address) });
}

inline bool SerialPointer::set(const std::string& data) {
    // Parse data
    const auto parsed = string::parsePointer(data);
    if(!parsed) return false;

    // Parse class id and virtual address
    const auto parsedClassID = string::deserializePrimitive<unsigned int>(parsed->at(0));
    const auto parsedAddress = string::deserializePrimitive<Address>(parsed->at(2));
    if(!parsedClassID) return false;
    if(!parsedAddress) return false;

    // Apply parsed data
    classID = parsedClassID.value();
    name    = parsed->at(1);
    address = parsedAddress.value();

    return true;
}

inline std::string SerialPointer::getName() const { return name; }

inline unsigned int SerialPointer::getClass() const { return classID; }

inline bool SerialPointer::virtualizePointers(const std::unordered_map<Address, Address>& addressMap) {
    // Check if address is mapped
    if(!addressMap.contains(address)) return false;

    // Set to new address
    address = addressMap.at(address);

    return true;
}

inline bool SerialPointer::restorePointers(const std::unordered_map<Address, Address>& addressMap) {
    // Check if address is mapped
    if(!addressMap.contains(address)) return false;

    // Set to new address
    address = addressMap.at(address);

    // Check if location is defined
    if(location == nullptr) return false;

    // Update location
    *location = std::bit_cast<void*>(address);

    // Check if classID matches
    return classID == static_cast<Serializable*>(*location)->classID();
}

inline void SerialPointer::setTarget(void** location) { this->location = location; }

namespace string {
inline std::string makeString(const std::initializer_list<std::string>& parts) {
    // Calculate string length
    std::size_t len = 0;
    for(const auto& part : parts) len += part.size();

    // Create new string
    std::string str;
    str.reserve(len);

    // Fill and return string
    for(const auto& part : parts) str.append(part);
    return str;
}

inline std::string substring(const std::string& str, std::size_t start, std::size_t end) {
    // Map (start, end) to (start, size)
    return str.substr(start, end - start);
}

inline std::string replaceAll(const std::string& str, const std::string& from, const std::string& to) {
    // Count matches
    std::size_t matches = 0;
    std::size_t pos     = 0;
    while((pos = str.find(from, pos)) != std::string::npos) {
        matches++;
        pos += from.length();
    }

    // Return original data if no matches are found
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

inline std::string connect(const std::vector<std::string>& lines, char delimiter) {
    // Count characters + newlines
    std::size_t len = lines.size();
    for(const auto& line : lines) len += line.size();

    // Create new string
    std::string str;
    str.reserve(len);

    // Fill and return string
    for(const auto& line : lines) str += line + delimiter;
    str = str.substr(0, str.size() - 1);
    return str;
}

inline std::vector<std::string> split(const std::string& data, char delimiter) {
    // Base case (empty data)
    if(data.empty()) return {};

    // Create vector
    std::vector<std::string> lines;
    lines.reserve(std::count(data.begin(), data.end(), delimiter));

    // Split data (but keep brackets together)
    std::size_t begin = 0;
    std::size_t pos   = 0;
    int level         = 0;
    while(pos < data.size()) {
        if(level > 0) {
            if(data.at(pos) == '}') level--;
            else if(data.at(pos) == '{') level++;
        } else {
            if(data.at(pos) == '{') level++;
            else if(data.at(pos) == delimiter) {
                lines.push_back(substring(data, begin, pos));
                begin = pos + 1;
            }
        }

        pos++;
    }

    if(begin <= pos) lines.push_back(substring(data, begin, pos));

    return lines;
}

inline std::string indent(const std::string& data) {
    // Add a tab to the start and after every newline
    return "\t" + replaceAll(data, "\n", "\n\t");
}

inline std::string unindent(const std::string& data) {
    // Base case: empty data
    if(data.empty()) return "";

    // Remove tab from start and after every newline
    return replaceAll(data.substr(1), "\n\t", "\n");
}

template <> inline std::string serializePrimitive<bool>(const bool& val) { return val ? "true" : "false"; }

template <> inline std::string serializePrimitive<std::string>(const std::string& val) {
    std::string safe = replaceAll(val, "\"", "&quot;");
    safe             = replaceAll(safe, "\n", "&newline;");
    return makeString({ "\"", safe, "\"" });
}

template <Enum E> std::string serializePrimitive(const E& val) {
    return serializePrimitive(static_cast<unsigned int>(val));
}

template <Number N> std::string serializePrimitive(const N& val) { return std::to_string(val); }

template <> inline std::optional<bool> deserializePrimitive<bool>(const std::string& str) {
    if(str == "true") return true;
    if(str == "false") return false;
    return std::nullopt;
}

template <> inline std::optional<char> deserializePrimitive<char>(const std::string& str) {
    const auto val = deserializePrimitive<long>(str);
    if(!val) return std::nullopt;
    if(val < CHAR_MIN || val > CHAR_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<unsigned char> deserializePrimitive<unsigned char>(const std::string& str) {
    const auto val = deserializePrimitive<unsigned long>(str);
    if(!val) return std::nullopt;
    if(val < 0 || val > UCHAR_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<short> deserializePrimitive<short>(const std::string& str) {
    const auto val = deserializePrimitive<long>(str);
    if(!val) return std::nullopt;
    if(val < SHRT_MIN || val > SHRT_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<unsigned short> deserializePrimitive<unsigned short>(const std::string& str) {
    const auto val = deserializePrimitive<unsigned long>(str);
    if(!val) return std::nullopt;
    if(val < 0 || val > USHRT_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<int> deserializePrimitive<int>(const std::string& str) {
    const auto val = deserializePrimitive<long>(str);
    if(!val) return std::nullopt;
    if(val < INT_MIN || val > INT_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<unsigned int> deserializePrimitive<unsigned int>(const std::string& str) {
    const auto val = deserializePrimitive<unsigned long>(str);
    if(!val) return std::nullopt;
    if(val < 0 || val > UINT_MAX) return std::nullopt;
    return val;
}

template <> inline std::optional<long> deserializePrimitive<long>(const std::string& str) {
    try {
        return std::stol(str);
    } catch(const std::exception& e) { return std::nullopt; }
}

template <> inline std::optional<unsigned long> deserializePrimitive<unsigned long>(const std::string& str) {
    if(str.starts_with("-")) return std::nullopt;
    try {
        return std::stoul(str);
    } catch(const std::exception& e) { return std::nullopt; }
}

template <> inline std::optional<float> deserializePrimitive<float>(const std::string& str) {
    try {
        return std::stof(str);
    } catch(const std::exception& e) { return std::nullopt; }
}

template <> inline std::optional<double> deserializePrimitive<double>(const std::string& str) {
    try {
        return std::stod(str);
    } catch(const std::exception& e) { return std::nullopt; }
}

template <> inline std::optional<std::string> deserializePrimitive<std::string>(const std::string& str) {
    if(!str.starts_with('"') || !str.ends_with('"')) return std::nullopt;
    std::string unsafe = substring(str, 1, str.size() - 1);
    unsafe             = replaceAll(unsafe, "&newline;", "\n");
    return replaceAll(unsafe, "&quot;", "\"");
}

template <Enum E> inline std::optional<E> deserializePrimitive(const std::string& str) {
    const auto number = deserializePrimitive<unsigned int>(str);
    if(number) return static_cast<E>(number.value());
    return std::nullopt;
}

// Pattern: TYPE NAME = VALUE, Returns: (type, name, value)
inline std::optional<std::array<std::string, 3>> parsePrimitive(const std::string& data) {
    // Find fixed points
    const std::size_t space  = data.find(' ');
    const std::size_t equals = data.find('=', space + 1);

    // Check fixed points
    if(space == std::string::npos) return std::nullopt;
    if(equals == std::string::npos) return std::nullopt;

    // Extract sections
    std::string type  = substring(data, 0, space);
    std::string name  = substring(data, space + 1, equals - 1);
    std::string value = substring(data, equals + 2, data.size());

    // Validate sections
    if(type.empty()) return std::nullopt;
    if(value.empty()) return std::nullopt;

    return std::array{ type, name, value };
}

// Pattern: OBJECT<CLASS> NAME = ADDRESS {\nCHILDREN\n}, Returns: (class, name, address, children)
inline std::optional<std::array<std::string, 4>> parseObject(const std::string& data) {
    // Find fixed points
    const std::size_t space   = data.find(' ');
    const std::size_t equals  = data.find('=', space + 1);
    const std::size_t opening = data.find('{', equals + 1);

    // Check fixed points
    if(space == std::string::npos) return std::nullopt;
    if(equals == std::string::npos) return std::nullopt;
    if(opening == std::string::npos) return std::nullopt;

    // Extract sections
    std::string classID  = substring(data, 7, space - 1);
    std::string name     = substring(data, space + 1, equals - 1);
    std::string address  = substring(data, equals + 2, opening - 1);
    std::string children = substring(data, opening + 2, data.size() - 2);

    // Validate sections
    if(classID.empty()) return std::nullopt;
    if(address.empty()) return std::nullopt;

    return std::array{ classID, name, address, children };
}

// Pattern: PTR<CLASS> NAME = ADDRESS, Returns: (class, name, address)
inline std::optional<std::array<std::string, 3>> parsePointer(const std::string& data) {
    // Find fixed points
    const std::size_t space  = data.find(' ');
    const std::size_t equals = data.find('=', space + 1);

    // Check fixed points
    if(space == std::string::npos) return std::nullopt;
    if(equals == std::string::npos) return std::nullopt;

    // Extract sections
    std::string classID = substring(data, 4, space - 1);
    std::string name    = substring(data, space + 1, equals - 1);
    std::string address = substring(data, equals + 2, data.size());

    // Validate sections
    if(classID.empty()) return std::nullopt;
    if(address.empty()) return std::nullopt;

    return std::array{ classID, name, address };
}
} // namespace string
} // namespace detail

inline std::pair<Serializable::Result, std::string> Serializable::serialize() {
    // Setup serialization state
    mode   = Mode::SERIALIZING;
    result = Result::OK;
    serial = { classID(), "root", std::bit_cast<detail::Address>(this), 0 };

    // Run exposers
    exposed();
    if(result != Result::OK) return { result, "" };

    // Virtualize addresses
    std::unordered_map<detail::Address, detail::Address> addressMap;
    serial.virtualizeAddresses(addressMap);
    if(!serial.virtualizePointers(addressMap)) return { Result::POINTER, "" };

    return { result, serial.get() };
}

inline Serializable::Result Serializable::deserialize(const std::string& data) {
    // Setup deserialization state
    mode   = Mode::DESERIALIZING;
    result = Result::OK;

    // Parse serialized data
    if(!serial.set(data)) return Result::STRUCTURE;

    // Check root object class id
    if(serial.getClass() != classID()) return Result::TYPECHECK;

    // Run exposers
    exposed();
    if(result != Result::OK) return result;

    // Restore addresses
    std::unordered_map<detail::Address, detail::Address> addressMap;
    serial.setRealAddress(std::bit_cast<detail::Address>(this));
    serial.restoreAddresses(addressMap);
    if(!serial.restorePointers(addressMap)) return Result::POINTER;

    return result;
}

inline Serializable::Result Serializable::save(const std::filesystem::path& path) {
    // Create parent path
    if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

    // Open and check file
    std::ofstream stream(path);
    if(!stream) return Result::FILE;

    // Serialize data
    const auto [result, serialized] = serialize();
    if(result != Result::OK) return result;

    // Write serial data
    stream << serialized;
    stream.close();

    // Finish
    return Result::OK;
}

inline Serializable::Result Serializable::load(const std::filesystem::path& path) {
    // Open and check file
    const std::ifstream stream(path);
    if(!stream) return Result::FILE;

    // Read serialized data
    std::stringstream str;
    str << stream.rdbuf();

    // Deserialize data
    return deserialize(str.str());
}

inline unsigned int Serializable::classID() const { return 0; }

template <detail::SerializablePrimitive S> void Serializable::expose(const std::string& name, S& value) {
    // Abort if a previous error was detected
    if(result != Result::OK) return;

    if(mode == Mode::SERIALIZING) {
        // Append new serial primitive object to root
        serial.append(std::make_shared<detail::SerialPrimitive>(detail::string::TypeToString<S>, name,
                                                                detail::string::serializePrimitive(value)));
    } else {
        // Find serial value in root object
        const auto serialValue = serial.getChild(name);
        if(!serialValue) {
            result = Result::INTEGRITY;
            return;
        }

        // Convert to serial primitive
        const auto* serialPrimitive = serialValue.value()->asPrimitive();
        if(serialPrimitive == nullptr) {
            result = Result::TYPECHECK;
            return;
        }

        // Check primitive type
        if(serialPrimitive->getType() != detail::string::TypeToString<S>) {
            result = Result::TYPECHECK;
            return;
        }

        // Get primitive value
        const auto primitiveValue = detail::string::deserializePrimitive<S>(serialPrimitive->getValue());
        if(!primitiveValue) {
            result = Result::TYPECHECK;
            return;
        }

        // Set value
        value = primitiveValue.value();
    }
}

inline void Serializable::expose(const std::string& name, Serializable& value) {
    // Abort if previous error was detected
    if(result != Result::OK) return;

    if(mode == Mode::SERIALIZING) {
        // Serialize object
        value.mode   = Mode::SERIALIZING;
        value.result = Result::OK;
        value.serial = { value.classID(), name, std::bit_cast<detail::Address>(&value), 0 };
        value.exposed();

        // Take result
        if(value.result != result) {
            result = value.result;
            return;
        }

        // Append serial object to root
        serial.append(std::make_shared<detail::SerialObject>(value.serial));
    } else {
        // Find serial value in root object
        const auto serialValue = serial.getChild(name);
        if(!serialValue) {
            result = Result::INTEGRITY;
            return;
        }

        // Convert to serial object
        auto* serialObject = serialValue.value()->asObject();
        if(serialObject == nullptr) {
            result = Result::TYPECHECK;
            return;
        }

        // Check class id
        if(serialObject->getClass() != value.classID()) {
            result = Result::TYPECHECK;
            return;
        }

        // Set objects real address
        serialObject->setRealAddress(std::bit_cast<detail::Address>(&value));

        // Deserialize object
        value.mode   = Mode::DESERIALIZING;
        value.result = Result::OK;
        value.serial = *serialObject;
        value.exposed();

        // Take result
        if(value.result != result) {
            result = value.result;
            return;
        }
    }
}

template <detail::SerializableObject S> void Serializable::expose(const std::string& name, S*& value) {
    // Abort if previous error was detected
    if(result != Result::OK) return;

    // Abort if value is a nullptr
    if(value == nullptr) {
        result = Result::POINTER;
        return;
    }

    // Get address from value
    void** address = std::bit_cast<void**>(&value);

    if(mode == Mode::SERIALIZING) {
        // Append new serial pointer object to root
        serial.append(std::make_shared<detail::SerialPointer>(value->classID(), name, address));
    } else {
        // Find serial value in root object
        const auto serialValue = serial.getChild(name);
        if(!serialValue) {
            result = Result::INTEGRITY;
            return;
        }

        // Convert to serial pointer
        auto* serialPointer = serialValue.value()->asPointer();
        if(serialPointer == nullptr) {
            result = Result::TYPECHECK;
            return;
        }

        // Check pointer type
        if(serialPointer->getClass() != value->classID()) {
            result = Result::TYPECHECK;
            return;
        }

        // Register location for address mapping
        serialPointer->setTarget(address);
    }
}
} // namespace serializable

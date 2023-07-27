#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace serializable {
class Serializable {
  public:
    Serializable()                               = default;
    Serializable(const Serializable&)            = default;
    Serializable(Serializable&&)                 = delete;
    Serializable& operator=(const Serializable&) = default;
    Serializable& operator=(Serializable&&)      = delete;
    virtual ~Serializable()                      = default;

    inline std::string serialize() {
        // Serialize data
        serialized.clear();
        state = State::WRITING;
        exposed();
        state = State::IDLE;

        // Return serialized string
        return serialized;
    }

    inline bool check(const std::string& data) {
        // Ser serialized string
        serialized = data;

        // Perform check
        return check();
    }

    inline bool deserialize(const std::string& data) {
        // Set serialized string
        serialized = data;

        // Check data
        if(!check()) return false;

        // Deserialize data
        state = State::READING;
        exposed();
        state = State::IDLE;

        // Finished
        return true;
    }

    inline bool load(const std::filesystem::path& path) {
        // Open and check file
        const std::ifstream file(path);
        if(!file) return false;

        // Read serialized data
        std::stringstream stream;
        stream << file.rdbuf();

        // Deserialize data
        return deserialize(stream.str());
    }

    inline bool save(const std::filesystem::path& path) {
        // Create parent path
        if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

        // Open and check file
        std::ofstream file(path);
        if(!file) return false;

        // Write serialized data
        file << serialize();

        // Finish
        return true;
    }

  protected:
    virtual void exposed() = 0;

    inline void expose(const char* name, bool& value) {
        expose<bool>(
            "BOOL", name, value, [](const bool& b) { return std::string(b ? "true" : "false"); },
            [](const std::string& str) { return str == "true"; }, false);
    }

    inline void expose(const char* name, char& value) {
        expose<char>(
            "CHAR", name, value, [](const char& c) { return std::to_string(c); },
            [](const std::string& str) { return static_cast<char>(std::stoi(str)); }, 0);
    }

    inline void expose(const char* name, unsigned char& value) {
        expose<unsigned char>(
            "UCHAR", name, value, [](const unsigned char& c) { return std::to_string(c); },
            [](const std::string& str) { return static_cast<unsigned char>(std::stoul(str)); }, 0);
    }

    inline void expose(const char* name, short& value) {
        expose<short>(
            "SHORT", name, value, [](const short& s) { return std::to_string(s); },
            [](const std::string& str) { return static_cast<short>(std::stoi(str)); }, 0);
    }

    inline void expose(const char* name, unsigned short& value) {
        expose<unsigned short>(
            "USHORT", name, value, [](const unsigned short& s) { return std::to_string(s); },
            [](const std::string& str) { return static_cast<unsigned short>(std::stoul(str)); }, 0);
    }

    inline void expose(const char* name, int& value) {
        expose<int>(
            "INT", name, value, [](const int& i) { return std::to_string(i); },
            [](const std::string& str) { return std::stoi(str); }, 0);
    }

    inline void expose(const char* name, unsigned int& value) {
        expose<unsigned int>(
            "UINT", name, value, [](const unsigned int& i) { return std::to_string(i); },
            [](const std::string& str) { return static_cast<unsigned int>(std::stoul(str)); }, 0);
    }

    inline void expose(const char* name, long& value) {
        expose<long>(
            "LONG", name, value, [](const long& i) { return std::to_string(i); },
            [](const std::string& str) { return std::stol(str); }, 0);
    }

    inline void expose(const char* name, unsigned long& value) {
        expose<unsigned long>(
            "ULONG", name, value, [](const unsigned long& i) { return std::to_string(i); },
            [](const std::string& str) { return std::stoul(str); }, 0);
    }

    inline void expose(const char* name, float& value) {
        expose<float>(
            "FLOAT", name, value, [](const float& f) { return std::to_string(f); },
            [](const std::string& str) { return std::stof(str); }, 0);
    }

    inline void expose(const char* name, double& value) {
        expose<double>(
            "DOUBLE", name, value, [](const double& d) { return std::to_string(d); },
            [](const std::string& str) { return std::stod(str); }, 0);
    }

    inline void expose(const char* name, std::string& value) {
        // Special string serializer
        const auto serialize = [](const std::string& s) {
            std::string str = std::regex_replace(s, std::regex("\""), "&quot;"); // Replace all quotes by &quot;
            str             = "\"" + str + "\"";                                 // Wrap string in quotes
            return str;
        };

        // Special string deserializer
        const auto deserialize = [](const std::string& s) {
            std::string str = s.substr(1, s.size() - 2);                           // Take off quotes
            str             = std::regex_replace(str, std::regex("&quot;"), "\""); // Replace &quot; by quotes
            return str;
        };

        // Expose
        expose<std::string>("STRING", name, value, serialize, deserialize, "");
    }

    inline void expose(const char* name, Serializable& value) {
        // Pack serialized data as child of another
        const auto packObject = [](const std::string& data) {
            if(data.empty()) return std::string("{}"); // Base case

            std::string str = "\n" + data.substr(0, data.size() - 1); // Add newline at the front, remove final newline
            str             = std::regex_replace(str, std::regex("\n"), "\n\t"); // Indent every line
            str             = "{" + str + "\n}";                                 // Wrap in brackets

            return str;
        };

        // Unpack previously packed object into serialized data
        const auto unpackObject = [](const std::string& data) {
            if(data == "{}") return std::string(); // Base case

            std::string str = data.substr(1, data.size() - 2); // Remove brackets (final newline is not read)
            str             = std::regex_replace(str, std::regex("\n\t"), "\n"); // Remove one indentation per line
            str             = str.substr(1, str.size() - 1) + "\n"; // Remove first newline, add newline to the end

            return str;
        };

        // Get object prefix
        const std::string prefix = std::string("OBJECT ").append(name).append(" = ");

        // Default checking behaviour
        if(state == State::CHECKING) {
            if(auto match = find(prefix)) good &= typecheck(value, match.value().substr(prefix.size()));
            else good = false;
        }

        // If writing: Write value
        else if(state == State::WRITING)
            serialized.append(prefix).append(packObject(value.serialize())).append("\n");

        // If reading: Read into value
        else if(state == State::READING)
            if(const auto match = find(prefix, true))
                value.deserialize(unpackObject(match.value().substr(prefix.size())));
    }

    template <typename T>
        requires std::is_enum_v<T>
    inline void expose(const char* name, T& value) {
        expose<T>(
            "ENUM", name, value, [](const T& t) { return std::to_string(static_cast<int>(t)); },
            [](const std::string& str) { return static_cast<T>(std::stoi(str)); }, value);
    }

  private:
    enum class State { IDLE, CHECKING, READING, WRITING } state{};
    bool good{};
    std::string serialized;

    inline bool check() {
        // Check data
        good  = true;
        state = State::CHECKING;
        exposed();
        state = State::IDLE;

        // Return result
        return good;
    }

    template <typename T>
    inline void expose(const char* type, const char* name, T& value, std::string (*serialize)(const T& t),
                       T (*deserialize)(const std::string& str), const T& fallback) {
        const std::string prefix = std::string(type).append(" ").append(name).append(" = ");

        // If checking: See if value is available
        if(state == State::CHECKING) {
            if(auto match = find(prefix)) good &= typecheck(type, match.value().substr(prefix.size()), deserialize);
            else good = false;
        }

        // If reading: Try to find the value, otherwise use fallback
        else if(state == State::READING) {
            if(const auto match = find(prefix, true)) value = deserialize(match.value().substr(prefix.size()));
            else value = fallback;
        }

        // If writing: Append value to serialized data
        else if(state == State::WRITING)
            serialized.append(prefix).append(serialize(value)).append("\n");
    }

    inline std::optional<std::string> find(const std::string& prefix, bool remove = false) {
        // Find start of object
        const std::size_t begin = serialized.find(prefix);
        if(begin == std::string::npos) return std::nullopt;

        // Find end of object
        bool inString = false;
        int depth     = 0;
        for(std::size_t pos = begin; pos < serialized.size(); pos++) {
            const char c = serialized.at(pos);

            // Finish if end is found
            if(c == '\n' && depth == 0 && !inString) {
                // If data should not be removed: Return value
                if(!remove) return serialized.substr(begin, pos - begin);

                // Erase data and return value
                std::string result = serialized.substr(begin, pos - begin);
                serialized.erase(begin, pos - begin + 1);
                return result;
            }

            // Ignore everything inside string literals
            if(c == '"') inString = !inString;
            if(inString) continue;

            // Increment depth
            if(c == '{') depth++;
            else if(c == '}') depth--;
        }

        // Failed to find end of object
        return std::nullopt;
    }

    template <typename T>
    inline static bool tryDeserialize(const std::string& value, T (*deserialize)(const std::string& str)) {
        try {
            deserialize(value);
            return true;
        } catch(std::exception& ex) { return false; }
    }

    template <typename T>
    inline static bool typecheck(const char* type, const std::string& value, T (*deserialize)(const std::string& str)) {
        const constexpr std::array UNSIGNED{"UCHAR", "USHORT", "UINT", "ULONG", "ENUM"};
        const constexpr std::array SIGNED{"CHAR", "SHORT", "INT", "LONG"};
        const constexpr std::array FLOATING{"FLOAT", "DOUBLE"};

        if(std::find(UNSIGNED.begin(), UNSIGNED.end(), type) != UNSIGNED.end())
            return std::regex_match(value, std::regex(R"(\d+)")) && tryDeserialize(value, deserialize);

        if(std::find(SIGNED.begin(), SIGNED.end(), type) != SIGNED.end())
            return std::regex_match(value, std::regex(R"(-?\d+)")) && tryDeserialize(value, deserialize);

        if(std::find(FLOATING.begin(), FLOATING.end(), type) != FLOATING.end())
            return std::regex_match(value, std::regex(R"(-?\d+(\.\d+)?)")) && tryDeserialize(value, deserialize);

        if(std::strcmp(type, "BOOL") == 0) return std::regex_match(value, std::regex("true|false"));
        if(std::strcmp(type, "STRING") == 0) return std::regex_match(value, std::regex(R"("[^"]*")"));

        return false;
    }

    inline static bool typecheck(Serializable& object, const std::string& data) { return object.check(data); };
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
};
} // namespace serializable

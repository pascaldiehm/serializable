#include "serializable.hpp"
#include <cstddef>
#include <cstdlib>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

void assert(bool value, const char* message) {
    if(value) return;
    std::cerr << "[Failed] " << message << "\n";
    std::exit(1); // NOLINT(concurrency-mt-unsafe)
}

std::string buildSerial(const std::vector<const char*>& fields) {
    std::string result;
    for(const auto& field : fields) result = result.append(field).append("\n");
    if(!result.empty()) result.pop_back();
    return result;
}

void shuffle(std::vector<const char*>& vec) {
    auto swap = [&](std::size_t i, std::size_t j) {
        const auto* temp = vec.at(i);
        vec.at(i)        = vec.at(j);
        vec.at(j)        = temp;
    };

    auto random = [] { return static_cast<float>(rand()) / RAND_MAX; }; // NOLINT(cert-msc*-c*,concurrency-mt-unsafe)

    while(random() < 0.95) {
        int i = static_cast<int>(random() * static_cast<float>(vec.size()));
        int j = static_cast<int>(random() * static_cast<float>(vec.size()));
        swap(i, j);
    }
}

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
void testSerialization() {
    struct : serializable::Serializable {
        int i = 42;
        void exposed() override { expose("i", i); }
    } a;
    assert(a.serialize() == buildSerial({"INT i = 42"}), "Serialize single field");

    struct : serializable::Serializable {
        long time           = 1677572053;
        int primes          = 2357;
        bool hasToken       = true;
        unsigned char token = 42;
        void exposed() override {
            expose("time", time);
            expose("Primes", primes);
            expose("Has Token", hasToken);
            expose("Token", token);
        }
    } b;
    assert(b.serialize() == buildSerial({"LONG time = 1677572053", "INT Primes = 2357", "BOOL Has Token = true",
                                         "UCHAR Token = 42"}),
           "Serialize multiple fields");

    struct : serializable::Serializable {
        std::string str = "Hello,\nworld!";
        void exposed() override { expose("str", str); }
    } c;
    assert(c.serialize() == buildSerial({"STRING str = \"Hello,", "world!\""}), "Serialize multi line strings");
}

void testDeserialization() {
    struct : serializable::Serializable {
        int i = 42;
        void exposed() override { expose("i", i); }
    } a, copy_a;
    a.i = 13;
    copy_a.deserialize(buildSerial({"INT i = 13"}));
    assert(a.serialize() == copy_a.serialize(), "Deserialize single field");

    struct : serializable::Serializable {
        long time           = 1677572053;
        int primes          = 2357;
        bool hasToken       = true;
        unsigned char token = 42;
        std::string my_str  = "Hello,\nworld!";
        void exposed() override {
            expose("time", time);
            expose("Primes", primes);
            expose("Has Token", hasToken);
            expose("Token", token);
            expose("String", my_str);
        }
    } b, copy_b;
    std::vector<const char*> serial{"LONG time = 1677572053", "INT Primes = 2357",        "BOOL Has Token = true",
                                    "UCHAR Token = 42",       "STRING String = \"Hello,", "world!\""};

    copy_b.deserialize(buildSerial(serial));
    assert(b.serialize() == copy_b.serialize(), "Deserialize multiple fields");

    shuffle(serial);
    copy_b.deserialize(buildSerial(serial));
    assert(b.serialize() == copy_b.serialize(), "Deserialize shuffled data");
}

void testErrorDetection() {
    struct : serializable::Serializable {
        int i = 42;
        void exposed() override { expose("i", i); }
    } a;
    assert(!a.check(""), "Check empty string");
    assert(!a.check("\n"), "Check pseudo-empty string");
    assert(!a.check(buildSerial({"LONG l = 42"})), "Check wrong data");
    assert(a.check(buildSerial({"INT i = 10"})), "Check correct data");
    assert(a.check(buildSerial({"INT i = 13", "LONG l = 43"})), "Check superset data");
    assert(a.check(buildSerial({"Hakuna matata?", "INT i = 13", "NOT_A_TOKEN"})), "Check dirty data");
    assert(!a.check(buildSerial({"INT i = wuff"})), "Check invalid value");

    struct : serializable::Serializable {
        bool b{true};
        int i{-1};
        unsigned int u{5};
        double d{1.123};
        std::string str;
        void exposed() override {
            expose("b", b);
            expose("i", i);
            expose("u", u);
            expose("d", d);
            expose("str", str);
        }
    } b;
    assert(b.check(buildSerial(
               {"BOOL b = true", "INT i = -1", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = "my-str")"})),
           "Typecheck correct");
    assert(!b.check(buildSerial(
               {"BOOL b = maybe", "INT i = -1", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = "my-str")"})),
           "Typecheck invalid bool");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = 0xAF", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = "my-str")"})),
           "Typecheck invalid int (1)");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = -1.23", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = "my-str")"})),
           "Typecheck invalid int (2)");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = -1", "UINT u = -5", "DOUBLE d = 1.123", R"(STRING str = "my-str")"})),
           "Typecheck invalid uint");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = -1", "UINT u = 5", "DOUBLE d = 1,123", R"(STRING str = "my-str")"})),
           "Typecheck invalid double");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = -1", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = "my-"str")"})),
           "Typecheck invalid string (1)");
    assert(!b.check(buildSerial(
               {"BOOL b = true", "INT i = -1", "UINT u = 5", "DOUBLE d = 1.123", R"(STRING str = my-str)"})),
           "Typecheck invalid string (2)");
    assert(!b.check(buildSerial({"BOOL b = true", "INT i = -1", "UINT u = 5", "", R"(STRING str = my-str)"})),
           "Typecheck missing value");
}

void testChains() {
    struct Struct : public serializable::Serializable {
        bool b{};
        char c{};
        unsigned char uc{};
        short s{};
        unsigned short us{};
        int i{};
        unsigned int ui{};
        long l{};
        unsigned long ul{};
        float f{};
        double d{};
        std::string str{};

        Struct() = default;

        Struct(bool b, char c, unsigned char uc, short s, unsigned short us, int i, unsigned int ui, long l,
               unsigned long ul, float f, double d, const char* str)
            : b(b), c(c), uc(uc), s(s), us(us), i(i), ui(ui), l(l), ul(ul), f(f), d(d), str(str) {}

        bool operator==(const Struct& a) const {
            return b == a.b && c == a.c && uc == a.uc && s == a.s && us == a.us && i == a.i && ui == a.ui && l == a.l &&
                   ul == a.ul && std::abs(a.f - f) < 0.1 && std::abs(a.d - d) < 0.1 && str == a.str;
        }

        void exposed() override {
            expose("Boolean", b);
            expose("Character", c);
            expose("Unsigned Character", uc);
            expose("Short Number", s);
            expose("Unsigned Short Number", us);
            expose("Number", i);
            expose("Unsigned Number", ui);
            expose("Long Number", l);
            expose("Unsigned Long Number", ul);
            expose("Floating Point Number", f);
            expose("Double Floating Point Number", d);
            expose("String", str);
        }
    };

    Struct base(true, 'P', 'D', 123, 456, -789, 2357, -123456789, 123456789, 3.14, 2.718281828, "serialization");
    assert(base.check(base.serialize()), "Serialize-Check-Chain failed");

    Struct copy;
    copy.deserialize(base.serialize());
    assert(copy == base, "Serialize-Deserialize-Chain failed");
}

void testNesting() {
    struct : serializable::Serializable {
        struct : serializable::Serializable {
            struct : serializable::Serializable {
                std::string str;
                void exposed() override { expose("str", str); }
            } hex, oct;
            int number{};
            bool prime{};
            void exposed() override {
                expose("Number", number);
                expose("Is Prime", prime);
                expose("Hexadecimal", hex);
                expose("Octal", oct);
            }
        } child1, child2, child3, child4;
        struct : serializable::Serializable {
            void exposed() override {}
        } empty;
        void exposed() override {
            expose("Child 1", child1);
            expose("Child 2", child2);
            expose("Child 3", child3);
            expose("Child 4", child4);
            expose("Empty Test", empty);
        }
    } master, master_copy;

    master.child1.number  = 42;
    master.child1.hex.str = (std::stringstream() << std::hex << 42).str();
    master.child1.oct.str = (std::stringstream() << std::oct << 42).str();

    master.child2.number  = 10;
    master.child2.hex.str = (std::stringstream() << std::hex << 10).str();
    master.child2.oct.str = (std::stringstream() << std::oct << 10).str();

    master.child3.number  = 13;
    master.child3.prime   = true;
    master.child3.hex.str = (std::stringstream() << std::hex << 13).str();
    master.child3.oct.str = (std::stringstream() << std::oct << 13).str();

    master.child4.number  = 123;
    master.child4.hex.str = (std::stringstream() << std::hex << 123).str();
    master.child4.oct.str = (std::stringstream() << std::oct << 123).str();

    assert(master_copy.check(master.serialize()), "Nested Serialize-Check-Chain");
    master_copy.deserialize(master.serialize());
    assert(master.serialize() == master_copy.serialize(), "Nested Serialize-Deserialize-Chain");
}

void testComplexTypes() {
    enum class MyEnum { ABC, DEF, XYZ };
    struct : serializable::Serializable {
        MyEnum my_enum{}, my_other_enum{};
        void exposed() override {
            expose("my_enum", my_enum);
            expose("other", my_other_enum);
        }
    } testEnum, testEnumCopy;

    testEnum.my_enum       = MyEnum::DEF;
    testEnum.my_other_enum = MyEnum::XYZ;
    assert(testEnum.check(testEnum.serialize()), "Enum Serialize-Check-Chain");

    testEnumCopy.deserialize(testEnum.serialize());
    assert(testEnum.serialize() == testEnumCopy.serialize(), "Enum Serialize-Deserialize-Chain");

    serializable::Array<int, 3> array;
    serializable::Array<int, 3> arrayCopy;

    array[0] = 42;
    array[1] = 10;
    array[2] = -1;
    assert(array.check(array.serialize()), "Array Serialize-Check-Chain");

    arrayCopy.deserialize(array.serialize());
    assert(array == arrayCopy, "Array Serialize-Deserialize-Chain");

    serializable::Vector<int> vector;
    serializable::Vector<int> vectorCopy;

    vector.push_back(42);
    vector.push_back(10);
    vector.push_back(-1);
    vector.push_back(123);
    vector.push_back(3);
    assert(vector.check(vector.serialize()), "Vector Serialize-Check-Chain");

    vectorCopy.deserialize(vector.serialize());
    assert(vector == vectorCopy, "Vector Serialize-Deserialize-Chain");

    serializable::List<std::string> list;
    serializable::List<std::string> listCopy;

    list.push_back("C++");
    list.push_back("TypeScript");
    list.push_back("Python3");
    list.push_back("Java");
    list.push_back("Lua");
    list.push_back("PHP");
    list.push_back("C#");
    assert(list.check(list.serialize()), "List Serialize-Check-Chain");

    listCopy.deserialize(list.serialize());
    assert(list == listCopy, "List Serialize-Deserialize-Chain");
}
// NOLINTEND(misc-non-private-member-variables-in-classes)

int main() {
    testSerialization();
    testDeserialization();
    testErrorDetection();
    testChains();
    testNesting();
    testComplexTypes();
    std::cout << "All tests passed!\n";
    return 0;
}

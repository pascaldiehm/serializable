#include "serializable.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <ratio>
#include <string>

using Result = serializable::Serializable::Result;

// NOLINTBEGIN(*non-private*)
class Simple : public serializable::Serializable {
  public:
    int i = 42;
    void exposed() override { expose("i", i); }
    unsigned int classID() const override { return 42; }
};

class AllTypes : public serializable::Serializable {
  public:
    enum class Enum { ABC, DEF, XYZ };
    bool b            = true;
    short s           = -1;
    unsigned short us = 2;
    int i             = -3;
    unsigned int ui   = 4;
    long l            = -5;
    unsigned long ul  = 6;
    // Float types aren't easily verifiable, therefore left out for now
    std::string str = "Hello, world!";
    Enum myEnum     = Enum::DEF;
    void exposed() override {
        expose("b", b);
        expose("s", s);
        expose("us", us);
        expose("i", i);
        expose("ui", ui);
        expose("l", l);
        expose("ul", ul);
        expose("str", str);
        expose("My Enum", myEnum);
    }
};

class Nested : public serializable::Serializable {
  public:
    Simple s1, s2;
    serializable::Vector<serializable::Array<int, 3>> triples;
    void exposed() override {
        expose("S1", s1);
        expose("S2", s2);
        expose("Pythagorean Triples", triples);
    }
    static serializable::Array<int, 3> makeTriple(int i, int j, int k) {
        serializable::Array<int, 3> arr;
        arr[0] = i;
        arr[1] = j;
        arr[2] = k;
        return arr;
    }
};

class Pointing : public serializable::Serializable {
  public:
    Simple simple, other_simple, not_exposed, *current{};
    Pointing* ptr{};
    void exposed() override {
        expose("Simple", simple);
        expose("Other", other_simple);
        expose("Current", current);
        expose("Mad", ptr);
    }
    unsigned int classID() const override { return 123; }
};

class Named : public serializable::Serializable {
  public:
    const char* name{};
    int i{};
    void exposed() override { expose(name, i); }
};
// NOLINTEND(*non-private*)

void assert(bool condition, const char* message) {
    if(condition) return;
    std::cerr << "[Failed] " << message << "\n";
    std::exit(1); // NOLINT(concurrency-mt-unsafe)
}

void assertEqual(const std::string& s, const std::string& t, const char* message) {
    if(s == t) return;
    std::cout << s << "\n>DIFFERS<\n" << t << "\n";
    assert(false, message);
}

void assertOk(Result result, const char* message) {
    switch(result) {
    case serializable::Serializable::Result::OK: return;
    case serializable::Serializable::Result::FILE: std::cerr << "I/O Error\n"; break;
    case serializable::Serializable::Result::STRUCTURE: std::cerr << "Syntax Error\n"; break;
    case serializable::Serializable::Result::TYPECHECK: std::cerr << "Type Error\n"; break;
    case serializable::Serializable::Result::INTEGRITY: std::cerr << "Integrity Error\n"; break;
    case serializable::Serializable::Result::POINTER: std::cerr << "Pointer Error\n"; break;
    }
    assert(false, message);
}

void testSerialize() {
    {
        Simple simple;
        auto [result, serial] = simple.serialize();
        assertOk(result, "Serialize: Simple");
        assertEqual(serial, "OBJECT ROOT = 0 (42) {\n\tINT i = 42\n}", "Serialize: Simple");
    }

    {
        AllTypes allTypes;
        auto [result, serial] = allTypes.serialize();
        assertOk(result, "Serialize: AllTypes");
        assertEqual(
            serial,
            "OBJECT ROOT = 0 (0) {\n\tENUM My Enum = 1\n\tULONG ul = 6\n\tINT i = -3\n\tUINT ui = 4\n\tUSHORT us = "
            "2\n\tLONG l = -5\n\tSHORT s = -1\n\tSTRING str = \"Hello, world!\"\n\tBOOL b = true\n}",
            "Serialize: AllTypes");
    }

    {
        serializable::Vector<int> vec;
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        auto [result, serial] = vec.serialize();
        assertOk(result, "Serialize: Vector");
        assertEqual(serial, "OBJECT ROOT = 0 (2) {\n\tINT 2 = 30\n\tINT 1 = 20\n\tINT 0 = 10\n\tULONG length = 3\n}",
                    "Serialize: Vector");
    }
}

void testDeserialize() {
    Result result{};

    Simple simple;
    result = simple.deserialize("OBJECT ROOT = 0 (42) {\n\tINT i = -42\n}");
    assertOk(result, "Deserialize: Simple");
    assert(simple.i == -42, "Deserialize: Simple (i)");

    result = simple.deserialize("OBJECT not_root = 0 (42) {\n\tINT i = 13\n}");
    assertOk(result, "Deserialize: Renamed root object");
    assert(simple.i == 13, "Deserialize: Renamed root object (i)");

    result = simple.deserialize("OBJECT ROOT = 0 (42) {\n\tINT i = 42\n}\n");
    assertOk(result, "Deserialize: Extra newline");
    assert(simple.i == 42, "Deserialize: Extra newline (i)");

    AllTypes allTypes;
    result = allTypes.deserialize(
        "OBJECT ROOT = 0 (0) {\n\tBOOL b = false\n\tSHORT s = -3\n\tUSHORT us = 6\n\tINT i = -9\n\tUINT ui = "
        "12\n\tLONG l = -15\n\tULONG ul = 18\n\tSTRING str = \"Bye bye!\"\n\tENUM My Enum = 2\n}");
    assertOk(result, "Deserialize: AllTypes");
    assert(!allTypes.b, "Deserialize: AllTypes (b)");
    assert(allTypes.s == -3, "Deserialize: AllTypes (s)");
    assert(allTypes.us == 6, "Deserialize: AllTypes (us)");
    assert(allTypes.i == -9, "Deserialize: AllTypes (i)");
    assert(allTypes.ui == 12, "Deserialize: AllTypes (ui)");
    assert(allTypes.l == -15, "Deserialize: AllTypes (l)");
    assert(allTypes.ul == 18, "Deserialize: AllTypes (ul)");
    assert(allTypes.str == "Bye bye!", "Deserialize: AllTypes (str)");
    assert(allTypes.myEnum == AllTypes::Enum::XYZ, "Deserialize: AllTypes (myEnum)");

    serializable::Vector<int> vec;
    result = vec.deserialize("OBJECT ROOT = 0 (2) {\n\tULONG length = 3\n\tINT 0 = 10\n\tINT 1 = 20\n\tINT 2 = 30\n}");
    assertOk(result, "Deserialize: Vector");
    assert(vec.size() == 3, "Deserialize: Vector (length)");
    assert(vec[0] == 10, "Deserialize: Vector (0)");
    assert(vec[1] == 20, "Deserialize: Vector (1)");
    assert(vec[2] == 30, "Deserialize: Vector (2)");
}

void testNested() {
    Nested original;
    original.s1.i = 13;
    original.s2.i = -42;
    original.triples.push_back(Nested::makeTriple(3, 4, 5));
    original.triples.push_back(Nested::makeTriple(5, 12, 13));
    original.triples.push_back(Nested::makeTriple(8, 15, 17));
    original.triples.push_back(Nested::makeTriple(7, 24, 25));
    original.triples.push_back(Nested::makeTriple(20, 21, 29));
    original.triples.push_back(Nested::makeTriple(12, 35, 37));
    original.triples.push_back(Nested::makeTriple(9, 40, 41));
    original.triples.push_back(Nested::makeTriple(28, 45, 53));
    original.triples.push_back(Nested::makeTriple(11, 60, 61));
    original.triples.push_back(Nested::makeTriple(16, 63, 65));
    original.triples.push_back(Nested::makeTriple(33, 56, 65));
    original.triples.push_back(Nested::makeTriple(48, 55, 73));
    original.triples.push_back(Nested::makeTriple(13, 84, 85));
    original.triples.push_back(Nested::makeTriple(36, 77, 85));
    original.triples.push_back(Nested::makeTriple(39, 80, 89));
    original.triples.push_back(Nested::makeTriple(65, 72, 97));
    auto [result, serial] = original.serialize();
    assertOk(result, "Nested: Serialization");

    Nested copy;
    result = copy.deserialize(serial);

    assertOk(result, "Nested: Deserialization");
    assert(copy.s1.i == original.s1.i, "Nested: s1");
    assert(copy.s2.i == original.s2.i, "Nested: s2");
    assert(copy.triples == original.triples, "Nested: triples");
}

void testPointers() {
    {
        Pointing ptr;
        ptr.simple.i       = 42;
        ptr.other_simple.i = 123;
        ptr.current        = &ptr.simple;
        ptr.ptr            = &ptr;

        auto [result, serial] = ptr.serialize();
        assertOk(result, "Pointers: Serialize");
    }
}

void testErrorDetection() {
    Simple simple;
    Pointing pointing;

    assert(simple.load("FILE DOES NOT EXIST") == Result::FILE, "Error Detection: FILE");

    assert(simple.deserialize("Not a save file") == Result::STRUCTURE, "Error Detection: STRUCTURE (1)");
    assert(simple.deserialize("") == Result::STRUCTURE, "Error Detection: STRUCTURE (2)");
    assert(simple.deserialize("\n") == Result::STRUCTURE, "Error Detection: STRUCTURE (3)");
    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n\tINT i = NaN\n}") == Result::STRUCTURE,
           "Error Detection: STRUCTURE (4)");
    assert(simple.deserialize("OBJECT ROOT = 0 (42) {}") == Result::STRUCTURE, "Error Detection: Structure (5)");
    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n\tINT i = 1\n}\nExtra data") == Result::STRUCTURE,
           "Error Detection: Structure (6)");

    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n\tINT i = 4294967296\n}") == Result::TYPECHECK,
           "Error Detection: TYPECHECK (1)");
    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n\tUINT i = 123\n}") == Result::TYPECHECK,
           "Error Detection: TYPECHECK (2)");
    assert(simple.deserialize("OBJECT ROOT = 0 (0) {\n\tUINT i = 123\n}") == Result::TYPECHECK,
           "Error Detection: TYPECHECK (3)");

    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n\tUINT ui = 123\n}") == Result::INTEGRITY,
           "Error Detection: INTEGRITY (1)");
    assert(simple.deserialize("OBJECT ROOT = 0 (42) {\n}") == Result::INTEGRITY, "Error Detection: INTEGRITY (2)");

    assert(pointing.serialize().first == Result::POINTER, "Error Detection: Nullptr");

    pointing.ptr     = &pointing;
    pointing.current = &pointing.not_exposed;
    assert(pointing.serialize().first == Result::POINTER, "Error Detection: Pointer to unexposed");

    assert(
        pointing.deserialize(
            "OBJECT ROOT = 0 (123) {\n\tPTR Current = 3 (0)\n\tPTR Mad = 0 (0)\n\tOBJECT Other = 2 (42) {\n\t\tINT i "
            "= 123\n\t}\n\tOBJECT Simple = 1 (42) {\n\t\tINT i = 42\n\t}\n}") == Result::TYPECHECK,
        "Error Detection: Invalid pointer ID");

    assert(pointing.deserialize("OBJECT ROOT = 0 (123) {\n\tPTR Current = 3 (42)\n\tPTR Mad = 0 (123)\n\tOBJECT Other "
                                "= 2 (42) {\n\t\tINT i "
                                "= 123\n\t}\n\tOBJECT Simple = 1 (42) {\n\t\tINT i = 42\n\t}\n}") == Result::POINTER,
           "Error Detection: Invalid virtual address");

    assert(pointing.deserialize("OBJECT ROOT = 0 (123) {\n\tPTR Current = 1 (42)\n\tPTR Mad = 0 (123)\n\tOBJECT Other "
                                "= 1 (42) {\n\t\tINT i "
                                "= 123\n\t}\n\tOBJECT Simple = 2 (42) {\n\t\tINT i = 42\n\t}\n}") == Result::OK,
           "Error Detection: MAD pointer");
}

void testNames() {
    Named named;

    {
        named.i               = 42;
        named.name            = "basic";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize basic");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize basic");
        assert(named.i == 42, "Named: Check basic (i)");
    }
    {
        named.i               = 42;
        named.name            = "with space";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize with space");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize with space");
        assert(named.i == 42, "Named: Check with space (i)");
    }
    {
        named.i               = 42;
        named.name            = "  with more spaces ";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize with more spaces");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize with more spaces");
        assert(named.i == 42, "Named: Check with more spaces (i)");
    }
    {
        named.i               = 42;
        named.name            = "n@n-ständárd characters!\t";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize n@n-ständárd characters!\t");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize n@n-ständárd characters!\t");
        assert(named.i == 42, "Named: Check n@n-ständárd characters!\t (i)");
    }
    {
        named.i               = 42;
        named.name            = "{containing} \"delimiters\"";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize {containing} \"delimiters\"");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize {containing} \"delimiters\"");
        assert(named.i == 42, "Named: Check {containing} \"delimiters\" (i)");
    }
    {
        named.i               = 42;
        named.name            = " ";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize just a space");

        named.i = 0;
        result  = named.deserialize(serial);
        assertOk(result, "Named: Deserialize just a space");
        assert(named.i == 42, "Named: Check just a space (i)");
    }
    {
        named.i               = 42;
        named.name            = "containing\nnewlines";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize containing\nnewlines");

        named.i = 13;
        result  = named.deserialize(serial);
        assert(result == Result::STRUCTURE, "Named: Deserialize containing\nnewlines");
        assert(named.i == 13, "Named: Check containing\nnewlines (i)");
    }
    {
        named.i               = 42;
        named.name            = "";
        auto [result, serial] = named.serialize();
        assertOk(result, "Named: Serialize no name");

        named.i = 13;
        result  = named.deserialize(serial);
        assert(result == Result::STRUCTURE, "Named: Deserialize no name");
        assert(named.i == 13, "Named: Check no name (i)");
    }
}

void testSpeed() {
    // Setup constant definitions
    using clock = std::chrono::steady_clock;
    using ms    = std::chrono::microseconds;
    using std::chrono::duration_cast;

    const constexpr int ITERATIONS = 1;
    const constexpr int ELEMENTS   = 1000000;

    // Create tracking arrays
    std::array<long, ITERATIONS> serializeTime{};
    std::array<long, ITERATIONS> deserializeTime{};

    // Create array
    serializable::Array<int, ELEMENTS> arr;
    for(int element = 0; element < ELEMENTS; element++) arr[element] = element + 1;

    // Run tests
    for(int iteration = 0; iteration < ITERATIONS; iteration++) {
        // Record serializing time
        auto startSerialize            = clock::now();
        auto [resultSerialize, serial] = arr.serialize();
        serializeTime.at(iteration)    = duration_cast<ms>(clock::now() - startSerialize).count();
        assertOk(resultSerialize, "Speed test: Serialize");

        // Record deserializing time
        auto startDeserialize         = clock::now();
        auto resultDeserialize        = arr.deserialize(serial);
        deserializeTime.at(iteration) = duration_cast<ms>(clock::now() - startDeserialize).count();
        assertOk(resultDeserialize, "Speed test: Deserialize");
    }

    // Calculate result
    double serializeResult = 0;
    for(const auto& time : serializeTime) serializeResult += static_cast<double>(time);
    serializeResult /= ITERATIONS;

    double deserializeResult = 0;
    for(const auto& time : deserializeTime) deserializeResult += static_cast<double>(time);
    deserializeResult /= ITERATIONS;

    // Print result
    std::cout << std::fixed << std::setprecision(2) << "Serializing took on average " << serializeResult
              << " microseconds\nDeserializing took on average " << deserializeResult << " microseconds\n";
}

int main() {
    testSerialize();
    testDeserialize();
    testNested();
    testPointers();
    testErrorDetection();
    testNames();
    testSpeed();
    std::cout << "All tests passed!\n";
}

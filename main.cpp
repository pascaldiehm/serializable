#include "serializable.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <climits>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// NOLINTBEGIN(*-non-private-*)

// Assert
template <typename T> concept Printable = requires(T t) { std::cout << t; };

void assert(bool b, const char* message) {
    if(b) return;
    std::cout << "[FAILED] "
              << ": " << message << '\n';
}

template <typename T, typename U> void assertEqual(const T& expected, const U& actual, const char* message) {
    assert(expected == actual, message);
}

template <Printable T, Printable U> void assertEqual(const T& expected, const U& actual, const char* message) {
    if(expected == actual) return; // NOLINT(*-decay)
    assert(false, message);
    // NOLINTNEXTLINE(*-decay)
    std::cout << "### EXPECTED ###\n" << expected << "\n### ACTUAL ###\n" << actual << "\n### END ###\n";
}

template <serializable::detail::Enum T, serializable::detail::Enum U>
void assertEqual(const T& expected, const U& actual, const char* message) {
    assertEqual(static_cast<int>(expected), static_cast<int>(actual), message);
}

// String manipulation
void testStringManipulation() {
    namespace str = serializable::detail::string;

    // Test str::makeString
    assertEqual("Hello, World!", str::makeString("Hello", ", ", "World", "!"), "str::makeString");
    assertEqual("Hello, World!", str::makeString(std::string("Hello"), ", ", std::string("World"), "!"),
                "str::makeString (std::string)");

    // Test str::substring
    assertEqual("Hello", str::substring("Hello, World!", 0, 5), "str::substring (from start)");
    assertEqual("World", str::substring("Hello, World!", 7, 12), "str::substring (from middle)");
    assertEqual("!", str::substring("Hello, World!", 12, 13), "str::substring (from end)");

    // Test str::replaceAll
    assertEqual("Hello, World!", str::replaceAll("Hello, World!", "!", "!"), "str::replaceAll (no change)");
    assertEqual("Hell_, W_rld!", str::replaceAll("Hello, World!", "o", "_"), "str::replaceAll (same size)");
    assertEqual("Hell, Wrld!", str::replaceAll("Hello, World!", "o", ""), "str::replaceAll (smaller)");
    assertEqual("Helloo, Woorld!", str::replaceAll("Hello, World!", "o", "oo"), "str::replaceAll (larger)");

    // Test str::connect
    assertEqual("", str::connect({}), "str::connect (empty)");
    assertEqual("", str::connect({ "" }), "str::connect (empty multiline)");
    assertEqual("\n", str::connect({ "", "" }), "str::connect (empty multiline)");
    assertEqual("ABC", str::connect({ "ABC" }), "str::connect (single)");
    assertEqual("ABC\nDEF", str::connect({ "ABC", "DEF" }), "str::connect (multiple)");
    assertEqual("ABC-DEF-XYZ", str::connect({ "ABC", "DEF", "XYZ" }, '-'), "str::connect (with separator)");

    // Test str::split
    assertEqual(std::vector<std::string>{ "" }, str::split(""), "str::split (empty)");
    assertEqual(std::vector<std::string>{ "", "" }, str::split("\n"), "str::split (empty multiline)");
    assertEqual(std::vector<std::string>{ "ABC" }, str::split("ABC"), "str::split (single)");
    assertEqual(std::vector<std::string>{ "ABC", "DEF" }, str::split("ABC\nDEF"), "str::split (multiple)");
    assertEqual(std::vector<std::string>{ "ABC", "DEF", "XYZ" }, str::split("ABC-DEF-XYZ", '-'),
                "str::split (with separator)");

    // Test str::indent
    assertEqual("\t", str::indent(""), "str::indent (empty)");
    assertEqual("\t\n\t", str::indent("\n"), "str::indent (empty multiline)");
    assertEqual("\tABC", str::indent("ABC"), "str::indent (single)");
    assertEqual("\tABC\n\tDEF", str::indent("ABC\nDEF"), "str::indent (multiple)");

    // Test str::unindent
    assertEqual("", str::unindent(""), "str::unindent (empty)");
    assertEqual("", str::unindent("\t"), "str::unindent (single tab)");
    assertEqual("\t", str::unindent("\t\t"), "str::unindent (multiple tabs)");
    assertEqual("", str::unindent("\n"), "str::unindent (empty multiline)");
    assertEqual("\n", str::unindent("\t\n\t"), "str::unindent (single tab multiline)");
    assertEqual("\n\t", str::unindent("\t\n\t\t"), "str::unindent (multiple tabs multiline)");
    assertEqual("ABC", str::unindent("\tABC"), "str::unindent (single)");
    assertEqual("ABC\nDEF", str::unindent("\tABC\n\tDEF"), "str::unindent (multiple)");
}

void testPrimitiveConversions() {
    namespace str            = serializable::detail::string;
    const constexpr auto NaN = std::nullopt;
    enum class Enum { ABC, DEF, XYZ };

    // Test str::serializePrimitive(bool)
    assertEqual("true", str::serializePrimitive(true), "serialize bool (true)");
    assertEqual("false", str::serializePrimitive(false), "serialize bool (false)");

    // Test str::deserializePrimitive<bool>()
    assertEqual(true, str::deserializePrimitive<bool>("true"), "deserialize bool (true)");
    assertEqual(false, str::deserializePrimitive<bool>("false"), "deserialize bool (false)");

    // Test str::serializePrimitive(char)
    assertEqual("42", str::serializePrimitive<char>(42), "serialize char (positive)");
    assertEqual("-42", str::serializePrimitive<char>(-42), "serialize char (negative)");

    // Test str::deserializePrimitive<char>()
    assertEqual(42, str::deserializePrimitive<char>("42"), "deserialize char (positive)");
    assertEqual(-42, str::deserializePrimitive<char>("-42"), "deserialize char (negative)");
    assertEqual(NaN, str::deserializePrimitive<char>("forty-two"), "deserialize char (invalid)");
    assertEqual(NaN, str::deserializePrimitive<char>(std::to_string(CHAR_MIN - 1)), "deserialize char (underflow)");
    assertEqual(NaN, str::deserializePrimitive<char>(std::to_string(CHAR_MAX + 1)), "deserialize char (overflow)");

    // Test str::serializePrimitive(unsigned char)
    assertEqual("42", str::serializePrimitive<unsigned char>(42), "serialize uchar (positive)");

    // Test str::deserializePrimitive<unsigned char>()
    assertEqual(42, str::deserializePrimitive<unsigned char>("42"), "deserialize uchar (positive)");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>("-42"), "deserialize uchar (negative)");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>("forty-two"), "deserialize uchar (invalid)");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>(std::to_string(UCHAR_MAX + 1)),
                "deserialize uchar (overflow)");

    // Test str::serializePrimitive(short)
    assertEqual("42", str::serializePrimitive<short>(42), "serialize short (positive)");
    assertEqual("-42", str::serializePrimitive<short>(-42), "serialize short (negative)");

    // Test str::deserializePrimitive<short>()
    assertEqual(42, str::deserializePrimitive<short>("42"), "deserialize short (positive)");
    assertEqual(-42, str::deserializePrimitive<short>("-42"), "deserialize short (negative)");
    assertEqual(NaN, str::deserializePrimitive<short>("forty-two"), "deserialize short (invalid)");
    assertEqual(NaN, str::deserializePrimitive<short>(std::to_string(SHRT_MIN - 1)), "deserialize short (underflow)");
    assertEqual(NaN, str::deserializePrimitive<short>(std::to_string(SHRT_MAX + 1)), "deserialize short (overflow)");

    // Test str::serializePrimitive(unsigned short)
    assertEqual("42", str::serializePrimitive<unsigned short>(42), "serialize ushort (positive)");

    // Test str::deserializePrimitive<unsigned short>()
    assertEqual(42, str::deserializePrimitive<unsigned short>("42"), "deserialize ushort (positive)");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>("-42"), "deserialize ushort (negative)");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>("forty-two"), "deserialize ushort (invalid)");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>(std::to_string(USHRT_MAX + 1)),
                "deserialize ushort (overflow)");

    // Test str::serializePrimitive(int)
    assertEqual("42", str::serializePrimitive<int>(42), "serialize int (positive)");
    assertEqual("-42", str::serializePrimitive<int>(-42), "serialize int (negative)");

    // Test str::deserializePrimitive<int>()
    assertEqual(42, str::deserializePrimitive<int>("42"), "deserialize int (positive)");
    assertEqual(-42, str::deserializePrimitive<int>("-42"), "deserialize int (negative)");
    assertEqual(NaN, str::deserializePrimitive<int>("forty-two"), "deserialize int (invalid)");
    assertEqual(NaN, str::deserializePrimitive<int>(std::to_string(INT_MIN - 1L)), "deserialize int (underflow)");
    assertEqual(NaN, str::deserializePrimitive<int>(std::to_string(INT_MAX + 1L)), "deserialize int (overflow)");

    // Test str::serializePrimitive(unsigned int)
    assertEqual("42", str::serializePrimitive<unsigned int>(42), "serialize uint (positive)");

    // Test str::deserializePrimitive<unsigned int>()
    assertEqual(42, str::deserializePrimitive<unsigned int>("42"), "deserialize uint (positive)");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>("-42"), "deserialize uint (negative)");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>("forty-two"), "deserialize uint (invalid)");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>(std::to_string(UINT_MAX + 1UL)),
                "deserialize uint (overflow)");

    // Test str::serializePrimitive(long)
    assertEqual("42", str::serializePrimitive<long>(42), "serialize long (positive)");
    assertEqual("-42", str::serializePrimitive<long>(-42), "serialize long (negative)");

    // Test str::deserializePrimitive<long>()
    assertEqual(42, str::deserializePrimitive<long>("42"), "deserialize long (positive)");
    assertEqual(-42, str::deserializePrimitive<long>("-42"), "deserialize long (negative)");
    assertEqual(NaN, str::deserializePrimitive<long>("forty-two"), "deserialize long (invalid)");
    assertEqual(NaN, str::deserializePrimitive<long>("-9999999999999999999"), "deserialize long (underflow)");
    assertEqual(NaN, str::deserializePrimitive<long>("9999999999999999999"), "deserialize long (overflow)");

    // Test str::serializePrimitive(unsigned long)
    assertEqual("42", str::serializePrimitive<unsigned long>(42), "serialize ulong (positive)");

    // Test str::deserializePrimitive<unsigned long>()
    assertEqual(42, str::deserializePrimitive<unsigned long>("42"), "deserialize ulong (positive)");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>("-42"), "deserialize ulong (negative)");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>("forty-two"), "deserialize ulong (invalid)");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>("99999999999999999999"), "deserialize ulong (overflow)");

    // Test str::serializePrimitive(float)
    assertEqual("3.141590", str::serializePrimitive<float>(3.14159), "serialize float (positive)");
    assertEqual("-3.141590", str::serializePrimitive<float>(-3.14159), "serialize float (negative)");

    // Test str::deserializePrimitive<float>()
    assert(std::abs(3.14159 - str::deserializePrimitive<float>("3.14159").value_or(0)) < 0.0001,
           "deserialize float (positive)");
    assert(std::abs(-3.14159 - str::deserializePrimitive<float>("-3.14159").value_or(0)) < 0.0001,
           "deserialize float (negative)");
    assertEqual(NaN, str::deserializePrimitive<float>("forty-two"), "deserialize float (invalid)");

    // Test str::serializePrimitive(double)
    assertEqual("3.141590", str::serializePrimitive<double>(3.14159), "serialize double (positive)");
    assertEqual("-3.141590", str::serializePrimitive<double>(-3.14159), "serialize double (negative)");

    // Test str::deserializePrimitive<double>()
    assert(std::abs(3.14159 - str::deserializePrimitive<double>("3.14159").value_or(0)) < 0.0001,
           "deserialize double (positive)");
    assert(std::abs(-3.14159 - str::deserializePrimitive<double>("-3.14159").value_or(0)) < 0.0001,
           "deserialize double (negative)");
    assertEqual(NaN, str::deserializePrimitive<double>("forty-two"), "deserialize double (invalid)");

    // Test str::serializePrimitive(string)
    assertEqual("\"Hello, world!\"", str::serializePrimitive<std::string>("Hello, world!"),
                "serialize string (simple)");
    assertEqual("\"&quot;Hello!&quot;&newline;\"", str::serializePrimitive<std::string>("\"Hello!\"\n"),
                "serialize string (complex)");

    // Test str::deserializePrimitive<string>()
    assertEqual("Hello, world!", str::deserializePrimitive<std::string>("\"Hello, world!\""),
                "deserialize string (simple)");
    assertEqual("\"Hello!\"\n", str::deserializePrimitive<std::string>("\"&quot;Hello!&quot;&newline;\""),
                "deserialize string (complex)");
    assertEqual(NaN, str::deserializePrimitive<std::string>("123"), "deserialize string (invalid)");

    // Test str::serializePrimitive(Enum)
    assertEqual("1", str::serializePrimitive<Enum>(Enum::DEF), "serialize Enum (DEF)");

    // Test str::deserializePrimitive<Enum>()
    assertEqual(Enum::DEF, str::deserializePrimitive<Enum>("1"), "deserialize Enum (DEF)");
    assertEqual(NaN, str::deserializePrimitive<Enum>("ABC"), "deserialize Enum (invalid)");
    assertEqual(static_cast<Enum>(4), str::deserializePrimitive<Enum>("4"), "deserialize Enum (out-of-range)");
}

void testParsers() {
    namespace str = serializable::detail::string;

    // Test str::parsePrimitive
    auto primitive = str::parsePrimitive("BOOL my_bool = true");
    if(primitive) {
        assertEqual("BOOL", primitive->at(0), "parsePrimitive 1 (type)");
        assertEqual("my_bool", primitive->at(1), "parsePrimitive 1 (name)");
        assertEqual("true", primitive->at(2), "parsePrimitive 1 (value)");
    } else assert(false, "parsePrimitive 1");

    primitive = str::parsePrimitive("STRING my_string = \"Hello, world!\"");
    if(primitive) {
        assertEqual("STRING", primitive->at(0), "parsePrimitive 2 (type)");
        assertEqual("my_string", primitive->at(1), "parsePrimitive 2 (name)");
        assertEqual("\"Hello, world!\"", primitive->at(2), "parsePrimitive 2 (value)");
    } else assert(false, "parsePrimitive 2");

    primitive = str::parsePrimitive("FLOAT my_int = 3.14159");
    if(primitive) {
        assertEqual("FLOAT", primitive->at(0), "parsePrimitive 3 (type)");
        assertEqual("my_int", primitive->at(1), "parsePrimitive 3 (name)");
        assertEqual("3.14159", primitive->at(2), "parsePrimitive 3 (value)");
    } else assert(false, "parsePrimitive 3");

    // Test str::parseObject
    auto object = str::parseObject("OBJECT<0> root = 1 {}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject 1 (class)");
        assertEqual("root", object->at(1), "parseObject 1 (name)");
        assertEqual("1", object->at(2), "parseObject 1 (address)");
        assertEqual("", object->at(3), "parseObject 1 (children)");
    } else assert(false, "parseObject 1");

    object = str::parseObject("OBJECT<0> root = 1 {\n\t\n}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject 2 (class)");
        assertEqual("root", object->at(1), "parseObject 2 (name)");
        assertEqual("1", object->at(2), "parseObject 2 (address)");
        assertEqual("\t", object->at(3), "parseObject 2 (children)");
    } else assert(false, "parseObject 2");

    object = str::parseObject("OBJECT<0> root = 1 {\n\tINT answer = 42\n\tFLOAT PI = 3.14159\n}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject 3 (class)");
        assertEqual("root", object->at(1), "parseObject 3 (name)");
        assertEqual("1", object->at(2), "parseObject 3 (address)");
        assertEqual("\tINT answer = 42\n\tFLOAT PI = 3.14159", object->at(3), "parseObject 3 (children)");
    } else assert(false, "parseObject 3");

    object = str::parseObject("OBJECT<0> root = 1 {\n\tOBJECT<1> child = 2 {\n\t\tINT answer = 42\n\t}\n}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject 4 (class)");
        assertEqual("root", object->at(1), "parseObject 4 (name)");
        assertEqual("1", object->at(2), "parseObject 4 (address)");
        assertEqual("\tOBJECT<1> child = 2 {\n\t\tINT answer = 42\n\t}", object->at(3), "parseObject 4 (children)");
    } else assert(false, "parseObject 4");

    // Test str::parsePointer
    auto pointer = str::parsePointer("PTR<8> my_pointer = 42");
    if(pointer) {
        assertEqual("8", pointer->at(0), "parsePointer 1 (class)");
        assertEqual("my_pointer", pointer->at(1), "parsePointer 1 (name)");
        assertEqual("42", pointer->at(2), "parsePointer 1 (address)");
    } else assert(false, "parsePointer 1");
}

// Serial types
void testSerialPrimitive() {
    using serializable::detail::SerialPrimitive;

    const SerialPrimitive source("INT", "my_int", "42");
    assertEqual("INT my_int = 42", source.get(), "SerialPrimitive::get()");

    SerialPrimitive target;
    assert(target.set(source.get()), "SerialPrimitive::set()");
    assertEqual(source.get(), target.get(), "SerialPrimitive::get()");
}

void testSerialObject() {
    using serializable::detail::SerialObject;
    using serializable::detail::SerialPrimitive;

    SerialObject source(0, "root", 0, 0);
    source.append(std::make_shared<SerialPrimitive>("INT", "answer", "42"));
    source.append(std::make_shared<SerialPrimitive>("FLOAT", "PI", "3.14159"));
    source.append(std::make_shared<SerialPrimitive>("BOOL", "my_bool", "true"));
    {
        auto pos = std::make_shared<SerialObject>(1, "pos", 0, 0);
        pos->append(std::make_shared<SerialPrimitive>("INT", "x", "1"));
        pos->append(std::make_shared<SerialPrimitive>("INT", "y", "4"));
        source.append(pos);
    }

    SerialObject target;
    assert(target.set(source.get()), "SerialObject::set()");

    assertEqual(0U, target.getClass(), "SerialObject::getClass()");
    assertEqual("root", target.getName(), "SerialObject::getName()");

    auto child = target.getChild("answer");
    if(child) assertEqual("INT answer = 42", child.value()->get(), "SerialObject::getChild() (answer)");
    else assert(false, "SerialObject::getChild() (answer)");

    child = target.getChild("PI");
    if(child) assertEqual("FLOAT PI = 3.14159", child.value()->get(), "SerialObject::getChild() (PI)");
    else assert(false, "SerialObject::getChild() (PI)");

    child = target.getChild("my_bool");
    if(child) assertEqual("BOOL my_bool = true", child.value()->get(), "SerialObject::getChild() (my_bool)");
    else assert(false, "SerialObject::getChild() (my_bool)");

    child = target.getChild("pos");
    if(child) {
        auto* pos = child.value()->asObject();

        auto sub = pos->getChild("x");
        if(sub) assertEqual("INT x = 1", sub.value()->get(), "SerialObject::getChild() (pos.x)");
        else assert(false, "SerialObject::getChild() (pos.x)");

        sub = pos->getChild("y");
        if(sub) assertEqual("INT y = 4", sub.value()->get(), "SerialObject::getChild() (pos.y)");
        else assert(false, "SerialObject::getChild() (pos.y)");
    } else assert(false, "SerialObject::getChild() (pos)");
}

void testSerialPointer() {
    using serializable::detail::SerialPointer;

    unsigned long data = 123;
    const SerialPointer source(42, "my_pointer", std::bit_cast<void**>(&data));
    assertEqual("PTR<42> my_pointer = 123", source.get(), "SerialPointer::get()");

    SerialPointer target;
    assert(target.set(source.get()), "SerialPointer::set()");
    assertEqual(source.get(), target.get(), "SerialPointer::get()");
}

// Basic
struct Basic : public serializable::Serializable {
    int value = 0;

    Basic() = default;

    explicit Basic(int value) : value(value) {}

    void exposed() override { expose("value", value); }
};

void testBasic() {
    Basic source(42);

    const auto serial = source.serialize();
    assertEqual(Basic::Result::OK, serial.first, "Basic::serialize() (result)");
    assertEqual("OBJECT<0> root = 1 {\n\tINT value = 42\n}", serial.second, "Basic::serialize() (data)");

    Basic target;
    assertEqual(Basic::Result::OK, target.deserialize(serial.second), "Basic::deserialize() (result)");
    assertEqual(source.value, target.value, "Basic::deserialize() (value)");
}

// All Types
struct AllTypes : public serializable::Serializable {
    enum class Enum { ABC, DEF, XYZ };

    bool b            = false;
    char c            = '\0';
    unsigned char uc  = '\0';
    short s           = 0;
    unsigned short us = 0;
    int i             = 0;
    unsigned int ui   = 0;
    long l            = 0;
    unsigned long ul  = 0;
    float f           = 0.0F;
    double d          = 0.0;
    std::string str;
    Enum e      = Enum::ABC;
    AllTypes* p = nullptr;
    std::array<int, 2> arr{};
    std::vector<int> vec{};

    AllTypes() : p(this){};

    AllTypes(bool b, char c, unsigned char uc, short s, unsigned short us, int i, unsigned int ui, long l,
             unsigned long ul, float f, double d, std::string str, Enum e, std::array<int, 2> arr, std::vector<int> vec)
        : b(b), c(c), uc(uc), s(s), us(us), i(i), ui(ui), l(l), ul(ul), f(f), d(d), str(std::move(str)), e(e), p(this),
          arr(arr), vec(std::move(vec)) {}

    void exposed() override {
        expose("b", b);
        expose("c", c);
        expose("uc", uc);
        expose("s", s);
        expose("us", us);
        expose("i", i);
        expose("ui", ui);
        expose("l", l);
        expose("ul", ul);
        expose("f", f);
        expose("d", d);
        expose("str", str);
        expose("e", e);
        expose("p", p);
        expose("arr", arr);
        expose("vec", vec);
    }

    unsigned int classID() const override { return 1; }
};

void testAllTypes() {
    AllTypes source(true, 'a', 'b', 1, 2, 3, 4, 5, 6, 7.0F, 8.0, "Hello World", AllTypes::Enum::XYZ, { 9, 10 },
                    { 11, 12 });
    const auto serial = source.serialize();
    assertEqual(AllTypes::Result::OK, serial.first, "AllTypes::serialize() (result)");

    AllTypes target;
    assertEqual(AllTypes::Result::OK, target.deserialize(serial.second), "AllTypes::deserialize() (result)");
    assertEqual(source.b, target.b, "AllTypes::deserialize() (b)");
    assertEqual(source.c, target.c, "AllTypes::deserialize() (c)");
    assertEqual(source.uc, target.uc, "AllTypes::deserialize() (uc)");
    assertEqual(source.s, target.s, "AllTypes::deserialize() (s)");
    assertEqual(source.us, target.us, "AllTypes::deserialize() (us)");
    assertEqual(source.i, target.i, "AllTypes::deserialize() (i)");
    assertEqual(source.ui, target.ui, "AllTypes::deserialize() (ui)");
    assertEqual(source.l, target.l, "AllTypes::deserialize() (l)");
    assertEqual(source.ul, target.ul, "AllTypes::deserialize() (ul)");
    assertEqual(source.f, target.f, "AllTypes::deserialize() (f)");
    assertEqual(source.d, target.d, "AllTypes::deserialize() (d)");
    assertEqual(source.str, target.str, "AllTypes::deserialize() (str)");
    assertEqual(source.e, target.e, "AllTypes::deserialize() (e)");
    assertEqual(&target, target.p, "AllTypes::deserialize() (p)");
    assertEqual(source.arr, target.arr, "AllTypes::deserialize() (arr)");
    assertEqual(source.vec, target.vec, "AllTypes::deserialize() (vec)");

    // Serialize nullptr
    source.p = nullptr;
    assertEqual(AllTypes::Result::POINTER, source.serialize().first, "AllTypes::serialize() (nullptr)");

    // Deserialize invalid pointer
    const auto tampered = serializable::detail::string::replaceAll(serial.second, "PTR<1> p = 1", "PTR<1> p = 42");
    assertEqual(AllTypes::Result::POINTER, target.deserialize(tampered), "AllTypes::deserialize() (invalid pointer)");
}

// Nested
struct Nested : public serializable::Serializable {
    Basic primary, secondary;

    Nested() = default;

    explicit Nested(int value) : primary(value), secondary(value) {}

    Nested(int primary, int secondary) : primary(primary), secondary(secondary) {}

    void exposed() override {
        expose("primary", primary);
        expose("secondary", secondary);
    }
};

void testNested() {
    Nested source(42, 24);

    const auto serial = source.serialize();
    assertEqual(Nested::Result::OK, serial.first, "Nested::serialize() (result)");

    Nested target;
    assertEqual(Nested::Result::OK, target.deserialize(serial.second), "Nested::deserialize() (result)");
    assertEqual(source.primary.value, target.primary.value, "Nested::deserialize() (primary)");
    assertEqual(source.secondary.value, target.secondary.value, "Nested::deserialize() (secondary)");
}

// Files
void testFiles() {
    Basic source(42);
    assertEqual(Basic::Result::OK, source.save("test.txt"), "Basic::save()");
    assertEqual(Basic::Result::OK, source.save("test.txt"), "Basic::save() (overwrite)");

    Basic target;
    assertEqual(Basic::Result::OK, target.load("test.txt"), "Basic::load()");
    assertEqual(source.value, target.value, "Basic::load() (value)");

    assertEqual(Basic::Result::FILE, target.load("non-existent.txt"), "Basic::load() (non-existent)");
}

// Errors
struct Errors : public serializable::Serializable {
    std::string name, value;

    Errors() = default;

    Errors(std::string name, std::string value) : name(std::move(name)), value(std::move(value)) {}

    void exposed() override { expose(name, value); }

    unsigned int classID() const override { return 2; }
};

void testErrors() {
    Errors errors("name", "value");

    // Wrong class id
    auto res = errors.deserialize("OBJECT<0> root = 1 {\n\tSTRING name = \"value\"\n}");
    assertEqual(Errors::Result::TYPECHECK, res, "error (wrong class id)");

    // Missing value
    res = errors.deserialize("OBJECT<2> root = 1 {\n\t\n}");
    assertEqual(Errors::Result::INTEGRITY, res, "error (missing value)");

    // Too many children
    res = errors.deserialize("OBJECT<2> root = 1 {\n\tSTRING name = \"value\"\n\tSTRING value = \"value\"\n}");
    assertEqual(Errors::Result::OK, res, "error (too many children)");

    // Wrong type
    res = errors.deserialize("OBJECT<2> root = 1 {\n\tINT name = 1\n}");
    assertEqual(Errors::Result::TYPECHECK, res, "error (wrong type)");

    // Empty string
    res = errors.deserialize("");
    assertEqual(Errors::Result::STRUCTURE, res, "error (empty string)");

    // JSON format
    res = errors.deserialize("{\n\t\"name\": \"value\"\n}");
    assertEqual(Errors::Result::STRUCTURE, res, "error (JSON format)");

    // Wrong value type
    res = errors.deserialize("OBJECT<2> root = 1 {\n\tSTRING name = 123\n}");
    assertEqual(Errors::Result::TYPECHECK, res, "error (wrong value type)");

    // Newline at end
    res = errors.deserialize("OBJECT<2> root = 1 {\n\tSTRING name = \"value\"\n}\n");
    assertEqual(Errors::Result::OK, res, "error (newline at end)");

    // Newline in middle
    res = errors.deserialize("OBJECT<2> root = 1 {\n\tSTRING name = \"value\"\n\t\n}");
    assertEqual(Errors::Result::OK, res, "error (newline in middle)");

    // Name with space
    errors.name  = "name with space";
    errors.value = "value";
    auto serial  = errors.serialize();
    assertEqual(Errors::Result::OK, serial.first, "error (name with space)");
    assertEqual("OBJECT<2> root = 1 {\n\tSTRING " + errors.name + " = \"value\"\n}", serial.second,
                "error (name with space)");
    assertEqual(Errors::Result::OK, errors.deserialize(serial.second), "error (name with space)");

    // Name with other funny characters
    errors.name = "name with other funny characters: !@#$%^&*(){}_+|:\"<>?`-[]\\;',./";
    serial      = errors.serialize();
    assertEqual(Errors::Result::OK, serial.first, "error (name with other funny characters)");
    assertEqual("OBJECT<2> root = 1 {\n\tSTRING " + errors.name + " = \"value\"\n}", serial.second,
                "error (name with other funny characters)");
    assertEqual(Errors::Result::OK, errors.deserialize(serial.second), "error (name with other funny characters)");

    // Name starting with primitive identifier
    errors.name = "INT name";
    serial      = errors.serialize();
    assertEqual(Errors::Result::OK, serial.first, "error (name starting with primitive identifier)");
    assertEqual("OBJECT<2> root = 1 {\n\tSTRING " + errors.name + " = \"value\"\n}", serial.second,
                "error (name starting with primitive identifier)");
    assertEqual(Errors::Result::OK, errors.deserialize(serial.second),
                "error (name starting with primitive identifier)");

    // No name
    errors.name = "";
    serial      = errors.serialize();
    assertEqual(Errors::Result::OK, serial.first, "error (no name)");
    assertEqual("OBJECT<2> root = 1 {\n\tSTRING " + errors.name + " = \"value\"\n}", serial.second, "error (no name)");
    assertEqual(Errors::Result::OK, errors.deserialize(serial.second), "error (no name)");
}

// Stress
struct Stress : public serializable::Serializable {
    template <serializable::detail::SerializablePrimitive T> struct Position : public serializable::Serializable {
        T x{}, y{}, z{};

        Position() = default;

        Position(T x, T y, T z) : x(x), y(y), z(z) {}

        void exposed() override {
            expose("x", x);
            expose("y", y);
            expose("z", z);
        }

        unsigned int classID() const override {
            unsigned int sum = 0;
            const char* type = serializable::detail::string::TypeToString<T>;
            for(char c = *type; c != 0; c = *++type) sum += c % 10; // NOLINT(*-pointer-arithmetic)
            return sum;
        }
    };

    Position<double> pos{ 3.141, 2.718, 0 }, prevPos{ 3.141, 2.312, 0 };
    Position<int> target{ 10, 10, 0 }, camp{ 0, 0, 0 }, closestEnemy{ 0, 5, 0 };

    void exposed() override {
        expose("pos", pos);
        expose("prev pos", prevPos);
        expose("target", target);
        expose("camp", camp);
        expose("closest enemy", closestEnemy);
    }

    unsigned int classID() const override { return 1; }
};

void stressTest() {
    Stress stress;
    const auto start = std::chrono::high_resolution_clock::now();

    for(unsigned int i = 0; i < 10000; i++) {
        const auto result = stress.serialize();
        if(result.first != Stress::Result::OK || stress.deserialize(result.second) != Stress::Result::OK) {
            std::cout << "Stress test failed at iteration " << i << ".\n";
            return;
        }
    }

    const double time = static_cast<double>((std::chrono::high_resolution_clock::now() - start).count()) / 1e9;
    std::cout << "Stress test completed in " << std::fixed << std::setprecision(3) << time << " seconds.\n";
}

// NOLINTEND(*-non-private-*)

int main() {
    testStringManipulation();
    testPrimitiveConversions();
    testParsers();

    testSerialPrimitive();
    testSerialObject();
    testSerialPointer();

    testBasic();
    testAllTypes();
    testNested();

    testFiles();
    testErrors();
    // stressTest();

    std::cout << "All tests completed.\n";
    return 0;
}

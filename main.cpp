#include "serializable.hpp"
#include <bit>
#include <climits>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

// NOLINTBEGIN(*-non-private-*)

// Assert
template <typename T> concept Printable = requires(T t) {
    { std::cout << t << "" };
};

void assert(bool b, const char* function, const char* message) {
    if(b) return;
    std::cout << "[FAILED] " << function << ": " << message << "\n";
}

template <typename T, typename U>
void assertEqual(const T& expected, const U& actual, const char* function, const char* message) {
    assert(expected == actual, function, message); // NOLINT(*-decay)
}

template <Printable T, Printable U>
void assertEqual(const T& expected, const U& actual, const char* function, const char* message) {
    if(expected == actual) return;                                                             // NOLINT(*-decay)
    assert(false, function, message);                                                          // NOLINT(*-decay)
    std::cout << "### EXPECTED ###\n" << expected << "\n### ACTUAL ###\n" << actual << "\n\n"; // NOLINT(*-decay)
}

template <serializable::detail::Enum T, serializable::detail::Enum U>
void assertEqual(const T& expected, const U& actual, const char* function, const char* message) {
    assertEqual(static_cast<unsigned int>(expected), static_cast<unsigned int>(actual), function, message);
}

// String manipulation
void testStringManipulation() {
    namespace str = serializable::detail::string;

    assertEqual("abcdefxyz", str::makeString({ "abc", "def", "xyz" }), "makeString", "Case 1");

    assertEqual("def", str::substring("abcdefxyz", 3, 6), "substring", "Case 1");

    assertEqual("acaccaacaaca", str::replaceAll("ababbaabaaba", "b", "c"), "replaceAll", "Case 2");
    assertEqual("a<>aa<>a", str::replaceAll("abaaba", "b", "<>"), "replaceAll", "Case 2");
    assertEqual("a____bb__b__b____aa", str::replaceAll("aababbbabbabbababaa", "ab", "__"), "replaceAll", "Case 3");
    assertEqual("xabxba", str::replaceAll("aaabaaba", "aa", "x"), "replaceAll", "Case 4");

    assertEqual("abc\ndef\nxyz", str::connect({ "abc", "def", "xyz" }), "connect", "Case 1");
    assertEqual("abcdef", str::connect({ "abcdef" }), "connect", "Case 2");
    assertEqual("", str::connect({}), "connect", "Case 3");

    auto split = str::split("abc\ndef\nxyz");
    assert(3 == split.size(), "split", "Case 1");
    assertEqual("abc", split[0], "split", "Case 1.1");
    assertEqual("def", split[1], "split", "Case 1.2");
    assertEqual("xyz", split[2], "split", "Case 1.3");

    split = str::split("abc\n\ndef\n");
    assert(4 == split.size(), "split", "Case 2");
    assertEqual("abc", split[0], "split", "Case 2.1");
    assertEqual("", split[1], "split", "Case 2.2");
    assertEqual("def", split[2], "split", "Case 2.3");
    assertEqual("", split[3], "split", "Case 2.4");

    split = str::split("abc {\n\tdef\n\txyz\n}\nhi");
    assert(2 == split.size(), "split", "Case 3");
    assertEqual("abc {\n\tdef\n\txyz\n}", split[0], "split", "Case 3.1");
    assertEqual("hi", split[1], "split", "Case 3.2");

    assertEqual("\tabc", str::indent("abc"), "indent", "Case 1");
    assertEqual("\tabc\n\tdef\n\txyz", str::indent("abc\ndef\nxyz"), "indent", "Case 2");
    assertEqual("\tabc\n\t\t\n\t\tdef\n\t", str::indent("abc\n\t\n\tdef\n"), "indent", "Case 3");

    assertEqual("abc", str::unindent("\tabc"), "unindent", "Case 1");
    assertEqual("abc\ndef\nxyz", str::unindent("\tabc\n\tdef\n\txyz"), "unindent", "Case 2");
    assertEqual("abc\n\t\n\tdef\n", str::unindent("\tabc\n\t\t\n\t\tdef\n\t"), "unindent", "Case 3");
}

void testPrimitiveConversions() {
    namespace str            = serializable::detail::string;
    const constexpr auto NaN = std::nullopt;
    enum class Enum { ABC, DEF, XYZ };

    assertEqual("true", str::serializePrimitive(true), "serialize bool", "true");
    assertEqual("false", str::serializePrimitive(false), "serialize bool", "false");

    assertEqual(true, str::deserializePrimitive<bool>("true"), "deserialize bool", "true");
    assertEqual(false, str::deserializePrimitive<bool>("false"), "deserialize bool", "false");
    assertEqual(NaN, str::deserializePrimitive<bool>("meow"), "deserialize bool", "invalid");

    assertEqual("42", str::serializePrimitive(42), "serialize char", "42");
    assertEqual("-42", str::serializePrimitive(-42), "serialize char", "-42");

    assertEqual(42, str::deserializePrimitive<char>("42"), "deserialize char", "42");
    assertEqual(-42, str::deserializePrimitive<char>("-42"), "deserialize char", "-42");
    assertEqual(NaN, str::deserializePrimitive<char>("forty-two"), "deserialize char", "invalid");
    assertEqual(NaN, str::deserializePrimitive<char>(std::to_string(CHAR_MIN - 1)), "deserialize char", "underflow");
    assertEqual(NaN, str::deserializePrimitive<char>(std::to_string(CHAR_MAX + 1)), "deserialize char", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize unsigned char", "42");

    assertEqual(42, str::deserializePrimitive<unsigned char>("42"), "deserialize unsigned char", "42");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>("-42"), "deserialize unsigned char", "negative");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>("forty-two"), "deserialize unsigned char", "invalid");
    assertEqual(NaN, str::deserializePrimitive<unsigned char>(std::to_string(UCHAR_MAX + 1)),
                "deserialize unsigned char", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize short", "42");
    assertEqual("-42", str::serializePrimitive(-42), "serialize short", "-42");

    assertEqual(42, str::deserializePrimitive<short>("42"), "deserialize short", "42");
    assertEqual(-42, str::deserializePrimitive<short>("-42"), "deserialize short", "-42");
    assertEqual(NaN, str::deserializePrimitive<short>("forty-two"), "deserialize short", "invalid");
    assertEqual(NaN, str::deserializePrimitive<short>(std::to_string(SHRT_MIN - 1)), "deserialize short", "underflow");
    assertEqual(NaN, str::deserializePrimitive<short>(std::to_string(SHRT_MAX + 1)), "deserialize short", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize unsigned short", "42");

    assertEqual(42, str::deserializePrimitive<unsigned short>("42"), "deserialize unsigned short", "42");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>("-42"), "deserialize unsigned short", "negative");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>("forty-two"), "deserialize unsigned short", "invalid");
    assertEqual(NaN, str::deserializePrimitive<unsigned short>(std::to_string(USHRT_MAX + 1)),
                "deserialize unsigned short", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize int", "42");
    assertEqual("-42", str::serializePrimitive(-42), "serialize int", "-42");

    assertEqual(42, str::deserializePrimitive<int>("42"), "deserialize int", "42");
    assertEqual(-42, str::deserializePrimitive<int>("-42"), "deserialize int", "-42");
    assertEqual(NaN, str::deserializePrimitive<int>("forty-two"), "deserialize int", "invalid");
    assertEqual(NaN, str::deserializePrimitive<int>(std::to_string(INT_MIN - 1L)), "deserialize int", "underflow");
    assertEqual(NaN, str::deserializePrimitive<int>(std::to_string(INT_MAX + 1L)), "deserialize int", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize unsigned int", "42");

    assertEqual(42, str::deserializePrimitive<unsigned int>("42"), "deserialize unsigned int", "42");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>("-42"), "deserialize unsigned int", "negative");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>("forty-two"), "deserialize unsigned int", "invalid");
    assertEqual(NaN, str::deserializePrimitive<unsigned int>(std::to_string(UINT_MAX + 1L)), "deserialize unsigned int",
                "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize long", "42");
    assertEqual("-42", str::serializePrimitive(-42), "serialize long", "-42");

    assertEqual(42, str::deserializePrimitive<long>("42"), "deserialize long", "42");
    assertEqual(-42, str::deserializePrimitive<long>("-42"), "deserialize long", "-42");
    assertEqual(NaN, str::deserializePrimitive<long>("forty-two"), "deserialize long", "invalid");
    assertEqual(NaN, str::deserializePrimitive<long>(std::to_string(LONG_MIN) + "0"), "deserialize long", "underflow");
    assertEqual(NaN, str::deserializePrimitive<long>(std::to_string(LONG_MAX) + "0"), "deserialize long", "overflow");

    assertEqual("42", str::serializePrimitive(42), "serialize unsigned long", "42");

    assertEqual(42, str::deserializePrimitive<unsigned long>("42"), "deserialize unsigned long", "42");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>("-42"), "deserialize unsigned long", "negative");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>("forty-two"), "deserialize unsigned long", "invalid");
    assertEqual(NaN, str::deserializePrimitive<unsigned long>(std::to_string(ULONG_MAX) + "0"),
                "deserialize unsigned long", "overflow");

    assertEqual("3.141000", str::serializePrimitive(3.141), "serialize float", "PI");
    assertEqual("-3.141000", str::serializePrimitive(-3.141), "serialize float", "-PI");

    assert(std::abs(3.141 - str::deserializePrimitive<float>("3.141000").value_or(0)) < 0.0001, "deserialize float",
           "PI");
    assert(std::abs(-3.141 - str::deserializePrimitive<float>("-3.141000").value_or(0)) < 0.0001, "deserialize float",
           "-PI");
    assertEqual(NaN, str::deserializePrimitive<float>("pi"), "deserialize float", "invalid");

    assertEqual("3.141000", str::serializePrimitive(3.141), "serialize double", "PI");
    assertEqual("-3.141000", str::serializePrimitive(-3.141), "serialize double", "-PI");

    assert(std::abs(3.141 - str::deserializePrimitive<double>("3.141000").value_or(0)) < 0.0001, "deserialize double",
           "PI");
    assert(std::abs(-3.141 - str::deserializePrimitive<double>("-3.141000").value_or(0)) < 0.0001, "deserialize double",
           "-PI");
    assertEqual(NaN, str::deserializePrimitive<double>("pi"), "deserialize double", "invalid");

    assertEqual("\"Hello, world!\"", str::serializePrimitive<std::string>("Hello, world!"), "serialize string",
                "Hello, world!");
    assertEqual("\"&quot;Hi!&quot;&newline;\"", str::serializePrimitive<std::string>("\"Hi!\"\n"), "serialize string",
                "Escaped characters");

    assertEqual("Hello!", str::deserializePrimitive<std::string>("\"Hello!\""), "deserialize string", "Hello!");
    assertEqual("\"Hi!\"\n", str::deserializePrimitive<std::string>("\"&quot;Hi!&quot;&newline;\""),
                "deserialize string", "Escaped characters");

    assertEqual("1", str::serializePrimitive(Enum::DEF), "serialize enum", "def");

    assertEqual(Enum::XYZ, str::deserializePrimitive<Enum>("2").value_or(Enum::ABC), "deserialize enum", "xyz");
    assertEqual(NaN, str::deserializePrimitive<Enum>("ABC"), "deserialize enum", "invalid");
    assertEqual(static_cast<Enum>(4), str::deserializePrimitive<Enum>("4"), "deserialize enum", "out of range");
}

void testParsers() {
    namespace str = serializable::detail::string;

    auto primitive = str::parsePrimitive("BOOL my_bool = true");
    if(primitive) {
        assertEqual("BOOL", primitive->at(0), "parsePrimitive", "Case 1.0");
        assertEqual("my_bool", primitive->at(1), "parsePrimitive", "Case 1.1");
        assertEqual("true", primitive->at(2), "parsePrimitive", "Case 1.2");
    } else assert(false, "parsePrimitive", "Case 1");

    primitive = str::parsePrimitive("STRING username = \"xXThat_GuyXx\"");
    if(primitive) {
        assertEqual("STRING", primitive->at(0), "parsePrimitive", "Case 2.0");
        assertEqual("username", primitive->at(1), "parsePrimitive", "Case 2.1");
        assertEqual("\"xXThat_GuyXx\"", primitive->at(2), "parsePrimitive", "Case 2.2");
    } else assert(false, "parsePrimitive", "Case 2");

    primitive = str::parsePrimitive("FLOAT pi = 3.141");
    if(primitive) {
        assertEqual("FLOAT", primitive->at(0), "parsePrimitive", "Case 3.0");
        assertEqual("pi", primitive->at(1), "parsePrimitive", "Case 3.1");
        assertEqual("3.141", primitive->at(2), "parsePrimitive", "Case 3.2");
    } else assert(false, "parsePrimitive", "Case 3");

    auto object = str::parseObject("OBJECT<0> root = 1 {\n\t\n}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject", "Case 1.0");
        assertEqual("root", object->at(1), "parseObject", "Case 1.1");
        assertEqual("1", object->at(2), "parseObject", "Case 1.2");
        assertEqual("\t", object->at(3), "parseObject", "Case 1.3");
    } else assert(false, "parseObject", "Case 1");

    object = str::parseObject("OBJECT<0> root = 1 {}");
    if(object) {
        assertEqual("0", object->at(0), "parseObject", "Case 2.0");
        assertEqual("root", object->at(1), "parseObject", "Case 2.1");
        assertEqual("1", object->at(2), "parseObject", "Case 2.2");
        assertEqual("", object->at(3), "parseObject", "Case 2.3");
    } else assert(false, "parseObject", "Case 2");

    object = str::parseObject("OBJECT<3> root = 5 {\n\tINT answer = 42\n\tBOOL valid = true\n}");
    if(object) {
        assertEqual("3", object->at(0), "parseObject", "Case 3.0");
        assertEqual("root", object->at(1), "parseObject", "Case 3.1");
        assertEqual("5", object->at(2), "parseObject", "Case 3.2");
        assertEqual("\tINT answer = 42\n\tBOOL valid = true", object->at(3), "parseObject", "Case 3.3");
    } else assert(false, "parseObject", "Case 3");

    object = str::parseObject("OBJECT<1> root = 1 {\n\tOBJECT<2> sub = 2 {\n\t\tDOUBLE pi = 3.14\n\t}\n}");
    if(object) {
        assertEqual("1", object->at(0), "parseObject", "Case 4.0");
        assertEqual("root", object->at(1), "parseObject", "Case 4.1");
        assertEqual("1", object->at(2), "parseObject", "Case 4.2");
        assertEqual("\tOBJECT<2> sub = 2 {\n\t\tDOUBLE pi = 3.14\n\t}", object->at(3), "parseObject", "Case 4.3");
    } else assert(false, "parseObject", "Case 4");

    auto pointer = str::parsePointer("PTR<4> my_pointer = 23");
    if(pointer) {
        assertEqual("4", pointer->at(0), "parsePointer", "Case 1.0");
        assertEqual("my_pointer", pointer->at(1), "parsePointer", "Case 1.1");
        assertEqual("23", pointer->at(2), "parsePointer", "Case 1.2");
    } else assert(false, "parsePointer", "Case 1");
}

// Serial types
void testSerialPrimitive() {
    using serializable::detail::SerialPrimitive;

    const SerialPrimitive source("INT", "the_answer", "42");
    assertEqual("INT the_answer = 42", source.get(), "SerialPrimitive", "get");

    SerialPrimitive target;
    assert(target.set("BOOL my_bool = true"), "SerialPrimitive", "set");
    assertEqual("BOOL my_bool = true", target.get(), "SerialPrimitive", "set");

    assert(target.set(source.get()), "SerialPrimitive", "transfer");
    assertEqual(source.get(), target.get(), "SerialPrimitive", "transfer");
}

void testSerialObject() {
    using serializable::detail::SerialObject;
    using serializable::detail::SerialPrimitive;

    SerialObject source(1, "root", 0, 0);
    source.append(std::make_unique<SerialPrimitive>("BOOL", "my_bool", "false"));
    source.append(std::make_unique<SerialPrimitive>("INT", "something", "123"));
    source.append(std::make_unique<SerialPrimitive>("FLOAT", "pi", "3.141"));
    {
        auto pos = std::make_unique<SerialObject>(2, "pos", 0, 0);
        pos->append(std::make_unique<SerialPrimitive>("INT", "x", "1"));
        pos->append(std::make_unique<SerialPrimitive>("INT", "y", "4"));
        source.append(std::move(pos));
    }

    SerialObject target;
    assert(target.set(source.get()), "SerialObject", "set");

    auto child = target.getChild("my_bool");
    if(child) assertEqual("BOOL my_bool = false", child.value()->get(), "SerialObject", "my_bool");
    else assert(false, "SerialObject", "extract my_bool");

    child = target.getChild("something");
    if(child) assertEqual("INT something = 123", child.value()->get(), "SerialObject", "something");
    else assert(false, "SerialObject", "extract something");

    child = target.getChild("pi");
    if(child) assertEqual("FLOAT pi = 3.141", child.value()->get(), "SerialObject", "pi");
    else assert(false, "SerialObject", "extract pi");

    child = target.getChild("pos");
    if(child) {
        auto* pos = child.value()->asObject();

        auto sub = pos->getChild("x");
        if(sub) assertEqual("INT x = 1", sub.value()->get(), "SerialObject", "pos.x");
        else assert(false, "SerialObject", "extract pos.x");

        sub = pos->getChild("y");
        if(sub) assertEqual("INT y = 4", sub.value()->get(), "SerialObject", "pos.y");
        else assert(false, "SerialObject", "extract pos.y");
    } else assert(false, "SerialObject", "extract pos");
}

void testSerialPointer() {
    using serializable::detail::SerialPointer;

    unsigned long data = 123;
    const SerialPointer source(42, "my_pointer", std::bit_cast<void**>(&data));
    assertEqual("PTR<42> my_pointer = 123", source.get(), "SerialPointer", "get");

    SerialPointer target;
    assert(target.set("PTR<3> mirror = 5"), "SerialPointer", "set");
    assertEqual("PTR<3> mirror = 5", target.get(), "SerialPointer", "set");

    assert(target.set(source.get()), "SerialPointer", "transfer");
    assertEqual(source.get(), target.get(), "SerialPointer", "transfer");
}

// Basic
struct Basic : public serializable::Serializable {
    int value;

    explicit Basic(int i) : value(i) {}

    void exposed() override { expose("i", value); }

    unsigned int classID() const override { return 3; }
};

void testBasic() {
    Basic source(42);

    const auto serialized = source.serialize();
    assertEqual(Basic::Result::OK, serialized.first, "Basic", "serialize result");
    assertEqual("OBJECT<3> root = 1 {\n\tINT i = 42\n}", serialized.second, "Basic", "serialize value");

    Basic target(0);
    const auto deserialized = target.deserialize(serialized.second);
    assertEqual(Basic::Result::OK, deserialized, "Basic", "deserialize result");
    assertEqual(source.value, target.value, "Basic", "deserialize value");
}

// All types
struct AllTypes : public serializable::Serializable {
    enum class MyEnum { ABC, DEF, XYZ };

    bool b;
    char c;
    unsigned char uc;
    short s;
    unsigned short us;
    int i;
    unsigned int ui;
    long l;
    unsigned long ul;
    float f;
    double d;
    std::string str;
    MyEnum m;
    AllTypes* self;

    AllTypes(bool b, char c, unsigned char uc, short s, unsigned short us, int i, unsigned int ui, long l,
             unsigned long ul, float f, double d, std::string str, MyEnum m)
        : b(b), c(c), uc(uc), s(s), us(us), i(i), ui(ui), l(l), ul(ul), f(f), d(d), str(std::move(str)), m(m),
          self(this) {}

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
        expose("m", m);
        expose("self", self);
    }

    unsigned int classID() const override { return 5; }
};

void testAllTypes() {
    AllTypes source(true, 'a', 'x', 42, 69, -123, 123, 6502, 12345, 3.141, -1.5, "Hello, world!\n",
                    AllTypes::MyEnum::DEF);
    const auto serialized = source.serialize();
    assertEqual(AllTypes::Result::OK, serialized.first, "All Types", "serialize result");

    AllTypes target(false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", AllTypes::MyEnum::ABC);
    const auto deserialized = target.deserialize(serialized.second);
    assertEqual(AllTypes::Result::OK, deserialized, "All Types", "deserialize result");
    assertEqual(source.b, target.b, "All Types", "deserialize value BOOL");
    assertEqual(source.c, target.c, "All Types", "deserialize value CHAR");
    assertEqual(source.uc, target.uc, "All Types", "deserialize value UCHAR");
    assertEqual(source.s, target.s, "All Types", "deserialize value SHORT");
    assertEqual(source.us, target.us, "All Types", "deserialize value USHORT");
    assertEqual(source.i, target.i, "All Types", "deserialize value INT");
    assertEqual(source.ui, target.ui, "All Types", "deserialize value UINT");
    assertEqual(source.l, target.l, "All Types", "deserialize value LONG");
    assertEqual(source.ul, target.ul, "All Types", "deserialize value ULONG");
    assertEqual(source.f, target.f, "All Types", "deserialize value FLOAT");
    assertEqual(source.d, target.d, "All Types", "deserialize value DOUBLE");
    assertEqual(source.str, target.str, "All Types", "deserialize value STRING");
    assertEqual(source.m, target.m, "All Types", "deserialize value ENUM");
    assertEqual(&target, target.self, "All Types", "deserialize value PTR");
}

// Nested
struct Nested : public serializable::Serializable {
    struct Position : public serializable::Serializable {
        Position() = default;

        Position(int x, int y) : x(x), y(y) {}

        void exposed() override {
            expose("x", x);
            expose("y", y);
        }

        unsigned int classID() const override { return 2; }

        int x{}, y{};
    };

    struct Player : public serializable::Serializable {
        Player() = default;

        Player(const Position& pos, int level) : pos(pos), level(level) {}

        void exposed() override {
            expose("Position", pos);
            expose("level", level);
        }

        unsigned int classID() const override { return 3; }

        Position pos;
        int level{};
    };

    void exposed() override {
        expose("pos", pos);
        expose("prevPos", prevPos);
        expose("player", player);
    }

    unsigned int classID() const override { return 1; }

    Position pos, prevPos;
    Player player;
};

void testNested() {
    Nested source;
    source.pos          = { 12, 34 };
    source.prevPos      = { 42, 69 };
    source.player.pos   = { 1, 2 };
    source.player.level = 2;

    const auto serialized = source.serialize();
    assertEqual(Nested::Result::OK, serialized.first, "Nested", "serialize result");

    Nested target;
    const auto deserialized = target.deserialize(serialized.second);
    assertEqual(Nested::Result::OK, deserialized, "Nested", "deserialize result");
    assertEqual(source.pos.x, target.pos.x, "Nested", "deserialize result pos.x");
    assertEqual(source.pos.y, target.pos.y, "Nested", "deserialize result pos.y");
    assertEqual(source.prevPos.x, target.prevPos.x, "Nested", "deserialize result prevPos.x");
    assertEqual(source.prevPos.y, target.prevPos.y, "Nested", "deserialize result prevPos.y");
    assertEqual(source.player.pos.x, target.player.pos.x, "Nested", "deserialize result player.pos.x");
    assertEqual(source.player.pos.y, target.player.pos.y, "Nested", "deserialize result player.pos.y");
    assertEqual(source.player.level, target.player.level, "Nested", "deserialize result player.level");
}

// Files
struct Files : public serializable::Serializable {
    int value{};

    Files() = default;

    void exposed() override { expose("value", value); }

    unsigned int classID() const override { return 42; }
};

void testFiles() {
    Files source;
    source.value = 42;

    auto result = source.save("save.txt");
    assertEqual(Files::Result::OK, result, "Files", "save");

    Files target;
    result = target.load("save.txt");
    assertEqual(Files::Result::OK, result, "Files", "load");
    assertEqual(source.value, target.value, "Files", "load data");

    result = target.load("not-there.txt");
    assertEqual(Files::Result::FILE, result, "Files", "load inexistent file");
}

// Errors
struct Errors : public serializable::Serializable {
    std::string name, value;

    Errors(std::string name, std::string value) : name(std::move(name)), value(std::move(value)) {}

    void exposed() override { expose(name, value); }

    unsigned int classID() const override { return 10; }

    void set(const std::string& name, const std::string& value) {
        this->name  = name;
        this->value = value;
    }
};

void testErrors() {
    Errors target("answer", "42");

    auto deserialize = target.deserialize("OBJECT<0> root = 0 {\n\tSTRING answer = \"42\"\n}");
    assertEqual(Errors::Result::TYPECHECK, deserialize, "Errors", "wrong class id");

    deserialize = target.deserialize("OBJECT<10> root = 0 {}");
    assertEqual(Errors::Result::INTEGRITY, deserialize, "Errors", "missing values");

    deserialize = target.deserialize("OBJECT<10> root = 0 {\n\tINT answer = 42\n}");
    assertEqual(Errors::Result::TYPECHECK, deserialize, "Errors", "wrong primitive type");

    deserialize = target.deserialize("");
    assertEqual(Errors::Result::STRUCTURE, deserialize, "Errors", "empty string");

    deserialize = target.deserialize(R"({"answer": "42"})");
    assertEqual(Errors::Result::STRUCTURE, deserialize, "Errors", "json string");

    deserialize = target.deserialize("answer := 42");
    assertEqual(Errors::Result::STRUCTURE, deserialize, "Errors", "unformatted");

    target.name    = "with space";
    auto serialize = target.serialize();
    assertEqual(Errors::Result::OK, serialize.first, "Errors", "name with space");
    assertEqual("OBJECT<10> root = 1 {\n\tSTRING " + target.name + " = \"42\"\n}", serialize.second, "Errors",
                "name with space value");
    assertEqual(Errors::Result::OK, target.deserialize(serialize.second), "Errors", "name with space deserialize");

    target.name = "some (more) \"funny\" stuff";
    serialize   = target.serialize();
    assertEqual(Errors::Result::OK, serialize.first, "Errors", "name with quotes and brackets");
    assertEqual("OBJECT<10> root = 1 {\n\tSTRING " + target.name + " = \"42\"\n}", serialize.second, "Errors",
                "name with quotes and brackets value");
    assertEqual(Errors::Result::OK, target.deserialize(serialize.second), "Errors",
                "name with quotes and brackets deserialize");

    target.name = "INT i";
    serialize   = target.serialize();
    assertEqual(Errors::Result::OK, serialize.first, "Errors", "primitive type name");
    assertEqual("OBJECT<10> root = 1 {\n\tSTRING " + target.name + " = \"42\"\n}", serialize.second, "Errors",
                "primitive type name value");
    assertEqual(Errors::Result::OK, target.deserialize(serialize.second), "Errors", "primitive type name deserialize");

    target.name = "";
    serialize   = target.serialize();
    assertEqual(Errors::Result::OK, serialize.first, "Errors", "no name");
    assertEqual("OBJECT<10> root = 1 {\n\tSTRING " + target.name + " = \"42\"\n}", serialize.second, "Errors",
                "no name value");
    assertEqual(Errors::Result::OK, target.deserialize(serialize.second), "Errors", "no name deserialize");
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

    std::cout << "Test finished\n";
    return 0;
}

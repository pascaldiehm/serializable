# Serializable

A simple, creatively named C++ serialization library.

## Table of Contents

- [Introduction](#introduction)
  - [Serialization](#serialization)
  - [Motivation](#motivation)
  - [Premise](#premise)
  - [Limitations](#limitations)
- [Usage](#usage)
  - [Installation](#installation)
  - [Quickstart](#quickstart)
  - [Documentation](#documentation)
  - [Save file syntax](#save-file-syntax)
- [Contributing](#contributing)
- [Notes](#notes)

## Introduction

### Serialization

Serialization is the process of translating a data structure into a format that can easily be stored or transmitted and reconstructed at a different time and/or place ([Source](https://en.wikipedia.org/wiki/Serialization)).
Achieving this usually means breaking the data structure down into some linear format (mostly streams, strings or byte-lists), storing it to the disk and later reconstructing the data structure from the serialized data.
While the serialization (data structure to stream) is _usually_ trivial, the deserialization (stream to data structure) can get quite tricky regarding aspects like type safety and error detection.

### Motivation

The concept of serialization is extremely common in virtually all types of programs.
As soon as you just want to store some state between program executions, you somehow have to serialize that state.
Naturally I also ran into situations requiring me to serialize some data to the disk.
The last time that happened, I finally decided that I want to avoid some hacky, pseudo-stable abomination that will cause more headaches than template errors and slowly degrade my will to work on that side project.
So I turned to the only website I _might_ use more often than StackOverflow: [cppreference.com](https://en.cppreference.com/w/cpp/links/libs), to find a usable serialization library there.
Sadly, everything I could find was either far to complicated, too wordy or just not what I was looking for.
So I set out on the quest of finally writing a _proper_ serialization library myself.
The project began as a (once again hacky) cocktail of ideas embedded in the mentioned side project, but as it grew in size and closer to my heart I decided to extract it to its own repository - mainly to be separated and reused in different projects but now also to live and grow as an open source project.

### Premise

In theory, serialization should be incredibly simple - in the end, what is a data structure if not serialized data stored in my computers RAM?
So the easiest way of serializing (and in fact also deserializing) data is just a simple memory copy, from the RAM to the disc (and the other way round).
That approach definitely works, but it has two major flaws:

Firstly, what happens when the data you stored (or transmitted) somehow got corrupted or even tampered with?
In the best case you will just read a few wrong numbers, in the worst case trying to deserialize that might send your entire program to an unavoidable crash.

Secondly, serialization doesn't mean blindly storing data.
There might be some data that we don't want to serialize (runtime status, counters, etc.) or even some things that can not (sensibly) be serialized (see [Limitations](#limitations)).
Therefore it would be great to be able to choose what exactly we want to serialize.

That is exactly how this library works.
It gives your data structure a way to _expose_, which variables it would like to be serialized.
Everything else, including some basic error checking, is then handled by the library.

### Limitations

Due to the unpredictable nature of RAM addressing, pointers can not easily be serialized, as the address of an object when it is serialized is not necessarily the same when it is deserialized.
Nevertheless, this library has the function to serialize and deserialize pointers.
This is achieved by assigning every object that is serialized a _virtual address_ that is then stored instead of all pointers to that object.
When deserialized, these virtual addresses get resolved to the new real address.
Even though for most cases this approach does exactly what it is supposed to do, there are a few limitations/caveats:

1. `nullptr` cannot be serialized, as their classID cannot be obtained.
2. For every pointer that is serialized, the object pointed to also has to be serialized in the same invocation of the `serialize` function. Otherwise serializing the object will fail.
3. Stored pointers lose their type. If the serialized data got corrupted or tampered with, deserializing a pointer might point you to a completely different type. You can prevent this by overriding the `classID` function.
4. Usually when exposing a variable, it becomes valid (i.e. the new value) right after your call to the `expose` function. Not so for pointers though. Since a pointer can point to an object that has not been deserialized when the pointer is deserialized, all pointers are set after _all_ `expose` calls are processed.

## Usage

### Installation

This is a header-only library.
So all you have to do to include it into your project is to download `serializable.hpp` into your include path.

Note: Even though this project looks like it can be build using CMake, it can't.
The `CMakeLists.txt` file is just used for building some tests.

### Quickstart

For starters: Every data structure you wish to serialize has to extend the base class `serializable::Serializable`.
Doing so will add four public member functions to your class:

1. `std::pair<Result, std::string> serialize()`: Runs the serialization and returns the result and the data, if it was successful.
2. `Result deserialize(const std::string&)`: Deserializes the given string into the class. Returns whether the deserialization was successful.
3. `Result load(const std::filesystem::path&)`: Loads the given file and deserializes into the class. Returns whether the file could be loaded and deserialized.
4. `Result save(const std::filesystem::path&)`: Serializes the class into the given file. Returns whether the file could be written to and the class could be serialized.

Any class extending the `Serializable` class has to override an abstract method: `void exposed()`.
This is where you declare which data you would like to be serialized/deserialized.
For every variable you want to serialize, you have to call `void expose(const std::string& name, T& value)`.

For `name` you can generally pass any string you want.
It's a good idea to take the name of the variable, to avoid duplication and confusion.
It's a bad idea to use strings containing `{`, `}`, `\n` and `=`, as this might confuse the parser.
Every other name _should_ be fine though.

For `value` you usually simply have to pass in the name of the variable - C++ will automatically pass it as a reference (as requested by the `expose` function).
The type of `value` should be a primitive type (`bool`, `[unsigned] char`, `[unsigned] short`, `[unsigned] int`, `[unsigned] long`, `double`, `float`), `std::string`, an enum, some class extending `serializable::Serializable` or a pointer to such a class.

The given functions will now serialize/deserialize any variable exposed in this way.
Note that it is also possible to apply some pre-/postprocessing to your variables inside of your `exposed` function.
(You can even copy a member variable to a local variable, expose the local variable and then copy the local variable back to the member variable).
Just keep in mind, that the `exposed` function is usually called twice per lifecycle: both serializing and deserializing call the `exposed` method.
(This could be 'fixed' by writing different functions for serializing/deserializing but the library is intentionally built in this way to prevent code duplication).

If you are planning on serializing and deserializing pointers, you should also override the `unsigned int classID()` method.
This method is supposed to return an unique (unsigned) integer for every class used to perform typechecking on serialized objects and pointers.
You should not use 0 as this is the default for classes that don't implement this function.

### Documentation

#### Class serializable::Serializable

- `public: enum Result` An enum class containing the return codes for `OK` (no error), `FILE` (file not found/accessible), `STRUCTURE` (structural error in provided serial data), `TYPECHECK` (invalid value or pointer type mismatch), `INTEGRITY` (missing required value) and `POINTER` (invalid pointer).
- `public: Serializable(...)` The class provides a default constructor (without arguments), the default copy constructor/assignment operator, a virtual default destructor and no move constructor/assignment operator.
- `public: std::pair<Result, std::string> serialize()` Returns the serialization result and the serialized data. Result can only be `OK` or `POINTER`.
- `public: Result deserialize(const std::string&)` Deserialize the given string into this class. Result can be anything except `FILE`.
- `public: Result load(const std::filesystem::path&)` Tries to load the given file and deserialize its content into the class. Result can be anything.
- `public: Result save(const std::filesystem::path&)` Tries to serialize the class into the given file. Result can be `OK`, `POINTER` or `FILE`.
- `protected: virtual void exposed()` Abstract function that is called to get exposed fields.
- `protected: virtual unsigned int classID()` Virtual function that is called to get the ID of the current class. By default it returns 0.
- `protected: void expose(const std::string&, bool&)` Expose a bool value.
- `protected: void expose(const std::string&, char&)` Expose a char value.
- `protected: void expose(const std::string&, unsigned char&)` Expose an unsigned char value.
- `protected: void expose(const std::string&, short&)` Expose a short value.
- `protected: void expose(const std::string&, unsigned short&)` Expose an unsigned short value.
- `protected: void expose(const std::string&, int&)` Expose an int value.
- `protected: void expose(const std::string&, unsigned int&)` Expose an unsigned int value.
- `protected: void expose(const std::string&, long&)` Expose a long value.
- `protected: void expose(const std::string&, unsigned long&)` Expose an unsigned long value.
- `protected: void expose(const std::string&, float&)` Expose a float value.
- `protected: void expose(const std::string&, double&)` Expose a double value.
- `protected: template <Enum E> void expose(const std::string&, E&)` Expose an enum value.
- `protected: void expose(const std::string&, std::string&)` Expose a string value.
- `protected: void expose(const std::string&, Serializable&)` Expose a Serializable subclass.
- `protected: template <typename S> requires std::is_base_of_v<Serializable, S> void expose(const std::string&, S*&)` Expose a pointer to a serializable child object.

#### Aliases, Concepts and other Classes

- `using detail::Address` A type alias for addresses.
- `concept detail::Enum` An enum type.
- `class detail::Serial` An intermediate class for translating between a string and your class. Don't worry about it.
- `class detail::SerialPrimitive` An intermediate class for translating a primitive value between a string and your class.
  - `enum detail::SerialPrimitive::Type` Type of a primitive value.
- `class detail::SerialObject` An intermediate class for translating a subclass between a string and your class.
- `class detail::SerialPointer` An intermediate class for translating a pointer between a string and your class.
- `std::string detail::string::makeString(const std::initializer_list<std::string>&)` Builds a string from parts.
- `std::string detail::string::substring(const std::string&, std::size_t, std::size_t)` Substring from start to end.
- `std::string detail::string::replaceAll(const std::string&, const std::string&, const std::string&)` Replaces all occurrences of the second string in the first string with the third string.
- `std::string detail::string::connect(const std::vector<std::string>&)` Combines a vector of lines into a single multiline string.
- `std::vector<std::string> detail::string::split(const std::string&)` Splits a multiline string into a vector of lines. Keeps areas between curly brackets together.
- `std::string detail::string::indent(const std::string&)` Indents every line in a string by one tab.
- `std::string detail::string::unindent(const std::string&)` Removes one tab from every line in a string.
- `std::string {primitive}ToString({primitive})` Converts a `{primitive}` to a string.
- `std::optional<{primitive}> stringTo{Primitive}(const std::string&)` Converts a string to a `{primitive}`.
- `std::string detail::string::encodeString(const std::string&)` Replaces unsafe characters (`"`, `\n`) in a string with safe versions.
- `std::string detail::string::decodeString(const std::string&)` Replaces safe versions of unsafe characters with the original character.
- `template <Enum E> std::string detail::string::enumToString(E)` Converts an enum to a string.
- `template <Enum E> std::optional<E> detail::string::stringToEnum(const std::string&)` Converts a string to an enum.
- `std::string detail::string::typeToString(SerialPrimitive::Type)` Returns a string representation of the type.
- `SerialPrimitive::Type detail::string::stringToType(const std::string&)` Converts a string representation of a type the the type.
- `std::optional<std::array<std::string, 3>> detail::string::parsePrimitive(const std::string&)` Parses a line of data as a primitive.
- `std::optional<std::array<std::string, 4>> detail::string::parseObject(const std::string&)` Parses a block of data as an object.
- `std::optional<std::array<std::string, 3>> detail::string::parsePointer(const std::string&)` Parses a line of data as a pointer.

### Save file syntax

The safe file contains one line per exposed field (the only exception being Serializable subclasses) consisting of the type, the name and the value of the variable.

```EBNF
file = object;
object = 'OBJECT<', class_id, '> ', name, ' = ', address, ' {\n', {'\t', value, '\n'}, '}';
class_id = unum;
name = safe_char, {safe_char};
address = unum;
value = object | primitive | pointer;
primitive = primitive_bool | primitive_number | primitive_string;
pointer = 'PTR<', class_id, '> ', name, ' = ', address;
primitive_bool = 'BOOL ', name, ' = ', ('true' | 'false');
primitive_number = primitive_signed | primitive_unsigned | primitive_floating;
primitive_string = 'STRING ', name, ' = "', {string_char}, '"';
primitive_signed = ('CHAR' | 'SHORT' | 'INT' | 'LONG'), ' ', name, ' = ', ['-'], unum;
primitive_unsigned = ('UCHAR' | 'USHORT' | 'UINT' | 'ULONG' | 'ENUM'), ' ', name, ' = ', unum;
primitive_floating = ('FLOAT' | 'DOUBLE'), ' ', name, ' = ', ['-'], unum, '.', unum;

unum = digit, {digit};

digit = <any digit>;
safe_char = <any character except curly brackets, newline, equals and space>;
string_char = <any character except quotes and newlines>;
```

Note that the save files are quite human-friendly.
That means this library can also be used as a configuration file manager.

## Contributing

If you find a bug, missing a feature or just have an awesome idea you would like to add to this library, I would love to receive a pull request from you.
The only non-trivial\* requirement I have is that the library has to stay a (single) header-only library.

<sub>\* non-trivial meaning that obviously PRs deleting everything and making this library a Fibonacci-Calculator will not be merged.</sub>

## Notes

- This library requires C++ >= 20
- Note that all classes using this serialization have to have a default value. Loading in the context of this library means you _overwrite_ existing data, not _create_ new data (that is why I usually call the process deserializing _into_ a class). First create default data, then (try to) overwrite it with stored data.
- About runtime. Serializing data just has to append the data it gets to a string, hence `serialize` runs in O(n). The problem with deserialization is that you cannot guarantee to get the serialized data in the same order as you serialized it. Therefor the serialized string is first parsed and inserted into a `std::unordered_map` which is then queried for variables. _Usually_ querying an unordered map runs in constant time, which would make `deserialize` also run in O(n) (Technically O(n\*d) where d is the maximum nesting depth, as the data of an object is processed by all parents above).
- All files in this repo (except the `serializable.hpp`) can safely be ignored: They are just meta-files, IDE settings and tests. Speaking about tests: I know, the tests implemented in the `main.cpp` file are as bad as I earlier on described my previous pseudo-stable serializers, but they are good enough for what they have to do. So don't judge me for the tests, judge me for the library. Also if you have an afternoon to spare, I would love to see proper testing some day.

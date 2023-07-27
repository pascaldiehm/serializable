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
It's a bad idea to use strings containing `\n` and `=`, as this will confuse the parser.
Every other name _should_ be fine though.

For `value` you usually simply have to pass the name of the variable - C++ will automatically pass it as a reference (as requested by the `expose` function).
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

- `namespace serializable` The enclosing namespace for everything this library provides.
  - `class Serializable` The base class providing the serialization functionality to any derived class.
    - `enum class Result` The result of a serialization action. `OK`: Everything worked, `FILE`: File was not found or could not be created, `STRUCTURE`: Data is syntactically invalid, `INTEGRITY`: Data does not satisfy required structure, `TYPECHECK`: Data has invalid types, `POINTER`: Invalid pointer type of value.
    - `public: Serializable()` A default constructor.
    - `public: Serializable(const Serializable&)` A default copy constructor.
    - `public: Serializable(Serializable&&)` An explicitly deleted move constructor.
    - `public: Serializable& operator=(const Serializable&)` A default copy assignment operator.
    - `public: Serializable& operator=(Serializable&&)` An explicitly deleted move assignment operator.
    - `public: virtual ~Serializable()` A virtual default destructor.
    - `public: std::pair<Result, std::string> serialize()` Serialize the class into a string.
    - `public: Result deserialize(const std::string&)` Deserialize a string into the class.
    - `public: Result save(const std::filesystem::path&)` Serialize to a file.
    - `public: Result load(const std::filesystem::path&)` Deserialize from a file.
    - `protected: virtual void exposed()` Will be called to get exposed variables.
    - `protected: virtual unsigned int classID() const` Will be called to get the unique class id.
    - `protected: template <SerializablePrimitive S> void expose(const std::string&, S&)` Expose a primitive value.
    - `protected: void expose(const std::string&, Serializable& value)` Expose a serializable class.
    - `protected: template <SerializableObject S> void expose(const std::string&, S*&)` Expose a pointer to a serializable class.
  - `namespace detail` A namespace containing helper functions, structures and other implementation details.
    - `using Address` A type alias for addresses.
    - `concept SerializableObject` A concept for any class extending the `Serializable` base class.
    - `concept Enum` A concept for any enum.
    - `concept Number` A concept for any numeric type (a number that can be converted to a string by std::to_string).
    - `class Serial` An abstract base class for structured serial data.
      - `public: Serial()` A default constructor.
      - `public: Serial(const Serial&)` A default copy constructor.
      - `public: Serial(Serial&&)` An explicitly deleted move constructor.
      - `public: Serial& operator=(const Serial&)` A default copy assignment operator.
      - `public: Serial& operator=(Serial&&)` An explicitly deleted move assignment operator.
      - `public: virtual ~Serial()` A virtual default destructor.
      - `public: virtual std::string get() const` A function returning the serialized data of this object.
      - `public: virtual bool set(const std::string&)` A function setting the object from serialized data returning the success of the operation.
      - `public: virtual std::string getName() const` A function returning the name of the serialize field.
      - `public: SerialPrimitive* asPrimitive()` A function returning `this` as a `SerialPrimitive` pointer.
      - `public: SerialObject* asObject()` A function returning `this` as a `SerialObject` pointer.
      - `public: SerialPointer* asPointer()` A function returning `this` as a `SerialPointer` pointer.
    - `class SerialPrimitive` A class representing a serialized primitive.
      - `public: SerialPrimitive()` A default constructor.
      - `public: SerialPrimitive(std::string, std::string, std::string)` A constructor from data.
      - `public: std::string get() const override` An implementation of `Serial::get`.
      - `public: void set(const std::string&) override` An implementation `Serial::set`.
      - `public: std::string getName() const override` An implementation `Serial::getName`.
      - `public: std::string getType() const` Returns the serialized type.
      - `public: std::string getValue() const` Returns the serialized value.
    - `class SerialObject` A class representing a serialized subclass.
      - `public: SerialObject()` A default constructor.
      - `public: SerialObject(unsigned int, std::string, Address, Address)` A constructor from data.
      - `public: std::string get() const override` An implementation of `Serial::get`.
      - `public: void set(const std::string&) override` An implementation `Serial::set`.
      - `public: std::string getName() const override` An implementation `Serial::getName`.
      - `public: void append(const std::shared_ptr<Serial>&)` Appends a shared pointer to a `Serial` object to this object.
      - `public: std::optional<std::shared_ptr<Serial>> getChild(const std::string&)` Returns the child with the specified name (if it exists).
      - `public: unsigned int getClass()` Returns the class id of the serialized object.
      - `public: void virtualizeAddresses(std::unordered_map<Address, Address>&)` Generates a virtual address and registers it in the address map. Also passes the invocation to all children `SerialObject`s.
      - `public: void restoreAddresses(std::unordered_map<Address, Address>&)` Registers its real address under its virtual address. Also passes the invocation to all children `SerialObject`s.
      - `public: bool virtualizePointers(const std::unordered_map<Address, Address>&)` Replace the real addresses of all children pointers with the corresponding virtual address. Returns `false` if a pointer could not be mapped. Also passes the invocation to all children `SerialObject`s.
      - `public: bool restorePointers(const std::unordered_map<Address, Address>&)` Replace the virtual addresses of all children pointers with the corresponding real address. Returns `false` if a pointer could not be mapped. Also passes the invocation to all children `SerialObject`s.
      - `public: void setRealAddress(Address)` Set the objects real address.
    - `class SerialPointer` A class representing a serialized pointer.
      - `public: SerialPointer()` A default constructor.
      - `public: SerialPointer(unsigned int, std::string, void**)` A constructor from data.
      - `public: std::string get() const override` An implementation of `Serial::get`.
      - `public: void set(const std::string&) override` An implementation `Serial::set`.
      - `public: std::string getName() const override` An implementation `Serial::getName`.
      - `public: unsigned int getClass()` Returns the class id of the serialized pointer.
      - `public: bool virtualizePointers(const std::unordered_map<Address, Address>&)` Replaces the real address with the corresponding virtual address. Returns `false` if the real value is not mapped.
      - `public: bool restorePointers(const std::unordered_map<Address, Address>&)` Replaces the virtual address with the corresponding real address. Returns `false` if the virtual value is not mapped. Also updates and validates the original pointer.
    - `namespace string` A namespace grouping function working with strings.
      - `std::string makeString(const std::initializer_list<std::string>&)` Concatenates multiple strings into one.
      - `std::string substring(const std::string&, std::size_t, std::size_t)` Substring with with start and end.
      - `std::string replaceAll(const std::string&, const std::string&, const std::string&)` Replaces every occurrence of the second argument in the first one with the third one.
      - `std::string connect(const std::vector<std::string>&, char)` Connects multiple strings into one.
      - `std::vector<std::string> split(const std::string&, char)` Splits a string at a delimiter. Keeps strings between `{` and `}` together.
      - `std::string indent(const std::string&)` Indents every line in a string.
      - `std::string unindent(const std::string&)` Un-indents every line in a string.
      - `template <typename T> const constexpr char* TypeToString` a string representing the provided type.
      - `template <typename T> std::string serializePrimitive(const T& val)` Serialize a primitive value.
      - `template <typename T> std::optional<T> deserializePrimitive(const std::string&)` Deserialize a string to a primitive value.
      - `std::optional<std::array<std::string, 3>> parsePrimitive(const std::string&)`
      - `std::optional<std::array<std::string, 4>> parseObject(const std::string&)`
      - `std::optional<std::array<std::string, 3>> parsePointer(const std::string&)`
    - `concept SerializablePrimitive` A concept for a type that can be serialized and deserialized.

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
safe_char = <any character except newline and equals>;
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
- All files in this repo (except the `serializable.hpp`) can safely be ignored: They are just meta-files, IDE settings and tests. Speaking about tests: I know, the tests implemented in the `main.cpp` file are as bad as I earlier on described my previous pseudo-stable serializers, but they are good enough for what they have to do. So don't judge me for the tests, judge me for the library. Also if you have an afternoon to spare, I would love to see proper testing some day.

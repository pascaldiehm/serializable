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

It should be noted that there are types of data that cannot be serialized.
The following quite common situations won't work with serialization:

- References: If you know just a tiny bit about how computers store data in RAM, it shouldn't come as a surprise, that it is complete nonsense to serialize references and pointers. The only thing serializing an object is its owner. If you want to represent relations between objects, you will have to implement an artificial (and serializable) identification system.
- Circles: While it is possible to serialize circular references, it is not possible to finish serialization of such an object. Put simply, serialization of circular references (the references you shouldn't serialize anyways, see above) _will_ (not might) lead to infinite loops and either your drive running out of space or your program exceeding recursion depth (both situations you would usually aim to avoid).

Note that these are not the limitations of this library but the limitations of (basic) serialization in general.
There may be ways to circumvent these limitations (circle detection, serialization-given IDs instead of addresses) but they are simply not included in the library (at least not yet ;-)).

## Usage

### Installation

This is a header-only library.
So all you have to do to include it into your project is to download `serializable.hpp` into your include path.

Note: Even though this project looks like it can be build using CMake, it can't.
The `CMakeLists.txt` file is just used for building some tests.

### Quickstart

For starters: Every data structure you wish to serialize has to extend the base class `serializable::Serializable`.
Doing so will add five public methods to your class:

1. `std::string serialize()`: Runs serialization and returns the serialized data.
2. `bool check(const std::string&)`: Returns whether the given string can be deserialized into this class.
3. `bool deserialize(const std::string&)`: Deserializes the given string into the class. Returns whether the deserialization was successful. The method will automatically abort (without changing any data) if `check` returns false.
4. `bool load(const std::filesystem::path&)`: Loads the given file and deserializes into the class. Returns false when the file is not found or the deserialization failed.
5. `bool save(const std::filesystem::path&)`: Serializes the class into the given file. Returns false if the specified file could not be written to.

Any class extending the `Serializable` class has to override an abstract method: `void exposed()`.
This is where you declare which data you would like to be serialized/deserialized.
For every variable you want to serialize, you have to call `void expose(const char* name, T& value)`.

For `name` you can generally pass any string you want.
It's a good idea to take the name of the variable, to avoid duplication and confusion.
It's a bad idea to use strings containing `{`, `}`, `"` or an equals sign (`=`) surrounded by two spaces as this might confuse the parser.
Every other name _should_ be fine though.

For `value` you usually simply have to pass in the name of the variable - C++ will automatically pass it as a reference (as requested by the `expose` function).
The type of `value` should be a primitive type (`bool`, `[unsigned] char`, `[unsigned] short`, `[unsigned] int`, `[unsigned] long`, `double`, `float`), `std::string`, some class extending `serializable::Serializable` or an enum.

The given functions will now serialize/deserialize any variable exposed in this way.
Note that it is also possible to apply some pre-/postprocessing to your variables inside of your `exposed` function.
(You can even copy a member variable to a local variable, expose the local variable and then copy the local variable back to the member variable).
Just keep in mind, that the `exposed` function is usually called three times per lifecycle: deserializing, checking and serializing all call `exposed`.
(This could be 'fixed' by writing different functions for serializing/deserializing but the library is intentionally build in this way to prevent code duplication).

### Documentation

#### Class serializable::Serializable

- `public: Serializable(...)` The class provides a default constructor (without arguments), the default copy constructor/assignment operator, a virtual default destructor and no move constructor/assignment operator.
- `public: std::string serialize()` Return the serialized data of all exposed variables.
- `public: bool check(const std::string&)` Return whether the given string provides all required fields for this class. (Note: It is fine if the string provides more data than required!)
- `public: bool deserialize(const std::string&)` Deserialize the give string into the class and return whether that worked. This includes a call to `check`.
- `public: bool load(const std::filesystem::path&)` Load a file and deserialize its content. Returns whether the file could be loaded and the deserialization worked.
- `public: bool save(const std::filesystem::path&)` Serialize data to a file. Returns whether the file could be written to.
- `protected: virtual void exposed()` Abstract function that is called to get exposed fields.
- `protected: void expose(const char*, bool&)` Expose a bool value.
- `protected: void expose(const char*, char&)` Expose a char value.
- `protected: void expose(const char*, unsigned char&)` Expose an unsigned char value.
- `protected: void expose(const char*, short&)` Expose a short value.
- `protected: void expose(const char*, unsigned short&)` Expose an unsigned short value.
- `protected: void expose(const char*, int&)` Expose an int value.
- `protected: void expose(const char*, unsigned int&)` Expose an unsigned int value.
- `protected: void expose(const char*, long&)` Expose a long value.
- `protected: void expose(const char*, unsigned long&)` Expose an unsigned long value.
- `protected: void expose(const char*, float&)` Expose a float value.
- `protected: void expose(const char*, double&)` Expose a double value.
- `protected: void expose(const char*, std::string&)` Expose a string value.
- `protected: void expose(const char*, Serializable&)` Expose a Serializable subclass.
- `protected: void expose(const char*, enum&)` Expose an enum value.

#### Class serializable::Array

This class extends `serializable::Serializable` and `std::array`.
It overwrites the `expose` function with a useful implementation.
Therefore it is a near-drop-in replacement for `std::array` that can be serialized.

#### Class serializable::Vector

This class extends `serializable::Serializable` and `std::vector`.
It overwrites the `expose` function with a useful implementation.
Therefore it is a near-drop-in replacement for `std::vector` that can be serialized.

#### Class serializable::List

This class extends `serializable::Serializable` and `std::list`.
It overwrites the `expose` function with a useful implementation.
Therefore it is a near-drop-in replacement for `std::list` that can be serialized.

### Save file syntax

The safe file contains one line per exposed field (the only exception being Serializable subclasses) consisting of the type, the name and the value of the variable.

```EBNF
line      = type ' ' name ' = ' value;
type      = 'BOOL' | 'CHAR' | 'UCHAR' | 'SHORT' | 'USHORT' | 'INT' | 'UINT' |
            'LONG' | 'ULONG' | 'FLOAT' | 'DOUBLE' | 'STRING' | 'OBJECT' | 'ENUM';
name      = safe_char {safe_char};
value     = ('true' | 'false') | (['-'] digit {digit} ['.' digit {digit}]) |
            '"' char '"' | '{}' | '{\n\t' line {'\n\t' line} '\n}';

digit     = ('0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9');
char      = <any char, '"' gets replaced with '&quot;'>;
safe_char = <any char except '{', '}', '"' and '='>;
```

Note that the save files are quite human-friendly.
That means this library can also be used as a configuration file manager.

## Contributing

If you find a bug, missing a feature or just have an awesome idea you would like to add to this library, I would love to receive a pull request from you.
The only non-trivial\* requirement I have is that the library has to stay a header-only library.

<sub>\* non-trivial meaning that obviously PRs deleting everything and making this library a Fibonacci-Calculator will not be merged.</sub>

## Notes

- Note that all classes using this serialization have to have a default value. Loading in the context of this library means you _overwrite_ existing data, not _create_ new data (that is why I usually call the process deserializing _into_ a class). First create default data, then (try to) overwrite it with stored data.
- About runtime. Without nesting, the worst-case runtime is actually O(n²), if the deserializer has to search the entire save file to find the value for the current variable at the end. But since the `exposed` function usually doesn't change between serializing and deserializing, _usually_ the requested value is at the top of the file, which makes finding it incredibly fast (as loaded data gets removed when read). Therefore I would expect `deserialize` (`serialize` anyways) to run in O(n). The only problem here is `check`. This function cannot remove any data and therefore runs in O(n²), also slowing down `deserialize` (_Might_ be fixed some day). Nesting also complicates things even further, as nested data is read by both the nester (parses serialized data for subclass) and the nested class (parses values).
- All files in this repo (except the `serializable.hpp`) can safely be ignored: They are just meta-files, IDE settings and tests. Speaking about tests: I know, the tests implemented in the `main.cpp` file are as bad as I earlier on described my previous pseudo-stable serializers, but they are good enough for what they have to do. So don't judge me for the tests, judge me for the library. Also if you have an afternoon to spare, I would love to see proper testing some day.

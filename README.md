# jschema-cpp

This repository contains an application that generates C++ classes and import/export code from a [JSON schema](https://json-schema.org/).
Currently, the source files are generated to work with boost::json, but multiple backends are possible simply by editing
the [inja templates]().

## Validation

It is intended to eventually add static validation features. These will be documented in the 'features' matrix below.

## Supported schema features

This is intended to generate bare-bones C++ `struct` files containing the data members in the JSON Schema of `"type": "object"`. This library is designed to generate a *minimal* amount of code for the given schema. For this reason, validation is optional and will be placed in a separate, procedural function that validates each field individually.

Consult the matrix below for features that are supported so far

    type: string [x]
        minLength [ ]
        maxLength [ ]
        pattern [ ]
        format=date-time [ ]
        format=uuid [ ]
    type: string with enum [x]
    type: integer [x]
        maximum [ ]
        minimum [ ]
        exclusiveMaximum
        exclusiveMinimum
        multipleOf
    type: number [x]
        maximum
        minimum
        exclusiveMaximum
        exclusiveMinimum
        multipleOf
    type: boolean [x]
    type: null [ ]
    type: array [ ]
        items [ ]
        minItems [ ]
        maxItems [ ]
    type: object [x]
        properties [x]
        required [ ]
    allOf [ ]
    anyOf [ ]
    oneOf [ ] 

## References

$ref references are supported for array items and object properties originating from the same document, with unique key names in the entire document

## Support for other JSON parsers

Adding support for other JSON parsers should be quite simple. Support is solely baked-in through the `templates` directory.

## Build system integration

This tool can be used with any build system that supports adding a custom executable target. The binary produced has no dependencies apart from the system C++ runtime.
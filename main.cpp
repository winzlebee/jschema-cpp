#include <iostream>
#include <string_view>
#include <fstream>
#include <string>
#include <set>
#include <stack>
#include <string_view>

#include "nlohmann/json.hpp"
#include "inja/inja.hpp"

namespace nl = nlohmann;

namespace jschema {

enum TokenType {
  UNKNOWN,
  INTEGER,
  NUMBER,
  BOOLEAN,
  STRING,
  OBJECT,
  ARRAY,
};

const std::map<std::string, TokenType> TOKEN_TYPES = {
  {"integer", INTEGER},
  {"number", NUMBER},
  {"boolean", BOOLEAN},
  {"string", STRING},
  {"object", OBJECT},
  {"reference", OBJECT},
  {"array", ARRAY}
};

static std::map<TokenType, std::string> CPP_TYPES = {
  {INTEGER, "int"},
  {NUMBER,  "double"},
  {BOOLEAN, "bool"},
  {STRING,  "std::string"},
  {ARRAY, "std::vector"},
};

void loadCppTypes()
{
  std::ifstream inTypes("templates/types.json");
  auto json = nl::json::parse(inTypes);

  for (const auto &tpItems : json.items()) {
    if (!TOKEN_TYPES.count(tpItems.key())) {
      std::cerr << "C++ Overload for unknown type: " << tpItems.key();
      continue;
    }

    auto tpEnum = TOKEN_TYPES.at(tpItems.key());

    CPP_TYPES[tpEnum] = tpItems.value();
  }
}

// Makes a string conform to Camel Case
// IE: Remove spaces and underscores and capitalise subsequent words
std::string camelCase(const std::string &input)
{
  std::string result;
  bool forceUpper = false;
  for (std::size_t i = 0; i < input.size(); ++i) {
    char c = forceUpper ? std::toupper(input[i]) : input[i];
    if (std::isalpha(static_cast<unsigned char>(c))) {
      result += c;
    }
    forceUpper = false;
    if (c == '_' || c == ' ') {
      forceUpper = true;
    }
  }

  return result;
}

// Camel case with first letter capitalised
std::string pascalCase(const std::string &input)
{
  std::string result = camelCase(input);

  if (result.size()) {
    result[0] = std::toupper(result[0]);
  }

  return result;
}

struct SchemaParser : nl::json_sax<nl::json>
{
  SchemaParser(const std::string &baseClassName = "Base")
    : m_currentVariable(baseClassName)
  {
    m_isPropertiesStack.push(false);
    m_isArrayItemsStack.push(false);
  }

  virtual ~SchemaParser()
  {
  }

  // We've begun to encounter some object properties
  virtual void begin_object_properties(std::string name) = 0;
  virtual void object_property_number(std::string name) = 0;
  virtual void object_default_number(std::string variable, double number) = 0;
  virtual void object_property_int(std::string name) = 0;
  virtual void object_default_int(std::string variable, int number) = 0;
  virtual void object_property_string(std::string name) = 0;
  virtual void object_default_string(std::string variable, std::string string) = 0;
  virtual void object_property_boolean(std::string name) = 0;
  virtual void object_default_boolean(std::string variable, bool boolean) = 0;
  virtual void object_property_ref(std::string name, std::string target) = 0;
  virtual void object_property_enum_element(std::string variable, std::string name) = 0;
  virtual void object_property_array(std::string name) = 0;
  virtual void end_object_properties() = 0;

  bool null() override
  {
    std::cerr << "null";
    return false;
  }

  bool binary(binary_t &val) override
  {
    std::cerr << "binary";
    return false;
  }

  // Called when a boolean is parsed; value is passed
  bool boolean(bool val)
  {
    if (m_default) {
      if (!is_type_consistent(BOOLEAN)) {
        return false;
      }
      
      object_default_boolean(m_currentVariable, val);
      return true;
    }

    std::cerr << "JSON parse error bool." << std::endl;

    return false;
  }

  // called when a signed or unsigned integer number is parsed; value is passed
  bool number_integer(number_integer_t val)
  {
    if (m_default) {
      if (!is_type_consistent(INTEGER)) {
        return false;
      }
      
      object_default_int(m_currentVariable, val);
      return true;
    }

    std::cerr << "JSON parse error int." << std::endl;

    return false;
  }

  bool number_unsigned(number_unsigned_t val)
  {
    return number_integer(val);
  }

  // called when a floating-point number is parsed; value and original string is passed
  bool number_float(number_float_t val, const string_t& s)
  {
    if (m_default) {
      if (!is_type_consistent(NUMBER)) {
        return false;
      }
      
      object_default_number(m_currentVariable, val);
      return true;
    }

    std::cerr << "JSON parse error float." << std::endl;

    return false;
  }

  bool is_type_consistent(TokenType tp)
  {
    bool consistent = m_typeStack.top() == UNKNOWN ||
                      m_typeStack.top() == tp;

    if (!consistent) {
      std::cerr << "type not consistent!"
                << "Last type: " << m_typeStack.top()
                << "This type: " << tp;
    }

    return consistent;                      
  }

  // called when a string is parsed; value is passed and can be safely moved away
  bool string(string_t& val)
  {
    if (m_unsupported) {
      std::cout << "WARNING: Skipping unsupported tag for class " << m_currentVariable << std::endl;
      return true;
    }

    if (m_ref) {
      // The last part of the reference key will be the target
      std::size_t lastSlash = val.find_last_of('/');
      object_property_ref(m_currentVariable, val.substr(lastSlash + 1));
      return true;
    }

    if (m_enum) {
      if (!is_type_consistent(STRING)) {
        std::cerr << "ENUM elements are only supported for strings";
        return false;
      }

      // If we encounter an enum then the type must be string
      object_property_enum_element(m_currentVariable, val);
      m_typeStack.top() = STRING;
      return true;
    }

    if (m_type) {

      if (!TOKEN_TYPES.count(val)) {
        std::cout << "Token type does not exist: " << val;
        return false;
      }

      const auto &tp = TOKEN_TYPES.at(val);

      // check for inconsistency between "default" and "type"
      if (!is_type_consistent(tp)) {
        std::cerr << "Type is not consistent! Was previously implied to be " << tp;
        return false;
      }

      // Rewrite the last seen token to the val
      m_typeStack.top() = tp;

      switch (tp) {
        case INTEGER: 
          object_property_int(m_currentVariable);
          break;
        case NUMBER:
          object_property_number(m_currentVariable);
          break;
        case BOOLEAN:
          object_property_boolean(m_currentVariable);
          break;
        case STRING:
          object_property_string(m_currentVariable);
          break;
        case OBJECT:
          if (m_isPropertiesStack.size() > 2) {
            object_property_ref(m_currentVariable, pascalCase(m_currentVariable));
          }
          break;
        case ARRAY:
          object_property_array(m_currentVariable);
          break;
      }

      return true;
    }

    if (m_default) {

      if (!is_type_consistent(STRING)) {
        return false;
      }

      object_default_string(m_currentVariable, val);
      return true;
    }

    std::cerr << "JSON parse error string." << std::endl;

    return false;
  }

  bool start_object(std::size_t elements)
  {
    // Token types within a stack shouldn't contradict each other
    m_typeStack.push(UNKNOWN);
    m_isArrayItemsStack.push(m_isArrayItems);

    if (m_isPropertiesStack.top()) {
      begin_object_properties(pascalCase(m_currentVariable));
    }

    m_isPropertiesStack.push(false);

    return true;
  }

  bool end_object()
  {
    m_typeStack.pop();
    m_isArrayItemsStack.pop();

    m_isPropertiesStack.pop();
    if (m_isPropertiesStack.top()) {
      end_object_properties();
    }

    return true;
  }

  bool start_array(std::size_t elements)
  {
    return true;
  }

  bool end_array()
  {
    if (!m_enum) {
      std::cerr << "End of array, but no enum!";
      return false;
    }

    m_enum = false;
    return true;
  }

  bool key(string_t& val)
  {
    m_ref = false;
    m_type = false;
    m_default = false;
    m_unsupported = false;
    m_isArrayItems = false;

    std::cout << val << std::endl;

    if (UNSUPPORTED_TOKENS.count(val)) {
      m_unsupported = true;
      return true;
    }

    if (!NON_TOKEN_ATTRIBUTES.count(val)) {
      m_currentVariable = val;
      return true;
    }

    if (val == PROPERTIES_KEY) {
      m_isPropertiesStack.top() = true;
      return true;
    }

    if (val == ARRAY_ITEMS_KEY) {
      m_isArrayItemsStack.top() = true;
      m_isArrayItems = true;
      return true;
    }

    if (val == REFERENCE_KEY) {
      m_ref = true;
      return true;
    }

    if (val == TYPE_KEY) {
      m_type = true;
      return true;
    }

    if (val == DEFAULT_KEY) {
      m_default = true;
      return true;
    }

    if (val == ENUM_KEY) {
      m_enum = true;
      return true;
    }

    std::cerr << "Bad Key: " << val;

    return false;
  }

  // called when a parse error occurs; byte position, the last token, and an exception is passed
  bool parse_error(std::size_t position, const std::string& last_token, const nl::detail::exception& ex)
  {
    std::cerr << "JSON parse error." << std::endl;
    return false;
  }

private:
  std::string m_currentVariable;

  // Valid value-tags that can be encountered
  bool m_ref = false;
  bool m_type = false;
  bool m_default = false;
  bool m_object = false;
  bool m_unsupported = false;
  bool m_enum = false;
  bool m_isArrayItems = false;

  std::stack<TokenType> m_typeStack;

  // Whether the current object represents properties
  std::stack<bool> m_isPropertiesStack;
  std::stack<bool> m_isArrayItemsStack;

  // Attributes that, when encountered, are not used for class naming
  const std::string PROPERTIES_KEY = "properties";
  const std::string TYPE_KEY = "type";
  const std::string DEFAULT_KEY = "default";
  const std::string REFERENCE_KEY = "$ref";
  const std::string ENUM_KEY = "enum";
  const std::string ARRAY_ITEMS_KEY = "items";
  const std::set<std::string> NON_TOKEN_ATTRIBUTES = {PROPERTIES_KEY, TYPE_KEY, DEFAULT_KEY, REFERENCE_KEY, "items", "enum"};
  const std::set<std::string> UNSUPPORTED_TOKENS = {"$schema", "$id", "title", "description"};
};

struct SchemaTemplateParser : SchemaParser
{

  SchemaTemplateParser(const std::string &baseClassName)
    : SchemaParser(baseClassName)
  {
    output["objects"] = {};
    output["enums"] = {};
    output["hasUuids"] = false;
  }

  virtual ~SchemaTemplateParser()
  {

  }

  nl::json output;

  protected:
    std::stack<nl::json> m_stack;

    void begin_object_properties(std::string name) override
    {
      nl::json current;

      current["className"] = name;
      current["variables"] = {};

      m_stack.push(current);
    }

    nl::json &getCurrent()
    {
      return m_stack.top();
    }

    void object_property_number(std::string name) override
    {
      getCurrent()["variables"][name]["type"] = "number";
    }

    void object_default_number(std::string name, double number) override
    {
      getCurrent()["variables"][name]["default"] = number;
    }

    void object_property_int(std::string name) override
    {
      getCurrent()["variables"][name]["type"] = "integer";
    }

    void object_default_int(std::string name, int number) override
    {
      getCurrent()["variables"][name]["default"] = number;
    }

    void object_property_string(std::string name) override
    {
      getCurrent()["variables"][name]["type"] = "string";
    }

    void object_default_string(std::string name, std::string string) override
    {
      getCurrent()["variables"][name]["default"] = string;
    }

    void object_property_boolean(std::string name) override
    {
      getCurrent()["variables"][name]["type"] = "boolean";
    }

    void object_default_boolean(std::string name, bool boolean) override
    {
      getCurrent()["variables"][name]["default"] = boolean;
    }

    void object_property_ref(std::string name, std::string target) override
    {
      getCurrent()["variables"][name]["type"] = "reference";
      getCurrent()["variables"][name]["className"] = target;
    }

    void object_property_array(std::string name) override
    {
      getCurrent()["variables"][name]["isArray"] = true;
    }

    void object_property_enum_element(std::string variable, std::string name) override
    {
      object_property_ref(variable, pascalCase(variable));
      output["enums"][pascalCase(variable)].push_back(name);
    }

    void end_object_properties() override
    {
      std::cout << "end object" << std::endl;
      output["objects"].push_back(getCurrent());
      m_stack.pop();
    }

};

}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::cerr << "Requires a schema file, output file and a base class name";
    return 1;
  }

  const char *fName = argv[1];
  const char *ofName = argv[2];

  std::ifstream schemaFile(fName);

  jschema::SchemaTemplateParser tParser("Base");
  nl::json::sax_parse(schemaFile, &tParser);

  std::ofstream outFile("source.h");

  std::cout << tParser.output.dump(4) << std::endl;

  inja::Environment env;
  env.set_trim_blocks(true);
  env.set_lstrip_blocks(true);

  env.add_callback("cppType", 1, [](inja::Arguments &args) {
    auto props = args.at(0)->get<inja::json>();

    auto tp = jschema::TOKEN_TYPES.at(props["type"]);
    std::string cppType;

    if (jschema::CPP_TYPES.count(tp)) {
      cppType = jschema::CPP_TYPES.at(tp);
    }

    if (props.count("className")) {
      cppType = props.at("className");
    }

    if (props.count("isArray")) {
      cppType = jschema::CPP_TYPES.at(jschema::ARRAY) + "<" + cppType + ">";
    }

    return cppType;
  });

  env.render_to(outFile, env.parse_file("templates/source.h.jinja2"), tParser.output);
  
}
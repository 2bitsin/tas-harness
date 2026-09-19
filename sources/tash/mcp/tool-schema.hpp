#pragma once
// The JSON Schema a tool announces, read off the very struct its body reads
// its arguments out of: declared once, so tools/list and the call can never
// disagree (MCP tools 2.4).

#include <oxbox/serialization/concepts.hpp>
#include <oxbox/serialization/reflected-scheme.hpp>
#include <oxbox/serialization/scheme.hpp>

#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tash::mcp::detail::tool_schema
{
  inline constexpr std::string_view OBJECT_TYPE{ "object" };
  inline constexpr std::string_view ARRAY_TYPE{ "array" };

  struct SchemaItems
  {
    friend constexpr auto reflect_scheme(SchemaItems*);

    std::string type;
  };

  struct SchemaProperty
  {
    friend constexpr auto reflect_scheme(SchemaProperty*);

    std::string                type;
    std::string                description;
    std::optional<SchemaItems> items;
  };

  struct ToolSchema
  {
    friend constexpr auto reflect_scheme(ToolSchema*);

    std::string                           type{ std::string{ OBJECT_TYPE } };
    std::map<std::string, SchemaProperty> properties{ };
    std::vector<std::string>              required{ };
  };

  template <typename T> struct HeldT              { using type = T; };
  template <typename T> struct HeldT<std::optional<T>> { using type = T; };

  template <typename> struct IsVectorT              : std::false_type { };
  template <typename T> struct IsVectorT<std::vector<T>> : std::true_type { };

  template <typename T> struct ElementT             { using type = T; };
  template <typename T> struct ElementT<std::vector<T>> { using type = T; };

  template <typename Value>
  [[nodiscard]] consteval auto JsonTypeOf() -> std::string_view
  {
    if constexpr (std::same_as<Value, bool>)       return "boolean";
    else if constexpr (std::integral<Value>)       return "integer";
    else if constexpr (std::floating_point<Value>) return "number";
    else                                           return "string";
  }

  // A member that can be absent is optional; everything else is required,
  // which is the one rule that keeps the schema and the decode in step.
  template <typename Field>
  auto Announce(ToolSchema& schema, Field const& field) -> void
  {
    using Declared = typename Field::value_type;
    constexpr bool spare{
      oxbox::serialization::IsStdOptionalT<Declared>::value };
    using Held = typename HeldT<Declared>::type;

    SchemaProperty property;
    property.description = std::string{ field.description };
    if constexpr (IsVectorT<Held>::value)
    {
      property.type = std::string{ ARRAY_TYPE };
      property.items = SchemaItems{ std::string{
        JsonTypeOf<typename ElementT<Held>::type>() } };
    }
    else
      property.type = std::string{ JsonTypeOf<Held>() };

    schema.properties.emplace(std::string{ field.WireName() },
                              std::move(property));
    if constexpr (!spare)
      schema.required.emplace_back(field.WireName());
  }

  template <typename Args>
  [[nodiscard]] auto SchemaOf() -> ToolSchema
  {
    ToolSchema schema;
    if constexpr (oxbox::serialization::HasScheme<Args>)
      std::apply([&schema](auto const&... fields)
                 { (Announce(schema, fields), ...); },
                 oxbox::serialization::SchemeFor(Args{ }).fields);
    return schema;
  }
}

namespace tash::mcp
{
  using detail::tool_schema::SchemaOf;
  using detail::tool_schema::SchemaProperty;
  using detail::tool_schema::ToolSchema;
}

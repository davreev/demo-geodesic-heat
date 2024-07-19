#pragma once

#include <happly.h>

#include <dr/span.hpp>

namespace dr
{

happly::Property* get_property(happly::Element& element, std::string const& name)
{
    if (!element.hasProperty(name))
        return nullptr;

    return element.getPropertyPtr(name).get();
}

template <typename T>
Span<T const> get_property_data(happly::Property const* prop)
{
    using namespace happly;
    auto derived = dynamic_cast<TypedProperty<T> const*>(prop);
    return (derived) ? as_span(derived->data) : Span<T const>{};
}

template <typename T>
Span<T const> get_property_data(happly::Element& element, std::string const& name)
{
    return get_property_data<T>(get_property(element, name));
}

template <typename T>
void get_list_property_data(
    happly::Property const* prop,
    Span<T const>& list_data,
    Span<usize const>& list_starts)
{
    using namespace happly;
    auto derived = dynamic_cast<TypedListProperty<T> const*>(prop);

    if (derived)
    {
        list_data = as_span(derived->flattenedData);
        list_starts = as_span(derived->flattenedIndexStart);
    }
    else
    {
        list_data = {};
        list_starts = {};
    }
}

template <typename T>
void get_list_property_data(
    happly::Element& element,
    std::string const& name,
    Span<T const>& list_data,
    Span<usize const>& list_starts)
{
    get_list_property_data(get_property(element, name), list_data, list_starts);
}

template <typename T>
Span<T const> get_list_property_data(happly::Property const* prop)
{
    Span<T const> data;
    Span<usize const> _;
    get_list_property_data(prop, data, _);
    return data;
}

template <typename T>
Span<T const> get_list_property_data(happly::Element& element, std::string const& name)
{
    return get_list_property_data<T>(get_property(element, name));
}

} // namespace dr
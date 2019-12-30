#include <fstream>
#include <iomanip>
#include <iostream>
#include <utility>

#include "jconf/jconf.hpp"

namespace jconf
{
using json_pointer = nlohmann::json_pointer<json>;
using invalid_iterator = nlohmann::json::invalid_iterator;

Config::Config(std::string storage_path, std::string schema_path)
    : m_storage_path(std::move(storage_path)),
      m_schema_path(std::move(schema_path))
{
}

void Config::load()
{
    m_mutex.lock();

    std::ifstream(m_storage_path) >> m_data;
    std::ifstream(m_schema_path) >> m_schema;

    // Fully validate the schema with a temporary validator object
    json_validator init_validator;
    init_validator.set_root_schema(m_schema);
    init_validator.validate(m_data);

    // Remove the 'required' field to avoid insertion runtime issues
    remove(m_schema, "required");

    m_validator.set_root_schema(m_schema);

    m_mutex.unlock();
}

void Config::save()
{
    std::ofstream o(m_storage_path);

    m_mutex.lock();

    o << std::setw(4) << m_data << std::endl;

    m_mutex.unlock();
}

std::ostream &operator<<(std::ostream &os, const Config &c)
{
    return os << c.m_data;
}

void Config::set(const json &property)
{
    m_mutex.lock();

    m_validator.validate(property);
    m_data.merge_patch(property);

    m_mutex.unlock();
}

void Config::set(const std::string &key, const json &property)
{
    m_mutex.lock();

    if (key.rfind('/', 0) == 0)
    {
        // If the key starts with "/", then it is a path, like "logging/level"
        json_pointer jp(key);
        m_data.at(jp) = property;
    }
    else
    {
        m_data.at(key) = property;
    }

    m_mutex.unlock();
}

json Config::get(const std::string &key)
{
    m_mutex.lock();

    json ret;

    if (key.compare("/") == 0)
    {
        ret = m_data;
    }
    else if (key.rfind('/', 0) == 0)
    {
        // If the key starts with "/", then it is a path, like "logging/level"
        json_pointer jp(key);
        ret = m_data.at(jp);
    }
    else
    {
        ret = m_data.at(key);
    }

    m_mutex.unlock();

    return ret;
}

void Config::remove(json &j, const std::string &key)
{
    m_mutex.lock();

    j.erase(key);
    for (auto it = j.begin(); it != j.end(); ++it)
    {
        // Keep recursing if the iterator is an array/object to remove nested
        // keys
        if (it->is_structured())
        {
            remove(*it, key);
        }
    }

    m_mutex.unlock();
}

}  // namespace jconf

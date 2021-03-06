#ifndef OPTIONS_H
#define OPTIONS_H

#include <unordered_map>
#include <string>

#include "errorcode.h"

class Options
{
private:
    enum class ValueType
    {
        INT,
        BOOL,
        COLOR,
        STRING,
    };

public:
    Options();
    ErrorCode set_option(const std::string& opt, const std::string& val);
    bool validate(ValueType val_type, const std::string& val);
    std::string get_value(const std::string& opt);

private:
    std::unordered_map<std::string, std::pair<ValueType, std::string>> value_map;
};

#endif // OPTIONS_H

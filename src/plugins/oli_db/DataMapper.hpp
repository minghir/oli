#ifndef DATA_MAPPER_HPP
#define DATA_MAPPER_HPP

#include "dbConnection.hpp" // Aici ai enum-ul vNativeDataType

namespace DataMapper {

    // Translator pentru DBF (FoxPro)
    inline vNativeDataType FromDBF(char type, int decimals) {
        switch(type) {
            case 'C': return vNativeDataType::V_TEXT;
            case 'N': return (decimals > 0) ? vNativeDataType::V_DOUBLE : vNativeDataType::V_INTEGER;
            case 'D': return vNativeDataType::V_DATE;
            case 'L': return vNativeDataType::V_BOOLEAN;
            default:  return vNativeDataType::V_TEXT;
        }
    }

    // Aici vei adăuga în viitor:
    // inline vNativeDataType FromPostgres(const std::string& pgType) { ... }
    // inline vNativeDataType FromMySQL(const std::string& myType) { ... }
}

#endif
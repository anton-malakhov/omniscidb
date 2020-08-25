/*
 * Copyright 2020 OmniSci, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <Shared/sqltypes.h>
#include <parquet/schema.h>
#include <parquet/types.h>
#include <set>

#include "Catalog/ColumnDescriptor.h"

namespace foreign_storage {

/**
 *  All allowed mappings between logical (or physical) parquet and omnisci types
 *  for which parquet statistics can be used in a metadata scan.
 */
struct AllowedParquetMetadataTypeMappings {
  inline static bool isColumnMappingSupported(
      const ColumnDescriptor* omnisci_desc,
      const parquet::ColumnDescriptor* parquet_desc) {
    auto column_type = omnisci_desc->columnType;
    auto logical_type = parquet_desc->logical_type();
    if (logical_type->is_none()) {  // Fallback on physical type
      const auto type = column_type.is_decimal() ? decimal_to_int_type(column_type)
                                                 : column_type.get_type();
      return physical_type_mappings.find({type, parquet_desc->physical_type()}) !=
             physical_type_mappings.end();
    }
    if (validateIntegralMapping(column_type, logical_type)) {
      return true;
    }
    if (validateDecimalMapping(column_type, logical_type)) {
      return true;
    }
    return false;
  }

 private:
  inline const static std::set<std::tuple<SQLTypes, parquet::Type::type>>
      physical_type_mappings{
          {kBOOLEAN, parquet::Type::BOOLEAN},
          {kTINYINT, parquet::Type::INT32},
          {kTINYINT, parquet::Type::INT64},
          {kSMALLINT, parquet::Type::INT32},
          {kSMALLINT, parquet::Type::INT64},
          {kINT, parquet::Type::INT32},
          {kINT, parquet::Type::INT64},
          {kBIGINT, parquet::Type::INT32},
          {kBIGINT, parquet::Type::INT64},
          {kFLOAT, parquet::Type::FLOAT},
          {kFLOAT, parquet::Type::DOUBLE},
          {kDOUBLE, parquet::Type::FLOAT},
          {kDOUBLE, parquet::Type::DOUBLE},
      };

  inline static bool validateIntegralMapping(
      const SQLTypeInfo& column_type,
      const std::shared_ptr<const parquet::LogicalType>& logical_type) {
    if (logical_type->is_int() && column_type.is_integer()) {
      auto int_logical_type =
          dynamic_cast<const parquet::IntLogicalType*>(logical_type.get());
      auto logical_byte_width = int_logical_type->bit_width() / 8;
      auto omnisci_byte_width = column_type.get_size();
      bool is_signed = int_logical_type->is_signed();
      // If parquet type is unsigned, to represent the same range with signed
      // integers, at least one additional bit is required
      if ((is_signed && logical_byte_width <= omnisci_byte_width) ||
          (!is_signed && logical_byte_width < omnisci_byte_width)) {
        return true;
      }
    }
    return false;
  }

  inline static bool validateDecimalMapping(
      const SQLTypeInfo& column_type,
      const std::shared_ptr<const parquet::LogicalType>& logical_type) {
    if (logical_type->is_decimal() && column_type.is_decimal()) {
      auto decimal_logical_type =
          dynamic_cast<const parquet::DecimalLogicalType*>(logical_type.get());
      if (column_type.get_precision() == decimal_logical_type->precision() &&
          column_type.get_scale() == decimal_logical_type->scale()) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace foreign_storage

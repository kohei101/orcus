/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "xlsx_session_data.hpp"

namespace orcus {

xlsx_session_data::formula::formula(
    spreadsheet::sheet_t sheet, spreadsheet::row_t row, spreadsheet::col_t column,
    const std::string& exp) :
    sheet(sheet), exp(exp)
{
    ref.column = column;
    ref.row = row;
}

xlsx_session_data::array_formula::array_formula(
    spreadsheet::sheet_t sheet, const spreadsheet::range_t& ref, const std::string& exp) :
    sheet(sheet),
    ref(ref),
    exp(exp),
    results(
        std::make_shared<range_formula_results>(
            ref.last.row-ref.first.row+1,
            ref.last.column-ref.first.column+1))
{
}

xlsx_session_data::shared_formula::shared_formula(
    spreadsheet::sheet_t sheet, spreadsheet::row_t row, spreadsheet::col_t column, size_t identifier) :
    sheet(sheet), row(row), column(column), identifier(identifier), master(false) {}

xlsx_session_data::shared_formula::shared_formula(
    spreadsheet::sheet_t sheet, spreadsheet::row_t row, spreadsheet::col_t column,
    size_t identifier, const std::string& formula) :
    sheet(sheet), row(row), column(column),
    identifier(identifier), formula(formula), master(true) {}

xlsx_session_data::~xlsx_session_data()
{
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

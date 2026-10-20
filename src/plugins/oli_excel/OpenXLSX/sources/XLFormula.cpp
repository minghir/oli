/*
SPDX-License-Identifier: BSD-3-Clause
SPDX-FileCopyrightText: Copyright (c) 2018, Kenneth Troldal Balslev

   ____                               ____      ___ ____       ____  ____      ___
  6MMMMb                              `MM(      )M' `MM'      6MMMMb\`MM(      )M'
 8P    Y8                              `MM.     d'   MM      6M'    ` `MM.     d'
6M      Mb __ ____     ____  ___  __    `MM.   d'    MM      MM        `MM.   d'
MM      MM `M6MMMMb   6MMMMb `MM 6MMb    `MM. d'     MM      YM.        `MM. d'
MM      MM  MM'  `Mb 6M'  `Mb MMM9 `Mb    `MMd       MM       YMMMMb     `MMd
MM      MM  MM    MM MM    MM MM'   MM     dMM.      MM           `Mb     dMM.
MM      MM  MM    MM MMMMMMMM MM    MM    d'`MM.     MM            MM    d'`MM.
YM      M9  MM    MM MM       MM    MM   d'  `MM.    MM            MM   d'  `MM.
 8b    d8   MM.  ,M9 YM    d9 MM    MM  d'    `MM.   MM    / L    ,M9  d'    `MM.
  YMMMM9    MMYMMM9   YMMMM9 _MM_  _MM_M(_    _)MM_ _MMMMMMM MYMMMM9 _M(_    _)MM_
            MM
            MM
           _MM_

  Copyright (c) 2018, Kenneth Troldal Balslev

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  - Neither the name of the author nor the
    names of any contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

//
// Created by Kenneth Balslev on 27/08/2021.
//

// ===== External Includes ===== //

// ===== OpenXLSX Includes ===== //
#include "XLFormula.hpp"
#include "XLException.hpp"
#include "XLXmlParser.hpp"              // pugixml wrapper
#include "utilities/XLUtilities.hpp"

using namespace OpenXLSX;

/**
 * @details Constructor. Default implementation.
 */
XLFormula::XLFormula() = default;

/**
 * @details Copy constructor. Default implementation.
 */
XLFormula::XLFormula(const XLFormula& other) = default;

/**
 * @details Move constructor. Default implementation.
 */
XLFormula::XLFormula(XLFormula&& other) noexcept = default;

/**
 * @details Destructor. Default implementation.
 */
XLFormula::~XLFormula() = default;

/**
 * @details Copy assignment operator. Default implementation.
 */
XLFormula& XLFormula::operator=(const XLFormula& other) = default;

/**
 * @details Move assignment operator. Default implementation.
 */
XLFormula& XLFormula::operator=(XLFormula&& other) noexcept = default;

/**
 * @details Return the m_formulaString member variable.
 */
std::string XLFormula::get() const { return m_formulaString; }

/**
 * @details Set the m_formulaString member to an empty string.
 */
XLFormula& XLFormula::clear()
{
    m_formulaString = "";
    return *this;
}

/**
 * @details
 */
XLFormula::operator std::string() const { return get(); }

/**
 * @details [private] Constructor. Set the m_cell and m_cellNode objects.
 */
XLFormulaProxy::XLFormulaProxy(XLCell* cell, XMLNode* cellNode)
 : m_cell(cell),
   m_cellNode(cellNode)
{
    assert_always(cell);    // NOLINT
}

/**
 * @details [private] Copy constructor
 */
XLFormulaProxy::XLFormulaProxy(const XLFormulaProxy& other)
 : m_cell(other.m_cell),
   m_cellNode(other.m_cellNode)
{}

/**
 * @details [private] Move constructor. Default implementation.
 */
XLFormulaProxy::XLFormulaProxy(XLFormulaProxy&& other) noexcept = default;

/**
 * @details Destructor. Default implementation.
 */
XLFormulaProxy::~XLFormulaProxy() = default;

/**
 * @details Copy assignment operator. Calls the templated string assignment operator.
 */
XLFormulaProxy& XLFormulaProxy::operator=(const XLFormulaProxy& other)
{
    if (&other != this) {
        *this = other.getFormula();
    }

    return *this;
}

/**
 * @details [private] Move assignment operator. Default implementation.
 */
XLFormulaProxy& XLFormulaProxy::operator=(XLFormulaProxy&& other) noexcept = default;

/**
 * @details
 */
XLFormulaProxy::operator std::string() const { return get(); }

/**
 * @details Returns the underlying XLFormula object, by calling getFormula().
 */
XLFormulaProxy::operator XLFormula() const { return getFormula(); }

/**
 * @details Call the .get() function in the underlying XLFormula object.
 */
std::string XLFormulaProxy::get() const { return getFormula().get(); }

/**
 * @details If a formula node exists, it will be erased.
 */
XLFormulaProxy& XLFormulaProxy::clear()
{
    // ===== Check that the m_cellNode is valid.
    assert_always(m_cellNode != nullptr);       // NOLINT
    assert_always(not m_cellNode->empty());     // NOLINT

    // ===== Remove the value node.
    if (not m_cellNode->child("f").empty()) m_cellNode->remove_child("f");
    return *this;
}

/**
 * @details Convenience function for setting the formula. This method is called from the templated
 * string assignment operator.
 */
void XLFormulaProxy::setFormulaString(const char* formulaString, bool resetValue) // NOLINT
{
    // ===== Check that the m_cellNode is valid.
    assert_always(m_cellNode != nullptr);       // NOLINT
    assert_always(not m_cellNode->empty());     // NOLINT

    if (formulaString[0] == 0) {    // if formulaString is empty
        m_cellNode->remove_child("f");    // clear the formula node
        return;                           // and exit
    }

    // ===== If the cell node doesn't have formula or value child nodes, create them.
    if (m_cellNode->child("f").empty()) m_cellNode->append_child("f");
    if (m_cellNode->child("v").empty()) m_cellNode->append_child("v");

    // ===== Remove the formula type and shared index attributes, if they exist.
    m_cellNode->child("f").remove_attribute("t");
    m_cellNode->child("f").remove_attribute("si");

    // ===== Set the text of the formula and value nodes.
    m_cellNode->child("f").text().set(formulaString);
    if (resetValue) m_cellNode->child("v").text().set(0);

    // BEGIN pull request #189
    // ===== Remove cell type attribute so that it can be determined by Office Suite when next calculating the formula.
    m_cellNode->remove_attribute("t");

    // ===== Remove inline string <is> tag, in case previous type was "inlineStr".
    m_cellNode->remove_child("is");

    // ===== Ensure that the formula node <f> is the first child, listed before the value <v> node.
    m_cellNode->prepend_move(m_cellNode->child("f"));
    // END pull request #189
}

/**
 * @details Creates and returns an XLFormula object, based on the formula string in the underlying
 * XML document.
 */
XLFormula XLFormulaProxy::getFormula() const
{
    assert_always(m_cellNode != nullptr);       // NOLINT
    assert_always(not m_cellNode->empty());     // NOLINT

    const auto formulaNode = m_cellNode->child("f");

    // ===== If the formula node doesn't exist, return an empty XLFormula object.
    if (formulaNode.empty()) return XLFormula();

    // ===== If the formula type is 'shared' or 'array', throw an exception.
    if (not formulaNode.attribute("t").empty() ) {    // 2024-05-28: de-duplicated check (only relevant for performance,
                                                      //  xml_attribute::value() returns an empty string for empty attributes)
        if (std::string(formulaNode.attribute("t").value()) == "shared")
            throw XLFormulaError("Shared formulas not supported.");
        if (std::string(formulaNode.attribute("t").value()) == "array")
            throw XLFormulaError("Array formulas not supported.");
    }

    return XLFormula(formulaNode.text().get());
}

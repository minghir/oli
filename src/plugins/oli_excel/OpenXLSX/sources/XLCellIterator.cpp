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

// ===== External Includes ===== //

// ===== OpenXLSX Includes ===== //
#include "XLCellIterator.hpp"
#include "XLCellRange.hpp"
#include "XLCellReference.hpp"
#include "XLException.hpp"
#include "XLXmlParser.hpp"              // pugixml wrapper
#include "utilities/XLUtilities.hpp"

using namespace OpenXLSX;

namespace OpenXLSX { // utility functions findRowNode and findCellNode
    /**
     * @details
     */
    XMLNode findRowNode(XMLNode sheetDataNode, uint32_t rowNumber, XMLNode hintRowNode)
    {
        if (rowNumber < 1 || rowNumber > OpenXLSX::MAX_ROWS) {
            using namespace std::literals::string_literals;
            throw XLCellAddressError("rowNumber "s + std::to_string( rowNumber ) + " is outside valid range [1;"s + std::to_string(OpenXLSX::MAX_ROWS) + "]"s);
        }

        // ===== Early check: use hintRowNode of available -> implies forward search
        if (not hintRowNode.empty() ) {
            while (hintRowNode.attribute("r").as_ullong() < rowNumber) hintRowNode = hintRowNode.next_sibling_of_type(pugi::node_element);
            if (hintRowNode.attribute("r").as_ullong() > rowNumber)
                return XMLNode{};
            else
                return hintRowNode;
        }

        // ===== Get the last child of sheetDataNode that is of type node_element.
        XMLNode rowNode = sheetDataNode.last_child_of_type(pugi::node_element);

        // ===== If there are now rows in the worksheet, or the requested row is beyond the current max row, return an empty node
        if (rowNode.empty() || (rowNumber > rowNode.attribute("r").as_ullong()))
            return XMLNode{};

        // ===== If the requested node is closest to the end, start from the end and search backwards.
        if (rowNode.attribute("r").as_ullong() - rowNumber < rowNumber) {
            while (not rowNode.empty() && (rowNode.attribute("r").as_ullong() > rowNumber)) rowNode = rowNode.previous_sibling_of_type(pugi::node_element);
            if (rowNode.empty() || (rowNode.attribute("r").as_ullong() != rowNumber))
                return XMLNode{};
        }
        // ===== Otherwise, start from the beginning
        else {
            // ===== At this point, it is guaranteed that there is at least one node_element in the row that is not empty.
            rowNode = sheetDataNode.first_child_of_type(pugi::node_element);

            // ===== It has been verified above that the requested rowNumber is <= the row number of the last node_element, therefore this loop will halt.
            while (rowNode.attribute("r").as_ullong() < rowNumber) rowNode = rowNode.next_sibling_of_type(pugi::node_element);
            if (rowNode.attribute("r").as_ullong() > rowNumber)
                return XMLNode{};
        }

        return rowNode;
    }
    XMLNode findRowNode(XMLNode sheetDataNode, uint32_t rowNumber) { return findRowNode( sheetDataNode, rowNumber, XMLNode() ); }

    /**
     * @details
     */
    XMLNode findCellNode(XMLNode rowNode, uint16_t columnNumber)
    {
        if (columnNumber < 1 || columnNumber > OpenXLSX::MAX_COLS) {
            using namespace std::literals::string_literals;
            throw XLException("XLWorksheet::column: columnNumber "s + std::to_string(columnNumber) + " is outside allowed range [1;"s + std::to_string(MAX_COLS) + "]"s);
        }
        if (rowNode.empty()) return XMLNode{};

        XMLNode cellNode = rowNode.last_child_of_type(pugi::node_element);

        // ===== If there are no cells in the current row, or the requested cell is beyond the last cell in the row...
        if (cellNode.empty() || (XLCellReference(cellNode.attribute("r").value()).column() < columnNumber))
            return XMLNode{};

        // ===== If the requested node is closest to the end, start from the end and search backwards...
        if (XLCellReference(cellNode.attribute("r").value()).column() - columnNumber < columnNumber) {
            while (not cellNode.empty() && (XLCellReference(cellNode.attribute("r").value()).column() > columnNumber))
                cellNode = cellNode.previous_sibling_of_type(pugi::node_element);
            if (cellNode.empty() || (XLCellReference(cellNode.attribute("r").value()).column() < columnNumber))
                return XMLNode{};
        }
        // ===== Otherwise, start from the beginning
        else {
            // ===== At this point, it is guaranteed that there is at least one node_element in the row that is not empty.
            cellNode = rowNode.first_child_of_type(pugi::node_element);

            // ===== It has been verified above that the requested columnNumber is <= the column number of the last node_element, therefore this loop will halt:
            while (XLCellReference(cellNode.attribute("r").value()).column() < columnNumber)
                cellNode = cellNode.next_sibling_of_type(pugi::node_element);
            if (XLCellReference(cellNode.attribute("r").value()).column() > columnNumber)
                return XMLNode{};
        }
        return cellNode;
    }
}    // namespace OpenXLSX


/**
 * @details
 */
XLCellIterator::XLCellIterator(const XLCellRange& cellRange, XLIteratorLocation loc, std::vector<XLStyleIndex> const * colStyles)
    : m_dataNode(std::make_unique<XMLNode>(*cellRange.m_dataNode)),
      m_topLeft(cellRange.m_topLeft),
      m_bottomRight(cellRange.m_bottomRight),
      m_sharedStrings(cellRange.m_sharedStrings),
      m_endReached(false),
      m_hintNode(std::make_unique<XMLNode>()),
      m_hintRow(0),
      m_hintColumn(0),
      m_hintRowNode(std::make_unique<XMLNode>()),
      m_currentCell(),
      m_currentCellStatus(XLNotLoaded),
      m_currentRow(0),
      m_currentColumn(0),
      m_colStyles(colStyles)
{
    if (loc == XLIteratorLocation::End)
        m_endReached = true;
    else {
        m_currentRow    = m_topLeft.row();
        m_currentColumn = m_topLeft.column();
    }
    if (m_colStyles == nullptr)
		 throw XLInternalError("XLCellIterator constructor parameter colStyles must not be nullptr");
// std::cout << "XLCellIterator constructed with topLeft " << m_topLeft.address() << " and bottomRight " << m_bottomRight.address() << std::endl;
// std::cout << "XLCellIterator m_endReached is " << ( m_endReached ? "true" : "false" ) << std::endl;
}

/**
 * @details
 */
XLCellIterator::~XLCellIterator() = default;

/**
 * @details
 */
XLCellIterator::XLCellIterator(const XLCellIterator& other)
    : m_dataNode(std::make_unique<XMLNode>(*other.m_dataNode)),
      m_topLeft      (other.m_topLeft),
      m_bottomRight  (other.m_bottomRight),
      m_sharedStrings(other.m_sharedStrings),
      m_endReached   (other.m_endReached),
      m_hintNode(std::make_unique<XMLNode>(*other.m_hintNode)),
      m_hintRow      (other.m_hintRow),
      m_hintColumn   (other.m_hintColumn),
      m_hintRowNode(std::make_unique<XMLNode>(*other.m_hintRowNode)),
      m_currentCell  (other.m_currentCell),
      m_currentCellStatus(other.m_currentCellStatus),
      m_currentRow   (other.m_currentRow),
      m_currentColumn(other.m_currentColumn),
      m_colStyles    (other.m_colStyles)
{}

/**
 * @details
 */
XLCellIterator::XLCellIterator(XLCellIterator&& other) noexcept = default;

/**
 * @details
 */
XLCellIterator& XLCellIterator::operator=(const XLCellIterator& other)
{
    if (&other != this) {
        *m_dataNode     = *other.m_dataNode;
        m_topLeft       =  other.m_topLeft;
        m_bottomRight   =  other.m_bottomRight;
        m_sharedStrings =  other.m_sharedStrings;
        m_endReached    =  other.m_endReached;
        *m_hintNode     =  *other.m_hintNode;
        m_hintRow       =  other.m_hintRow;
        m_hintColumn    =  other.m_hintColumn;
        *m_hintRowNode  =  *other.m_hintRowNode;
        m_currentCell   =  other.m_currentCell;
        m_currentCellStatus = other.m_currentCellStatus;
        m_currentRow    =  other.m_currentRow;
        m_currentColumn =  other.m_currentColumn;
        m_colStyles     =  other.m_colStyles;
    }

    return *this;
}

/**
 * @details
 */
XLCellIterator& XLCellIterator::operator=(XLCellIterator&& other) noexcept = default;

/**
 * @brief update m_currentCell by fetching (or inserting) a cell at m_currentRow, m_currentColumn
 */
void XLCellIterator::updateCurrentCell(bool createIfMissing)
{
    // ===== Quick exit checks - can't be true when m_endReached
    if (m_currentCellStatus == XLLoaded) return;                         // nothing to do, cell is already loaded
    if (!createIfMissing && m_currentCellStatus == XLNoSuchCell) return; // nothing to do, cell has already been determined as missing

    // At this stage, m_currentCellStatus is XLUnloaded or XLNoSuchCell and createIfMissing == true

    // NOTE: for XLCellReverseIterator, m_beginReached is caught in XLCellReverseIterator::operator++
    if (m_endReached)
        throw XLInputError("XLCellIterator updateCurrentCell: iterator should not be dereferenced when endReached() == true");

    // ===== Cell needs to be updated

    if (m_hintNode->empty()) {   // no hint has been established: fetch first cell node the "tedious" way
        if (createIfMissing)        // getCellNode / getRowNode create missing cells
            m_currentCell = XLCell(getCellNode(getRowNode(*m_dataNode, m_currentRow), m_currentColumn, 0, *m_colStyles), m_sharedStrings.get());
        else                        // findCellNode / findRowNode return an empty cell for missing cells
            m_currentCell = XLCell(findCellNode(findRowNode(*m_dataNode, m_currentRow, *m_hintRowNode), m_currentColumn), m_sharedStrings.get());

        m_hintRowNode = std::make_unique<XMLNode>();        // re-set m_hintRowNode to prevent use before new initialization
    }
    else {
        // ===== Find or create, and fetch an XLCell at m_currentRow, m_currentColumn
        if (m_currentRow == m_hintRow) { // new cell is within the same row
            if (m_currentColumn > m_hintColumn) {
                // ===== Start from m_hintNode and search forwards...
                XMLNode cellNode = m_hintNode->next_sibling_of_type(pugi::node_element);
                uint16_t colNo = 0;
                while (not cellNode.empty()) {
                    colNo = XLCellReference(cellNode.attribute("r").value()).column();
                    if (colNo >= m_currentColumn) break;    // if desired cell was reached / passed, break before incrementing cellNode
                    cellNode = cellNode.next_sibling_of_type(pugi::node_element);
                }
                if (colNo != m_currentColumn) cellNode = XMLNode{}; // if a higher column number was found, set empty node (means: "missing")
                // ===== Create missing cell node if createIfMissing == true
                if (createIfMissing && cellNode.empty()) {
                    cellNode = m_hintNode->parent().insert_child_after("c", *m_hintNode);
                    setDefaultCellAttributes(cellNode, XLCellReference(m_currentRow, m_currentColumn).address(), m_hintNode->parent(),
                    /**/                      m_currentColumn, *m_colStyles);
                }
                m_currentCell = XLCell(cellNode, m_sharedStrings.get()); // cellNode.empty() can be true if createIfMissing == false and cell is not found
            }
            else if (m_currentColumn < m_hintColumn) {
                // ===== Start from m_hintNode and search backwards...
                XMLNode cellNode = m_hintNode->previous_sibling_of_type(pugi::node_element);
                uint16_t colNo = 0;
                while (not cellNode.empty()) {
                    colNo = XLCellReference(cellNode.attribute("r").value()).column();
                    if (colNo <= m_currentColumn) break;    // if desired cell was reached / passed, break before incrementing cellNode
                    cellNode = cellNode.previous_sibling_of_type(pugi::node_element);
                }
                if (colNo != m_currentColumn) cellNode = XMLNode{}; // if a higher column number was found, set empty node (means: "missing")
                // ===== Create missing cell node if createIfMissing == true
                if (createIfMissing && cellNode.empty()) {
                    cellNode = m_hintNode->parent().insert_child_before("c", *m_hintNode);
                    setDefaultCellAttributes(cellNode, XLCellReference(m_currentRow, m_currentColumn).address(), m_hintNode->parent(),
                    /**/                      m_currentColumn, *m_colStyles);
                }
                m_currentCell = XLCell(cellNode, m_sharedStrings.get()); // cellNode.empty() can be true if createIfMissing == false and cell is not found
            }
            else { // m_currentRow == m_hintRow && m_currentColumn == m_hintColumn -> increment or eraseCell on the next cell were followed by a decrement, ending up on the same cell as before
                // BUGFIX 2026-08-02: this used to throw an exception from when no decrement operator existed. With a decrement operator in place, this is actually now a nominal code branch
                // ===== Easy job: just update m_currentCell to m_hintNode
                m_currentCell = XLCell(*m_hintNode, m_sharedStrings.get());
            }
        }
        else {
            XMLNode rowNode{};  // scope declaration

            if (m_currentRow > m_hintRow) {
                // ===== Start from m_hintNode parent row and search forwards...
                rowNode = m_hintNode->parent().next_sibling_of_type(pugi::node_element);
                uint32_t rowNo = 0;
                while (not rowNode.empty()) {
                    rowNo = rowNode.attribute("r").as_ullong();
                    if (rowNo >= m_currentRow) break; // if desired row was reached / passed, break before incrementing rowNode
                    rowNode = rowNode.next_sibling_of_type(pugi::node_element);
                }
                if (rowNo != m_currentRow) rowNode = XMLNode{}; // if a higher row number was found, set empty node (means: "missing")
                // ===== Create missing row node if createIfMissing == true
                if (createIfMissing && rowNode.empty()) {
                    rowNode = m_dataNode->insert_child_after("row", m_hintNode->parent());
                    rowNode.append_attribute("r").set_value(m_currentRow);
                }
            }
            else {  // 2026-07-21: support backwards searching for decrement operator
                // throw XLInternalError("XLCellIterator::updateCurrentCell: an internal error occurred (m_currentRow < m_hintRow)");

                // ===== Start from m_hintNode parent row and search *backwards*
                rowNode = m_hintNode->parent().previous_sibling_of_type(pugi::node_element);
                uint32_t rowNo = 0;
                while (not rowNode.empty()) {
                    rowNo = rowNode.attribute("r").as_ullong();
                    if (rowNo <= m_currentRow) break; // if desired row was reached / passed, break before decrementing rowNode
                    rowNode = rowNode.previous_sibling_of_type(pugi::node_element);
                }
                if (rowNo != m_currentRow) rowNode = XMLNode{}; // if a higher row number was found, set empty node (means: "missing")
                // ===== Create missing row node if createIfMissing == true
                if (createIfMissing && rowNode.empty()) {
                    rowNode = m_dataNode->insert_child_before("row", m_hintNode->parent());
                    rowNode.append_attribute("r").set_value(m_currentRow);
                }
            }

            // ===== Common code for m_currentRow != m_hintRow
            if (rowNode.empty())    // if row could not be found / created
                m_currentCell = XLCell{}; // make sure m_currentCell is set to an empty cell
            else {                  // else: row found
                if (createIfMissing) {
                    // ===== Pass the already known m_currentRow to getCellNode so that it does not have to be fetched again
                    m_currentCell = XLCell(getCellNode (rowNode, m_currentColumn, m_currentRow, *m_colStyles), m_sharedStrings.get());
                }
                else // ===== Do a "soft find" if a missing cell shall not be created
                    m_currentCell = XLCell(findCellNode(rowNode, m_currentColumn), m_sharedStrings.get());
            }
        }
    }

    if (m_currentCell.empty())    // if cell is confirmed missing
        m_currentCellStatus = XLNoSuchCell; // mark this status for further calls to updateCurrentCell()
    else {
        // ===== The current cell exists. If m_hintNode needs to be updated, update the hints
        if (m_hintNode->empty() || m_hintRow != m_currentRow || m_hintColumn != m_currentColumn) {
            m_hintNode   = std::make_unique<XMLNode>(*m_currentCell.m_cellNode);  // 2024-08-11: don't store a full XLCell, just the XMLNode, for better performance
            m_hintRow    = m_currentRow;
            m_hintColumn = m_currentColumn;
        }
        // ===== Mark cell status for further calls to updateCurrentCell()
        m_currentCellStatus = XLLoaded;
    }
}

/**
 * @details
 * @note 2024-06-03: implemented a calculated distance based on m_currentCell, m_topLeft and m_bottomRight (if m_endReached)
 *                   accordingly, implemented defined setting of m_endReached at all times
 */
int64_t XLCellIterator::distance_impl(const XLCellIterator& last)
{
    // ===== Determine rows and columns, taking into account beyond-the-end iterators
    uint32_t row = (m_endReached ? m_bottomRight.row() : m_currentRow);
    uint16_t col = (m_endReached ? m_bottomRight.column() + 1 : m_currentColumn);
    uint32_t lastRow = (last.m_endReached ? last.m_bottomRight.row() : last.m_currentRow);
    // ===== lastCol can store +1 for beyond-the-end iterator without overflow because MAX_COLS is less than max uint16_t
    uint16_t lastCol = (last.m_endReached ? last.m_bottomRight.column() + 1 : last.m_currentColumn);

    uint16_t rowWidth = m_bottomRight.column() - m_topLeft.column() + 1;    // amount of cells in a row of the iterator range

    return              ((int64_t)(lastRow) - row) * rowWidth    //   row distance * rowWidth
                       + (int64_t)(lastCol) - col;               // + column distance (may be negative)
}

/**
 * @details
 */
XLCellIterator& XLCellIterator::operator++()
{
    if (m_endReached)
        throw XLInputError("XLCellIterator: tried to increment beyond range end");

    if (m_currentColumn < m_bottomRight.column())
        ++m_currentColumn;
    else if (m_currentRow < m_bottomRight.row()) {
        ++m_currentRow;
        m_currentColumn = m_topLeft.column();
    }
    else
        m_endReached = true;

    m_currentCellStatus = XLNotLoaded; // trigger a new attempt to locate / create the cell via updateCurrentCell

    return *this;
}

/**
 * @details
 */
XLCellIterator XLCellIterator::operator++(int)    // NOLINT
{
    auto oldIter(*this);
    ++(*this);
    return oldIter;
}

/**
 * @details
 */
XLCellIterator& XLCellIterator::operator--()
{
    uint32_t topRow = m_topLeft.row();
    uint16_t leftCol = m_topLeft.column();
    assert_always (m_endReached || (m_currentRow >= topRow && m_currentColumn >= leftCol));     // 2026-07-24: safety check for a few months to gain confidence that no bug can cause this state
    if (m_currentRow == topRow && m_currentColumn == leftCol)
        throw XLInputError("XLCellIterator: tried to decrement beyond range begin");

    if (m_endReached) {
        m_endReached = false;
        m_currentRow    = m_bottomRight.row();      // 2026-07-19: enable reverse iteration by correctly initializing m_currentRow
        m_currentColumn = m_bottomRight.column();   //              and m_currentColumn
    }
    else if (m_currentColumn > leftCol)
        --m_currentColumn;
    else {     // m_currentColumn == leftCol && m_currentRow > topRow
        --m_currentRow;
        m_currentColumn = m_bottomRight.column();
    }

    m_currentCellStatus = XLNotLoaded; // trigger a new attempt to locate / create the cell via updateCurrentCell

    return *this;
}

/**
 * @details
 */
XLCellIterator XLCellIterator::operator--(int)    // NOLINT
{
    auto oldIter(*this);
    --(*this);
    return oldIter;
}

/**
 * @details
 */
XLCell& XLCellIterator::operator*()
{
// std::cout << "XLCellIterator dereference operator* invoked" << std::endl;
    updateCurrentCell(XLCreateIfMissing);
    return m_currentCell;
}

/**
 * @details
 */
XLCellIterator::pointer XLCellIterator::operator->()
{
// std::cout << "XLCellIterator dereference operator-> invoked" << std::endl;
    updateCurrentCell(XLCreateIfMissing);
    return &m_currentCell;
}

/**
 * @details
 */
bool XLCellIterator::operator==(const XLCellIterator& rhs) const
{
    // BUGFIX 2024-08-10: there was no test for (!m_currentCell && rhs.m_currentCell),
    //     leading to a potential dereference of a nullptr in m_currentCell::m_cellNode

    if (m_endReached && rhs.m_endReached) return true;    // If both iterators are end iterators

    if ((m_currentColumn != rhs.m_currentColumn)          // if iterators point to a different column or row
      ||(m_currentRow    != rhs.m_currentRow))
        return false;                                         // that means no match

    // CAUTION: for performance reasons, disabled all checks whether this and rhs are iterators on the same worksheet & range
    return true;

    // if (*m_dataNode != *rhs.m_dataNode) return false;     // TBD: iterators over different worksheets may never match
    // TBD if iterators shall be considered not equal if they were created on different XLCellRanges
    // this would require checking the topLeft and bottomRight references, potentially costing CPU time

    // return m_currentCell == rhs.m_currentCell;   // match only if cell nodes are equal
    // CAUTION: in the current code, that means iterators that point to the same column & row in different worksheets,
    // and cells that do not exist in both sheets, will be considered equal
}

/**
 * @details
 */
bool XLCellIterator::operator!=(const XLCellIterator& rhs) const { return !(*this == rhs); }

/**
 * @details
 */
bool XLCellIterator::cellExists()
{
    // ===== Update m_currentCell once so that cellExists will always test the correct cell (an empty cell if current cell doesn't exist)
    updateCurrentCell(XLDoNotCreateIfMissing);
    return not m_currentCell.empty();
}

/**
 * @details
 */
bool XLCellIterator::eraseCell()
{
    if (cellExists()) {     // NOTE: cellExists will updateCurrentCell - this if-scope relies on that
        m_hintRowNode = std::make_unique<XMLNode>(m_hintNode->parent());    // update m_hintRowNode
        XMLNode prev = m_hintNode->previous_sibling_of_type(pugi::node_element);
        XMLNode prevRow{};                                                  // scope declaration
        if (prev.empty()) {                                                 // if current cell has no left neighbor
            prevRow = m_hintRowNode->previous_sibling_of_type(pugi::node_element);      // attempt to fetch previous row
            if (not prevRow.empty()) {                                                  // if any previous row is found
                prev = prevRow.last_child_of_type(pugi::node_element);                      // attempt to fetch last (cell) node within previous row
                m_hintRow = prevRow.attribute("r").as_ullong();                             // update hint row number
            }
            else                                                                        // else: no previous row
                m_hintRow = 0;                                                              // clear hint row number
        }
        m_hintNode = std::make_unique<XMLNode>(prev);                       // update m_hintNode: if prev is empty, this will become an empty hint node, triggering a full find
        if (not m_hintNode->empty())                                        // if m_hintNode is set
            m_hintColumn = XLCellReference(prev.attribute("r").value()).column();   // then update m_hintColumn
        else                                                                // otherwise
            m_hintColumn = 0;                                                       // clear m_hintColumn (redundant for readability - m_hintColumn is not evaluated if m_hintNode is empty)

        m_hintRowNode->remove_child(*m_currentCell.m_cellNode);             // remove current cell node
        if (not prevRow.empty())                                            // if context for m_hintNode is switched to previous row
            m_hintRowNode = std::make_unique<XMLNode>(prevRow);                 // update m_hintRowNode to previous row
        m_currentCellStatus = XLNoSuchCell;                                 // mark cell status as deleted
        return true;        // confirm deletion
    }
    else
        return false;       // could not delete anything
}

/**
 * @details
 */
uint64_t XLCellIterator::distance(const XLCellIterator& last)
{
    int64_t dist = distance_impl(last);
    if (dist < 0)
        throw XLInputError("XLCellIterator::distance is negative");

    return static_cast<uint64_t>(dist);        // after excluding negative result: cast back to positive value
}

/**
 * @details
 */
uint64_t XLCellIterator::rdistance(const XLCellIterator& first)
{
    int64_t dist = -1 * distance_impl(first);
    if (dist < 0)
        throw XLInputError("XLCellIterator::rdistance is negative");

    return static_cast<uint64_t>(dist);        // after excluding negative result: cast back to positive value
}

/**
 * @details
 */
bool addressIsValid(uint32_t row, uint16_t column)
{
    return !(row < 1 || row > OpenXLSX::MAX_ROWS || column < 1 || column > OpenXLSX::MAX_COLS);
}

/**
 * @details
 */
std::string XLCellIterator::address() const
{
    uint32_t row = (m_endReached ? m_bottomRight.row() : m_currentRow);
    uint16_t col = (m_endReached ? m_bottomRight.column() + 1 : m_currentColumn);
    return (m_endReached ? "END(" : "") + XLCellReference(row, col).address() + (m_endReached ? ")" : "");
}


/**
 * @details initialize the internal XLCellIterator and m_beginReached status for beyond-the-end iterator
 */
XLCellReverseIterator::XLCellReverseIterator(const XLCellRange& cellRange, XLIteratorLocation loc, std::vector<XLStyleIndex> const * colStyles)
    : m_XLCellIterator(
        cellRange,
        ( loc == XLIteratorLocation::Begin ? XLIteratorLocation::End : XLIteratorLocation::Begin ),     // iterator location is inverted for internal forward iterator
        colStyles),
      m_beginReached(false)
{
    if (loc == XLIteratorLocation::End)
        m_beginReached = true;
    else    // for all but the end location:
        --m_XLCellIterator;     // immediately decrement the underlying XLCellIterator to navitate to the first cell in reverse
}

/**
 * @details
 */
XLCellReverseIterator::~XLCellReverseIterator() = default;

/**
 * @details
 */
XLCellReverseIterator::XLCellReverseIterator(const XLCellReverseIterator& other)
    : m_XLCellIterator(other.m_XLCellIterator),
      m_beginReached(other.m_beginReached)
{}

/**
 * @details
 */
XLCellReverseIterator::XLCellReverseIterator(XLCellReverseIterator&& other) noexcept = default;

/**
 * @details
 */
XLCellReverseIterator& XLCellReverseIterator::operator=(const XLCellReverseIterator& other)
{
    if (&other != this) {
        m_XLCellIterator = other.m_XLCellIterator;
        m_beginReached   = other.m_beginReached;
    }

    return *this;
}

/**
 * @details
 */
XLCellReverseIterator& XLCellReverseIterator::operator=(XLCellReverseIterator&& other) noexcept
{
    if (&other != this) {
        m_XLCellIterator = std::move(other.m_XLCellIterator);
        m_beginReached   = other.m_beginReached;
    }
    return *this;
}

/**
 * @details
 */
XLCell& XLCellReverseIterator::operator*() { return *m_XLCellIterator; }

/**
 * @details
 */
XLCellReverseIterator::pointer XLCellReverseIterator::operator->() { return &*m_XLCellIterator; }

/**
 * @details
 */
XLCellReverseIterator& XLCellReverseIterator::operator++()
{
    if (m_beginReached)
        throw XLInputError("XLCellReverseIterator: tried to increment beyond range end");
    try { --m_XLCellIterator; }                                 // defer increment to internal XLCellIterator decrement
    catch( XLInputError const & e ) { m_beginReached = true; }  // XLCellIterator decrement throws on decrement "past begin"
    return *this;
}

/**
 * @details
 */
XLCellReverseIterator XLCellReverseIterator::operator++(int)    // NOLINT
{
    auto oldIter(*this);
    ++(*this);
    return oldIter;
}

/**
 * @details
 */
bool XLCellReverseIterator::operator==(const XLCellReverseIterator& rhs) const
{
    if (m_beginReached && rhs.m_beginReached) return true;      // if both iterators are "end" iterators, consider them equal
    if (m_beginReached != rhs.m_beginReached) return false;     // if one iterator is "end iterator", consider them not equal
    //                                                          // otherwise:
    return m_XLCellIterator == rhs.m_XLCellIterator;            // fall back to underlying XLCellIterator equality
}

/**
 * @details
 */
bool XLCellReverseIterator::operator!=(const XLCellReverseIterator& rhs) const { return !(*this == rhs); }

/**
 * @details
 */
uint64_t XLCellReverseIterator::distance(const XLCellReverseIterator& last)
{
    int64_t dist = m_XLCellIterator.rdistance(last.m_XLCellIterator);
    if (dist < 0)
        throw XLInputError("XLCellReverseIterator::distance is negative");

    if (last.m_beginReached && !m_beginReached)     // if distance is measured from a cell in the range to "past the begin" iterator
        ++dist;                                         // then increase the distance calculated by XLCellIterator::rdistance by one

    return static_cast<uint64_t>(dist);             // after excluding negative result: cast back to positive value
}

/**
 * @details
 */
std::string XLCellReverseIterator::address() const
{
    using namespace std::literals::string_literals;
    if (m_beginReached) {   // ===== Special rule for beyond-the-end iterator
        uint16_t col = column();    // XLCellReverseIterator::column already returns the topLeft column minus 1 if m_beginReached
        if (col == 0)                                                   // if iterator topLeft is in first column
            return "rEND(_"s + std::to_string(row()) + ")"s;                 // then mark address column as "before the first column"
        else                                                            // otherwise
            return "rEND("s + XLCellReference(row(), col).address() + ")"s;  // simply return an address to the column left of the topLeft column
    }
    else
        return m_XLCellIterator.address();
}

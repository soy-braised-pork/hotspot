/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sourcecodemodel.h"
#include "hotspot-config.h"

#include <QFile>
#include <QPalette>
#include <QTextDocument>

#ifdef KF5SyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#endif

SourceCodeModel::SourceCodeModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_document(new QTextDocument(this))
#ifdef KF5SyntaxHighlighting_FOUND
    , m_repository(std::make_unique<KSyntaxHighlighting::Repository>())
    , m_highlighter(new KSyntaxHighlighting::SyntaxHighlighter(m_document))
#endif
{
    qRegisterMetaType<QTextLine>();

    updateColorTheme();
}

SourceCodeModel::~SourceCodeModel() = default;

void SourceCodeModel::clear()
{
    beginResetModel();
    m_sourceCode.clear();
    endResetModel();
}

void SourceCodeModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    if (disassemblyOutput.sourceFileName.isEmpty())
        return;

    QFile file(disassemblyOutput.sourceFileName);

    if (!file.open(QIODevice::ReadOnly))
        return;

    beginResetModel();

    m_validLineNumbers.clear();
    m_costs = {};
    m_costs.initializeCostsFrom(m_callerCalleeResults.selfCosts);

    const auto sourceCode = QString::fromUtf8(file.readAll());

    const auto lines = sourceCode.split(QLatin1Char('\n'));

    m_document->setPlainText(sourceCode);
    m_document->setTextWidth(m_document->idealWidth());

#ifdef KF5SyntaxHighlighting_FOUND
    // if the document is set in the constructor, highlighting doen't work
    const auto def = m_repository->definitionForFileName(disassemblyOutput.sourceFileName);
    m_highlighter->setDefinition(def);
#endif

    int maxIndex = 0;
    int minIndex = INT_MAX;

    auto entry = m_callerCalleeResults.entry(disassemblyOutput.symbol);

    for (const auto& line : disassemblyOutput.disassemblyLines) {
        if (line.sourceCodeLine == 0)
            continue;

        if (line.sourceCodeLine > maxIndex) {
            maxIndex = line.sourceCodeLine;
        }
        if (line.sourceCodeLine < minIndex) {
            minIndex = line.sourceCodeLine;
        }

        auto it = entry.offsetMap.find(line.addr);
        if (it != entry.offsetMap.end()) {
            const auto& locationCost = it.value();
            const auto& costLine = locationCost.selfCost;
            const auto totalCost = m_callerCalleeResults.selfCosts.totalCosts();

            m_costs.add(line.sourceCodeLine, costLine);
        }

        m_validLineNumbers.insert(line.sourceCodeLine);
    }

    m_sourceCode.clear();

    // the start / end of a function is defined by { }
    // the following code tries to find ( to include the function signature in the source code
    if (!lines[minIndex - 1].contains(QLatin1Char('('))) {
        for (int i = minIndex - 1; i > minIndex - 6; i--) {
            if (i < 0)
                break;
            if (lines[minIndex - 1].contains(QLatin1Char('(')))
                break;
            minIndex--;
        }
    }

    for (int i = minIndex - 1; i < maxIndex + 1; i++) {
        const auto block = m_document->findBlockByLineNumber(i);
        m_sourceCode.push_back(block.layout()->lineAt(0));
    }

    m_lineOffset = minIndex;

    endResetModel();
}

QVariant SourceCodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= COLUMN_COUNT + m_numTypes)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == SourceCodeColumn)
        return tr("Source Code");

    if (section == SourceCodeLineNumber)
        return tr("Source Code Line Number");

    if (section == Highlight) {
        return tr("Highlight");
    }

    if (section - COLUMN_COUNT <= m_numTypes) {
        return m_callerCalleeResults.selfCosts.typeName(section - COLUMN_COUNT);
    }

    return {};
}

QVariant SourceCodeModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_sourceCode.count() || index.row() < 0)
        return {};

    const auto& line = m_sourceCode.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole || role == CostRole || role == TotalCostRole) {
        if (index.column() == SourceCodeColumn)
            return QVariant::fromValue(line);

        if (index.column() == SourceCodeLineNumber) {
            int line = index.row() + m_lineOffset;
            if (m_validLineNumbers.contains(line))
                return line;
            return 0;
        }

        if (index.column() == Highlight) {
            return index.row() + m_lineOffset == m_highlightLine;
        }

        if (index.column() - COLUMN_COUNT < m_numTypes) {
            const auto cost = m_costs.cost(index.column() - COLUMN_COUNT, index.row() + m_lineOffset);
            const auto totalCost = m_costs.totalCost(index.column() - COLUMN_COUNT);
            if (role == CostRole) {
                return cost;
            }
            if (role == TotalCostRole) {
                return totalCost;
            }

            return Util::formatCostRelative(cost, totalCost, true);
        }
    }
    return {};
}

int SourceCodeModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT + m_numTypes;
}

int SourceCodeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_sourceCode.count();
}

void SourceCodeModel::updateHighlighting(int line)
{
    m_highlightLine = line;
    emit dataChanged(createIndex(0, Columns::SourceCodeColumn), createIndex(rowCount(), Columns::SourceCodeColumn));
}

int SourceCodeModel::lineForIndex(const QModelIndex& index)
{
    return index.row() + m_lineOffset;
}

void SourceCodeModel::setCallerCalleeResults(const Data::CallerCalleeResults& results)
{
    beginResetModel();
    m_callerCalleeResults = results;
    m_numTypes = results.selfCosts.numTypes();
    endResetModel();
}

void SourceCodeModel::updateColorTheme()
{
#ifdef KF5SyntaxHighlighting_FOUND
    KSyntaxHighlighting::Repository::DefaultTheme theme;
    if (QPalette().base().color().lightness() < 128) {
        theme = KSyntaxHighlighting::Repository::DarkTheme;
    } else {
        theme = KSyntaxHighlighting::Repository::LightTheme;
    }
    m_highlighter->setTheme(m_repository->defaultTheme(theme));
    m_highlighter->rehighlight();
#endif
}
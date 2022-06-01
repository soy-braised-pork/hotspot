/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QAbstractTableModel>
#include <QTextLine>

#include "data.h"
#include "disassemblyoutput.h"

class QTextDocument;

namespace KSyntaxHighlighting {
class SyntaxHighlighter;
class Repository;
}

class DisassemblyModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DisassemblyModel(QObject *parent = nullptr);
    ~DisassemblyModel();

    void setDisassembly(const DisassemblyOutput& disassemblyOutput);
    void setResults(const Data::CallerCalleeResults& results);

    void clear();
    QModelIndex findIndexWithOffset(int offset);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int lineForIndex(const QModelIndex& index);

    enum Columns
    {
        DisassemblyColumn,
        LinkedFunctionName,
        LinkedFunctionOffset,
        SourceCodeLine,
        Highlight,
        COLUMN_COUNT
    };

    enum CustomRoles
    {
        CostRole = Qt::UserRole,
        TotalCostRole = Qt::UserRole + 1,
    };

public slots:
    void updateHighlighting(int line);
    void updateColorTheme();

private:
    QTextDocument* m_document;
    std::unique_ptr<KSyntaxHighlighting::Repository> m_repository;
    KSyntaxHighlighting::SyntaxHighlighter* m_highlighter = nullptr;
    DisassemblyOutput m_data;
    QList<QTextLine> m_lines;
    Data::CallerCalleeResults m_results;
    int m_numTypes = 0;
    int m_highlightLine = 0;
};

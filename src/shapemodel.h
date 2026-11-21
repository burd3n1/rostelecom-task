#pragma once
#include "database.h"
#include <QSqlTableModel>
#include <QStyledItemDelegate>

class ShapeTableModel final : public QSqlTableModel {
    Q_OBJECT
public:
    enum Column { Id, Type, Links, Count, Width, Height, Vertices };
    explicit ShapeTableModel(QSqlDatabase db, QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool applyFilter(qint64 id, int type, int minCount, int maxCount);
};

class ShapeCountDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    static int iconCount(int count);
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
};

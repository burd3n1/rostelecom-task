#pragma once
#include "database.h"
#include <QMainWindow>
#include <QHash>
#include <functional>

class QComboBox;
class QSpinBox;
class QTableView;
class QGraphicsScene;
class QGraphicsView;
class QPushButton;
class QLabel;
class ShapeItem;
class ShapeTableModel;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Database &database, QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    void buildUi();
    void refresh(const QVector<qint64> &selection = {});
    void applyFilters();
    void addShape();
    void deleteSelection();
    void connectSelection();
    void sceneSelectionChanged();
    void tableSelectionChanged();
    void updateActions();
    void fitScene();
    QVector<qint64> selectedIds() const;
    void guard(const std::function<void()> &action);

    Database &database_;
    ShapeTableModel *model_ = nullptr;
    QGraphicsScene *scene_ = nullptr;
    QGraphicsView *view_ = nullptr;
    QTableView *table_ = nullptr;
    QHash<qint64, ShapeItem *> items_;
    QComboBox *shapeType_ = nullptr;
    QSpinBox *width_ = nullptr;
    QSpinBox *height_ = nullptr;
    QSpinBox *vertices_ = nullptr;
    QSpinBox *filterId_ = nullptr;
    QComboBox *filterType_ = nullptr;
    QSpinBox *filterMin_ = nullptr;
    QSpinBox *filterMax_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *demoButton_ = nullptr;
    QLabel *summary_ = nullptr;
    QLabel *selectionLabel_ = nullptr;
    QLabel *tableSummary_ = nullptr;
    bool syncing_ = false;
};

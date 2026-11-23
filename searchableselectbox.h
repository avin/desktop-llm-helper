#ifndef SEARCHABLESELECTBOX_H
#define SEARCHABLESELECTBOX_H

#include <QList>
#include <QPushButton>

class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QSortFilterProxyModel;
class QStackedWidget;

class SearchableSelectBox : public QPushButton {
    Q_OBJECT

public:
    QString currentValue() const;

signals:
    void currentValueChanged(const QString &value);

protected:
    struct Item {
        QString value;
        QString label;
        QString searchAlias;
    };

    explicit SearchableSelectBox(const QString &searchPlaceholder, QWidget *parent,
                                 bool itemDetailsEnabled = false);
    void setCurrentValue(const QString &value);
    void setItems(QList<Item> items);
    void setEmptyChoiceLabel(const QString &label);
    QString emptyChoiceLabel() const;
    void showStatus(const QString &message, bool searchEnabled);
    bool isPopupVisible() const;

    Q_SIGNAL void itemDetailsRequested(const QString &value, const QRect &globalAnchor);
    Q_SIGNAL void itemDetailsDismissed();
    virtual void preparePopup();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    class ListModel;

    void ensurePopup();
    void showPopup();
    void filterItems();
    void chooseItem(const QModelIndex &index);
    void selectCurrentItem();
    void moveCurrentIndex(int delta);
    bool requestItemDetails(const QPoint &position);
    void updateButtonLabel();

    const QString searchPlaceholder;
    const bool itemDetailsEnabled;
    QString selectedValue;
    ListModel *choices;
    QFrame *popup = nullptr;
    QLineEdit *searchEdit = nullptr;
    QStackedWidget *stack = nullptr;
    QLabel *statusLabel = nullptr;
    QListView *listView = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
};

#endif // SEARCHABLESELECTBOX_H

#include "searchableselectbox.h"

#include <QAbstractListModel>
#include <QFrame>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QVBoxLayout>

#include <utility>

namespace {
constexpr int kPopupHeight = 300;
constexpr int kRowHeight = 30;

enum ItemRole {
    ValueRole = Qt::UserRole + 1,
    SearchAliasRole,
    EmptyChoiceRole
};

QRect detailsRect(const QRect &row) {
    return QRect(row.right() - 25, row.y() + (row.height() - 18) / 2, 18, 18);
}

class SelectFilter : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const QRegularExpression pattern = filterRegularExpression();
        return index.data(EmptyChoiceRole).toBool()
            || index.data(Qt::DisplayRole).toString().contains(pattern)
            || index.data(SearchAliasRole).toString().contains(pattern);
    }
};

class SelectItemDelegate : public QStyledItemDelegate {
public:
    SelectItemDelegate(bool itemDetailsEnabled, QObject *parent)
        : QStyledItemDelegate(parent)
        , itemDetailsEnabled(itemDetailsEnabled) {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        painter->save();

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const bool emptyChoice = index.data(EmptyChoiceRole).toBool();
        const bool showDetails = itemDetailsEnabled && !emptyChoice;

        if (selected)
            painter->fillRect(option.rect, option.palette.highlight());
        else if (hovered)
            painter->fillRect(option.rect, option.palette.alternateBase());

        QColor textColor = selected
            ? option.palette.highlightedText().color()
            : option.palette.text().color();
        if (emptyChoice && !selected)
            textColor = QColor(170, 170, 170);

        const QRect textRect = option.rect.adjusted(8, 0, showDetails ? -34 : -8, 0);
        const QString text = option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(),
                                                           Qt::ElideRight, textRect.width());
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

        if (showDetails) {
            const QRect helpRect = detailsRect(option.rect);
            const QColor borderColor = selected
                ? option.palette.highlightedText().color()
                : option.palette.mid().color();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(borderColor);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(helpRect);

            QFont helpFont = option.font;
            helpFont.setBold(true);
            painter->setFont(helpFont);
            painter->drawText(helpRect, Qt::AlignCenter, QStringLiteral("?"));
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return QSize(0, kRowHeight);
    }

private:
    const bool itemDetailsEnabled;
};
} // namespace

class SearchableSelectBox::ListModel : public QAbstractListModel {
public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() ? 0 : items.size() + (emptyLabel.isEmpty() ? 0 : 1);
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= rowCount())
            return {};

        const int offset = emptyLabel.isEmpty() ? 0 : 1;
        if (index.row() < offset) {
            if (role == Qt::DisplayRole)
                return emptyLabel;
            if (role == EmptyChoiceRole)
                return true;
            return {};
        }

        const Item &item = items.at(index.row() - offset);
        switch (role) {
        case Qt::DisplayRole:
            return item.label;
        case ValueRole:
            return item.value;
        case SearchAliasRole:
            return item.searchAlias;
        default:
            return {};
        }
    }

    void setItems(QList<Item> newItems) {
        beginResetModel();
        items = std::move(newItems);
        endResetModel();
    }

    void setEmptyLabel(const QString &label) {
        if (emptyLabel == label)
            return;
        if (emptyLabel.isEmpty()) {
            beginInsertRows({}, 0, 0);
            emptyLabel = label;
            endInsertRows();
        } else {
            emptyLabel = label;
            emit dataChanged(index(0, 0), index(0, 0), {Qt::DisplayRole});
        }
    }

    QString emptyLabel;

private:
    QList<Item> items;
};

SearchableSelectBox::SearchableSelectBox(const QString &searchPlaceholder, QWidget *parent,
                                         bool itemDetailsEnabled)
    : QPushButton(parent)
    , searchPlaceholder(searchPlaceholder)
    , itemDetailsEnabled(itemDetailsEnabled)
    , choices(new ListModel(this)) {
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(24);
    updateButtonLabel();
    connect(this, &QPushButton::clicked, this, &SearchableSelectBox::showPopup);
}

QString SearchableSelectBox::currentValue() const {
    return selectedValue;
}

void SearchableSelectBox::setCurrentValue(const QString &value) {
    const QString normalized = value.trimmed();
    if (selectedValue == normalized)
        return;
    selectedValue = normalized;
    updateButtonLabel();
}

void SearchableSelectBox::setItems(QList<Item> items) {
    choices->setItems(std::move(items));
    if (!popup)
        return;
    searchEdit->setEnabled(true);
    stack->setCurrentWidget(listView);
    if (isPopupVisible()) {
        searchEdit->setFocus(Qt::PopupFocusReason);
        searchEdit->selectAll();
        selectCurrentItem();
    }
}

void SearchableSelectBox::setEmptyChoiceLabel(const QString &label) {
    const QString normalized = label.trimmed();
    choices->setEmptyLabel(normalized.isEmpty() ? tr("Not selected") : normalized);
    updateButtonLabel();
    if (isPopupVisible())
        selectCurrentItem();
}

QString SearchableSelectBox::emptyChoiceLabel() const {
    return choices->emptyLabel;
}

void SearchableSelectBox::showStatus(const QString &message, bool searchEnabled) {
    ensurePopup();
    searchEdit->setEnabled(searchEnabled);
    statusLabel->setText(message);
    stack->setCurrentWidget(statusLabel);
    if (searchEnabled && isPopupVisible())
        searchEdit->setFocus(Qt::PopupFocusReason);
}

bool SearchableSelectBox::isPopupVisible() const {
    return popup && popup->isVisible();
}

void SearchableSelectBox::preparePopup() {
}

bool SearchableSelectBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == popup && event->type() == QEvent::Hide)
        emit itemDetailsDismissed();

    if (watched == searchEdit && event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Down:
            moveCurrentIndex(1);
            return true;
        case Qt::Key_Up:
            moveCurrentIndex(-1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            chooseItem(listView->currentIndex());
            return true;
        default:
            break;
        }
    }

    if (itemDetailsEnabled && listView && watched == listView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            if (!requestItemDetails(static_cast<QMouseEvent *>(event)->pos()))
                emit itemDetailsDismissed();
        } else if (event->type() == QEvent::Leave) {
            emit itemDetailsDismissed();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (requestItemDetails(static_cast<QMouseEvent *>(event)->pos()))
                return true;
        }
    }
    return QPushButton::eventFilter(watched, event);
}

void SearchableSelectBox::paintEvent(QPaintEvent *) {
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    const QRect textRect = rect().adjusted(8, 0, -26, 0);
    const QString elided = fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width());
    style()->drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          palette(), isEnabled(), elided, QPalette::ButtonText);

    QStyleOption arrowOption;
    arrowOption.initFrom(this);
    arrowOption.rect = QRect(width() - 20, (height() - 12) / 2, 12, 12);
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOption, &painter, this);
}

void SearchableSelectBox::ensurePopup() {
    if (popup)
        return;

    popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName(QStringLiteral("searchableSelectPopup"));
    popup->setFrameShape(QFrame::StyledPanel);
    popup->installEventFilter(this);
    popup->setStyleSheet(QStringLiteral(
        "QFrame#searchableSelectPopup { background: palette(window); border: 1px solid palette(mid); }"
        "QLineEdit { margin: 6px; padding: 4px 6px; }"
    ));

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    searchEdit = new QLineEdit(popup);
    searchEdit->setPlaceholderText(searchPlaceholder);
    searchEdit->installEventFilter(this);
    layout->addWidget(searchEdit);

    stack = new QStackedWidget(popup);
    layout->addWidget(stack, 1);

    proxy = new SelectFilter(popup);
    proxy->setSourceModel(choices);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    listView = new QListView(stack);
    listView->setModel(proxy);
    listView->setItemDelegate(new SelectItemDelegate(itemDetailsEnabled, listView));
    listView->setFrameShape(QFrame::NoFrame);
    listView->setUniformItemSizes(true);
    listView->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    listView->setMouseTracking(true);
    listView->setSelectionMode(QAbstractItemView::SingleSelection);
    listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (itemDetailsEnabled) {
        listView->viewport()->setMouseTracking(true);
        listView->viewport()->installEventFilter(this);
    }
    stack->addWidget(listView);

    statusLabel = new QLabel(stack);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setTextFormat(Qt::PlainText);
    stack->addWidget(statusLabel);

    connect(searchEdit, &QLineEdit::textChanged, this, &SearchableSelectBox::filterItems);
    connect(listView, &QListView::clicked, this, &SearchableSelectBox::chooseItem);
    connect(listView, &QListView::activated, this, &SearchableSelectBox::chooseItem);
}

void SearchableSelectBox::showPopup() {
    ensurePopup();
    searchEdit->clear();
    preparePopup();

    popup->setFixedSize(width(), kPopupHeight);
    popup->move(mapToGlobal(QPoint(0, 0)));
    popup->show();
    popup->raise();
    if (searchEdit->isEnabled())
        searchEdit->setFocus(Qt::PopupFocusReason);
    selectCurrentItem();
}

void SearchableSelectBox::filterItems() {
    proxy->setFilterFixedString(searchEdit->text().trimmed());
    selectCurrentItem();
}

void SearchableSelectBox::chooseItem(const QModelIndex &index) {
    if (stack->currentWidget() != listView || !index.isValid())
        return;
    const QString value = index.data(ValueRole).toString();
    popup->hide();
    if (selectedValue == value)
        return;
    selectedValue = value;
    updateButtonLabel();
    emit currentValueChanged(selectedValue);
}

void SearchableSelectBox::selectCurrentItem() {
    if (stack->currentWidget() != listView)
        return;

    QModelIndex selectedIndex = proxy->index(0, 0);
    for (int row = 0; row < proxy->rowCount(); ++row) {
        const QModelIndex index = proxy->index(row, 0);
        if (index.data(ValueRole).toString() == selectedValue) {
            selectedIndex = index;
            break;
        }
    }

    listView->selectionModel()->setCurrentIndex(
        selectedIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    listView->doItemsLayout();
    const int viewportRows = qMax(1, listView->viewport()->height() / kRowHeight);
    listView->verticalScrollBar()->setValue(qMax(0, selectedIndex.row() - viewportRows / 2));
}

void SearchableSelectBox::moveCurrentIndex(int delta) {
    if (stack->currentWidget() != listView || proxy->rowCount() == 0)
        return;

    const int currentRow = qMax(0, listView->currentIndex().row());
    const int nextRow = qBound(0, currentRow + delta, proxy->rowCount() - 1);
    const QModelIndex nextIndex = proxy->index(nextRow, 0);
    listView->selectionModel()->setCurrentIndex(
        nextIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    listView->scrollTo(nextIndex, QAbstractItemView::EnsureVisible);
}

bool SearchableSelectBox::requestItemDetails(const QPoint &position) {
    const QModelIndex index = listView->indexAt(position);
    if (!index.isValid() || index.data(EmptyChoiceRole).toBool())
        return false;
    const QRect area = detailsRect(listView->visualRect(index));
    if (!area.contains(position))
        return false;
    emit itemDetailsRequested(index.data(ValueRole).toString(),
                              QRect(listView->viewport()->mapToGlobal(area.topLeft()), area.size()));
    return true;
}

void SearchableSelectBox::updateButtonLabel() {
    const QString label = choices->emptyLabel.isEmpty() ? tr("Not selected") : choices->emptyLabel;
    setText(selectedValue.isEmpty() ? label : selectedValue);
}

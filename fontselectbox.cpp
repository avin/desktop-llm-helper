#include "fontselectbox.h"

#include <QFontDatabase>

#include <utility>

FontSelectBox::FontSelectBox(Filter filter, const QString &currentFamily, QWidget *parent)
    : SearchableSelectBox(tr("Search fonts"), parent) {
    QList<Item> items;
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        if (filter == Filter::Monospaced && !QFontDatabase::isFixedPitch(family))
            continue;
        items.append(Item{family, family, {}});
        if (family == currentFamily)
            setCurrentValue(family);
    }
    setItems(std::move(items));
}

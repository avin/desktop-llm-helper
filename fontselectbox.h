#ifndef FONTSELECTBOX_H
#define FONTSELECTBOX_H

#include "searchableselectbox.h"

class FontSelectBox : public SearchableSelectBox {
    Q_OBJECT

public:
    enum class Filter { All, Monospaced };

    explicit FontSelectBox(Filter filter, const QString &currentFamily, QWidget *parent = nullptr);
};

#endif // FONTSELECTBOX_H

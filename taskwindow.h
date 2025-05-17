#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>

class TaskWidget;

/**
 * @brief Независимое окно со списком задач.
 *
 *  • Всегда поверх всех окон (Qt::WindowStaysOnTopHint)  
 *  • Получает фокус сразу после показа  
 *  • Закрывается по нажатию выбранной кнопки или по Esc
 */
class TaskWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TaskWindow(const QList<TaskWidget*>& tasks, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* ev) override;
};

#endif // TASKWINDOW_H
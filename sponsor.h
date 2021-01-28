#ifndef SPONSOR_H
#define SPONSOR_H

#include <QDialog>

namespace Ui {
class Sponsor;
}

class Sponsor : public QDialog
{
    Q_OBJECT

public:
    explicit Sponsor(QWidget *parent = nullptr);
    ~Sponsor();

private:
    Ui::Sponsor *ui;
};

#endif // SPONSOR_H

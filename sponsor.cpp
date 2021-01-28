#include "sponsor.h"
#include "ui_sponsor.h"

Sponsor::Sponsor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Sponsor)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    QPixmap pixpaypal(":/png/paypal.png");
    pixpaypal = pixpaypal.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblPaypal->setPixmap(pixpaypal);

    QPixmap pixwavepay(":/png/wavepay.png");
    pixwavepay = pixwavepay.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblWavePay->setPixmap(pixwavepay);

    QPixmap pixkpay(":/png/kpay.png");
    pixkpay = pixkpay.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblKpay->setPixmap(pixkpay);

    setFixedSize(size());
}

Sponsor::~Sponsor()
{
    delete ui;
}

#include "About.h"
#include "ui_About.h"

About::About(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    QPixmap pixmail(":/png/gmail.png");
    pixmail = pixmail.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblEmail->setPixmap(pixmail);

    QPixmap pixfb(":/png/facebook.png");
    pixfb = pixfb.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblFacebook->setPixmap(pixfb);

    QPixmap pixgit(":/png/github.png");
    pixgit = pixgit.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblGithub->setPixmap(pixgit);

    QPixmap pixweb(":/png/web.png");
    pixweb = pixweb.scaled(QSize(20,20), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->lblWeb->setPixmap(pixweb);

    ui->lblCredit->setPixmap(pixgit);

    setFixedSize(size());
}

About::~About()
{
    delete ui;
}

/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangyu<zhangyub@uniontech.com>
 *
 * Maintainer: zhangyu<zhangyub@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "loadinglabel.h"

#include <DSpinner>
#include <DAnchors>

#include <QLabel>

DWIDGET_USE_NAMESPACE
using namespace ddplugin_wallpapersetting;

#define WAITITEM_ICON_CONTANT_SPACE 10  //gap
#define MARGIN_OF_WAITICON 6

LoadingLabel::LoadingLabel(QWidget *parent)
    : QFrame(parent)
{
    //set window and transparency
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    init();
}

LoadingLabel::~LoadingLabel()
{

}

void LoadingLabel::start()
{
    animationSpinner->setAttribute(Qt::WA_TransparentForMouseEvents);
    animationSpinner->setFocusPolicy(Qt::NoFocus);
    animationSpinner->setFixedSize(spinnerSize);

    DAnchorsBase::setAnchor(animationSpinner, Qt::AnchorVerticalCenter, icon, Qt::AnchorVerticalCenter);
    DAnchorsBase::setAnchor(animationSpinner, Qt::AnchorRight, icon, Qt::AnchorRight);
    DAnchorsBase::getAnchorBaseByWidget(animationSpinner)->setRightMargin(MARGIN_OF_WAITICON);

    animationSpinner->show();
    animationSpinner->start();
}

void LoadingLabel::init()
{
    icon = new QLabel(this);
    contant = new QLabel(this);
    animationSpinner = new DSpinner(icon);
}

void LoadingLabel::resize(const QSize &size)
{
    //set window size and the son size
    this->setFixedSize(size);
    moveDistance = size.width() * proportion;

    //calculates the total width of the son widget
    int sumwidth = iconSize.width() + contantSize.width() + WAITITEM_ICON_CONTANT_SPACE;

    //adjust the width of m_movedistance
    if ((size.width() - static_cast<int>(moveDistance)) < sumwidth) {
        int temp = sumwidth - (size.width() - static_cast<int>(moveDistance));
        moveDistance -= temp;
    }

    //move the son widget by m_movedistance
    if (sumwidth <= size.width()) {
        icon->move(static_cast<int>(moveDistance), size.height() / 3);
        icon->setFixedSize(iconSize);
        contant->move(static_cast<int>(moveDistance) + WAITITEM_ICON_CONTANT_SPACE + icon->width(), size.height() / 3 + MARGIN_OF_WAITICON);
        contant->setFixedSize(contantSize);
    } else {
        qDebug() << "the parent widget is too small that can not to display the son widget";
        icon->setFixedSize(QSize(0, 0));
        contant->setFixedSize(QSize(0, 0));
    }
}

void LoadingLabel::setText(const QString &text)
{
    contant->setText(text);
}


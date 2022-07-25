/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuyangming<liuyangming@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
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
#include "tagbutton.h"

#include <QMouseEvent>
#include <QPainter>

using namespace dfmplugin_tag;

TagButton::TagButton(const QColor &color, QWidget *const parent)
    : QFrame(parent),
      tagColor(color)
{
}

void TagButton::setRadiusF(const double &radius) noexcept
{
    this->radius.first = radius;
    this->setFixedSize(static_cast<int>(radius), static_cast<int>(radius));
}

void TagButton::setRadius(const std::size_t &radius) noexcept
{
    this->radius.second = radius;
    this->setFixedSize(static_cast<int>(radius), static_cast<int>(radius));
}

void TagButton::setCheckable(bool checkable) noexcept
{
    this->checkable = checkable;
}

void TagButton::setChecked(bool checked)
{
    if (!checkable)
        return;

    if (checked) {
        if (paintStatus == PaintStatus::kChecked)
            return;

        setPaintStatus(PaintStatus::kChecked);
    } else if (paintStatus == PaintStatus::kChecked) {
        setPaintStatus(PaintStatus::kNormal);
    } else {
        return;
    }

    emit checkedChanged();
}

bool TagButton::isChecked() const
{
    return paintStatus == PaintStatus::kChecked;
}

bool TagButton::isHovered() const
{
    return testAttribute(Qt::WA_UnderMouse);
}

QColor TagButton::color() const
{
    return tagColor;
}

void TagButton::enterEvent(QEvent *event)
{
    if (!isChecked()) {
        setPaintStatus(PaintStatus::kHover);
    }

    event->accept();

    emit enter();
}

void TagButton::leaveEvent(QEvent *event)
{
    if (!isChecked()) {
        setPaintStatus(PaintStatus::kNormal);
    }

    event->accept();

    emit leave();
}

void TagButton::mousePressEvent(QMouseEvent *event)
{
    if (!isChecked()) {
        setPaintStatus(PaintStatus::kPressed);
    }

    QFrame::mousePressEvent(event);
}

void TagButton::mouseReleaseEvent(QMouseEvent *event)
{
    setChecked(!isChecked());

    emit click(tagColor);
    QFrame::mouseReleaseEvent(event);
}

void TagButton::paintEvent(QPaintEvent *paintEvent)
{
    (void)paintEvent;

    QPainter painter { this };
    double theRadius { static_cast<double>(radius.first > radius.second ? radius.first : radius.second) };

    QPen pen { Qt::SolidLine };

    pen.setWidthF(1.0);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setRenderHint(QPainter::Antialiasing, true);

    switch (paintStatus) {
    case PaintStatus::kNormal: {
        QRectF rectF { 4, 4, theRadius - 8, theRadius - 8 };
        QBrush brush { tagColor };

        pen.setColor(tagColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        break;
    }
    case PaintStatus::kHover: {
        QRectF rectF { 0, 0, theRadius, theRadius };

        pen.setColor(outlineColor);
        painter.setPen(pen);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        QBrush brush { tagColor };
        rectF = QRectF { 4, 4, theRadius - 8, theRadius - 8 };
        pen.setColor(tagColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        break;
    }
    case PaintStatus::kPressed: {
        QRectF rectF { 0, 0, theRadius, theRadius };
        QBrush brush { backgroundColor };

        pen.setColor(outlineColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        rectF = QRectF { 4, 4, theRadius - 8, theRadius - 8 };
        pen.setColor(tagColor);
        brush.setColor(tagColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        break;
    }
    case PaintStatus::kChecked: {
        QRectF rectF { 0, 0, theRadius, theRadius };
        QBrush brush { backgroundColor };

        pen.setColor(outlineColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        rectF = QRectF { 4, 4, theRadius - 8, theRadius - 8 };
        pen.setColor(tagColor);
        brush.setColor(tagColor);
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawEllipse(rectF.marginsRemoved(margins));

        break;
    }
    }
}

void TagButton::setPaintStatus(TagButton::PaintStatus status)
{
    if (paintStatus == status)
        return;

    paintStatus = status;

    update();
}

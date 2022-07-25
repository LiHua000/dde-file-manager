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
#ifndef TAGCRUMBEDIT_H
#define TAGCRUMBEDIT_H

#include "dfmplugin_tag_global.h"

#include <DCrumbEdit>

namespace dfmplugin_tag {

class TagCrumbEdit : public DTK_WIDGET_NAMESPACE::DCrumbEdit
{

public:
    explicit TagCrumbEdit(QWidget *parent = nullptr);

    bool isEditing();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    bool isEditByDoubleClick { false };
};

}

#endif   // TAGCRUMBEDIT_H

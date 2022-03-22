/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
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
#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include "dfmplugin_sidebar_global.h"

#include "dfm-base/interfaces/abstractframe.h"
#include "services/filemanager/sidebar/sidebar_defines.h"

#include <QUrl>

class QAbstractItemView;

DPSIDEBAR_BEGIN_NAMESPACE
class SideBarView;
class SideBarModel;
class SideBarItem;
class SideBarWidget : public DFMBASE_NAMESPACE::AbstractFrame
{
    Q_OBJECT

public:
    explicit SideBarWidget(QFrame *parent = nullptr);
    void setCurrentUrl(const QUrl &sidebarUrl) override;
    QUrl currentUrl() const override;
    void changeEvent(QEvent *event) override;

    QAbstractItemView *view();

    int addItem(SideBarItem *item);
    bool insertItem(const int index, SideBarItem *item);
    bool removeItem(SideBarItem *item);
    bool removeItem(const QUrl &url);

    void updateItem(const QUrl &url, const DSB_FM_NAMESPACE::SideBar::ItemInfo &newInfo);
    void updateItem(const QUrl &url, const QString &newName, bool editable);
    void updateItem(const QUrl &url, const QIcon &newIcon);
    int findItem(const QUrl &url) const;
    void editItem(const QUrl &url);

private Q_SLOTS:
    void onItemActived(const QModelIndex &index);
    void customContextMenuCall(const QPoint &pos);
    void onItemRenamed(const QModelIndex &index, const QString &newName);

private:
    void initializeUi();
    void initDefaultModel();
    void initConnect();
    void updateSeparatorVisibleState();

private:
    QUrl sidebarUrl;
    SideBarView *sidebarView { nullptr };
    SideBarModel *sidebarModel { nullptr };
    QStringList currentGroups;
};
DPSIDEBAR_END_NAMESPACE

#endif   // SIDEBARWIDGET_H

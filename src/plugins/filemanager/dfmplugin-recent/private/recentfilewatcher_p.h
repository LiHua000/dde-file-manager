/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: liuyangming<liuyangming@uniontech.com>
 *             gongheng<gongheng@uniontech.com>
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

#ifndef RECENTDIRITERATORPRIVATE_H
#define RECENTDIRITERATORPRIVATE_H

#include "dfmplugin_recent_global.h"
#include "dfm-base/interfaces/private/abstractfilewatcher_p.h"

namespace dfmplugin_recent {

class RecentFileWatcher;
class RecentFileWatcherPrivate : public DFMBASE_NAMESPACE::AbstractFileWatcherPrivate
{
    friend RecentFileWatcher;

public:
    RecentFileWatcherPrivate(const QUrl &fileUrl, RecentFileWatcher *qq);

public:
    bool start() override;
    bool stop() override;

    void initFileWatcher();
    void initConnect();

    AbstractFileWatcherPointer proxy;
    QMap<QUrl, AbstractFileWatcherPointer> urlToWatcherMap;
};

}
#endif   // RECENTDIRITERATORPRIVATE_H

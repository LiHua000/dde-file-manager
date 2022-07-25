/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     yanghao<yanghao@uniontech.com>
 *
 * Maintainer: zhangsheng<zhangsheng@uniontech.com>
 *             liuyangming<liuyangming@uniontech.com>
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
#ifndef FILEOPERATORHELPER_H
#define FILEOPERATORHELPER_H

#include "dfmplugin_workspace_global.h"
#include "dfm_global_defines.h"
#include "dfm-base/interfaces/abstractjobhandler.h"

#include <QObject>
#include <QUrl>

namespace dfmplugin_workspace {
class FileView;
class FileOperatorHelper : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(FileOperatorHelper)

public:
    static FileOperatorHelper *instance();
    void touchFolder(const FileView *view);
    void touchFiles(const FileView *view, const DFMGLOBAL_NAMESPACE::CreateFileType type, QString suffix = "");
    void openFiles(const FileView *view);
    void openFiles(const FileView *view, const QList<QUrl> &urls);
    void openFilesByMode(const FileView *view, const QList<QUrl> &urls, const DirOpenMode mode = DirOpenMode::kOpenInCurrentWindow);
    void openFilesByApp(const FileView *view);
    void openFilesByApp(const FileView *view, const QList<QUrl> &urls, const QList<QString> &apps);
    void renameFile(const FileView *view, const QUrl &oldUrl, const QUrl &newUrl);
    void copyFiles(const FileView *view);
    void cutFiles(const FileView *view);
    void pasteFiles(const FileView *view);
    void undoFiles(const FileView *view);
    void moveToTrash(const FileView *view);
    void moveToTrash(const FileView *view, const QList<QUrl> &urls);
    void deleteFiles(const FileView *view);
    void createSymlink(const FileView *view, QUrl targetParent = QUrl());
    void openInTerminal(const FileView *view);
    void showFilesProperty(const FileView *view);
    void sendBluetoothFiles(const FileView *view);
    void previewFiles(const FileView *view, const QList<QUrl> &selectUrls, const QList<QUrl> &currentDirUrls);
    void dropFiles(const FileView *view, const Qt::DropAction &action, const QUrl &targetUrl, const QList<QUrl> &urls);

    void renameFilesByReplace(const QWidget *sender, const QList<QUrl> &urlList, const QPair<QString, QString> &replacePair);
    void renameFilesByAdd(const QWidget *sender, const QList<QUrl> &urlList, const QPair<QString, DFMBASE_NAMESPACE::AbstractJobHandler::FileNameAddFlag> &addPair);
    void renameFilesByCustom(const QWidget *sender, const QList<QUrl> &urlList, const QPair<QString, QString> &customPair);

private:
    explicit FileOperatorHelper(QObject *parent = nullptr);
    void callBackFunction(const DFMBASE_NAMESPACE::Global::CallbackArgus args);

    DFMGLOBAL_NAMESPACE::OperatorCallback callBack;
};

#define FileOperatorHelperIns using namespace dfmplugin_workspace;::FileOperatorHelper::instance()
}

#endif   // FILEOPERATORHELPER_H

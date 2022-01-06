/*
 * Copyright (C) 2021 ~ 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     lanxuesong<lanxuesong@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             liyigang<liyigang@uniontech.com>
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
#include "fileoperatebaseworker.h"

#include "dfm-base/interfaces/abstractdiriterator.h"
#include "dfm-base/base/schemefactory.h"
#include "fileoperations/fileoperationutils/fileoperationsutils.h"

#include <dfm-io/core/diofactory.h>
#include <dfm-io/dfmio_register.h>

#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>
#include <QStorageInfo>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static const quint32 kMoveMaxBufferLength { 1024 * 1024 * 1 };

DSC_USE_NAMESPACE
USING_IO_NAMESPACE

FileOperateBaseWorker::FileOperateBaseWorker(QObject *parent)
    : AbstractWorker(parent)
{
}

FileOperateBaseWorker::~FileOperateBaseWorker()
{
}
/*!
 * \brief FileOperateBaseWorker::doHandleErrorAndWait Handle the error and block waiting for the error handling operation to return
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param errorUrl URL of the error file
 * \param error Wrong type
 * \param errorMsg Wrong message
 * \return AbstractJobHandler::SupportAction Current processing operation
 */
AbstractJobHandler::SupportAction FileOperateBaseWorker::doHandleErrorAndWait(const QUrl &fromUrl, const QUrl &toUrl,
                                                                              const QUrl &errorUrl,
                                                                              const AbstractJobHandler::JobErrorType &error,
                                                                              const QString &errorMsg)
{
    setStat(AbstractJobHandler::JobState::kPauseState);
    emitErrorNotify(fromUrl, toUrl, error, errorUrl.toString() + errorMsg);

    if (handlingErrorQMutex.isNull())
        handlingErrorQMutex.reset(new QMutex);
    if (handlingErrorCondition.isNull())
        handlingErrorCondition.reset(new QWaitCondition);
    handlingErrorQMutex->lock();
    handlingErrorCondition->wait(handlingErrorQMutex.data());
    handlingErrorQMutex->unlock();

    return currentAction;
}

/*!
 * \brief FileOperateBaseWorker::createFileDevice Device to create the file
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param needOpenInfo file information
 * \param file fromeFile Output parameter: file device
 * \param result result result Output parameter: whether skip
 * \return Is the device of the file created successfully
 */
bool FileOperateBaseWorker::createFileDevice(const QUrl &fromUrl,
                                             const QUrl &toUrl,
                                             const AbstractFileInfoPointer &needOpenInfo,
                                             QSharedPointer<DFile> &file, bool *result)
{
    file.reset(nullptr);
    QUrl url = needOpenInfo->url();
    AbstractJobHandler::SupportAction actionForCreatDevice = AbstractJobHandler::SupportAction::kNoAction;
    QSharedPointer<DIOFactory> factory { nullptr };
    do {
        factory = produceQSharedIOFactory(url.scheme(), static_cast<QUrl>(url));
        if (!factory) {
            actionForCreatDevice = doHandleErrorAndWait(fromUrl, toUrl, needOpenInfo->url(), AbstractJobHandler::JobErrorType::kDfmIoError, QObject::tr("create dfm io factory failed!"));
        }
    } while (!isStopped() && actionForCreatDevice == AbstractJobHandler::SupportAction::kRetryAction);

    if (actionForCreatDevice != AbstractJobHandler::SupportAction::kNoAction) {
        *result = actionForCreatDevice == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    do {
        file = factory->createFile();
        if (!file) {
            actionForCreatDevice = doHandleErrorAndWait(fromUrl, toUrl, needOpenInfo->url(), AbstractJobHandler::JobErrorType::kDfmIoError, QObject::tr("create dfm io dfile failed!"));
        }
    } while (!isStopped() && actionForCreatDevice == AbstractJobHandler::SupportAction::kRetryAction);

    if (actionForCreatDevice != AbstractJobHandler::SupportAction::kNoAction) {
        *result = actionForCreatDevice == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::createFileDevices Device for creating source and directory files
 * \param fromInfo File information of source file
 * \param toInfo File information of target file
 * \param fromeFile Output parameter: device of source file
 * \param toFile Output parameter: device of target file
 * \param result result Output parameter: whether skip
 * \return Whether the device of source file and target file is created successfully
 */
bool FileOperateBaseWorker::createFileDevices(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo, QSharedPointer<DFile> &fromeFile, QSharedPointer<DFile> &toFile, bool *result)
{
    if (!createFileDevice(fromInfo->url(), toInfo->url(), fromInfo, fromeFile, result))
        return false;
    if (!createFileDevice(fromInfo->url(), toInfo->url(), toInfo, toFile, result))
        return false;
    return true;
}

/*!
 * \brief FileOperateBaseWorker::openFiles Open source and destination files
 * \param fromInfo File information of source file
 * \param toInfo File information of target file
 * \param fromeFile device of source file
 * \param toFile device of target file
 * \param result result Output parameter: whether skip
 * \return Open source and target files successfully
 */
bool FileOperateBaseWorker::openFiles(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo, const QSharedPointer<DFile> &fromeFile, const QSharedPointer<DFile> &toFile, bool *result)
{
    if (fromInfo->size() > 0 && !openFile(fromInfo->url(), toInfo->url(), fromInfo, fromeFile, DFile::OpenFlag::ReadOnly, result)) {
        return false;
    }

    if (!openFile(fromInfo->url(), toInfo->url(), toInfo, toFile, DFile::OpenFlag::Truncate, result)) {
        return false;
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::openFile
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param fileInfo file information
 * \param file file deivce
 * \param flags Flag for opening file
 * \param result result Output parameter: whether skip
 * \return wether open the file successfully
 */
bool FileOperateBaseWorker::openFile(const QUrl &fromUrl, const QUrl &toUrl, const AbstractFileInfoPointer &fileInfo, const QSharedPointer<DFile> &file, const DFile::OpenFlag &flags, bool *result)
{
    AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;
    do {
        if (!file->open(flags)) {
            action = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(), AbstractJobHandler::JobErrorType::kDfmIoError, QObject::tr("create dfm io dfile failed!"));
        }
    } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);
    if (action != AbstractJobHandler::SupportAction::kNoAction) {
        *result = action == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }
    return true;
}

/*!
 * \brief FileOperateBaseWorker::setTargetPermissions Set permissions on the target file
 * \param fromInfo File information of source file
 * \param toInfo File information of target file
 */
void FileOperateBaseWorker::setTargetPermissions(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo)
{
    // 修改文件修改时间
    handler->setFileTime(toInfo->url(), fromInfo->lastRead(), fromInfo->lastModified());
    QFileDevice::Permissions permissions = fromInfo->permissions();
    QString path = fromInfo->url().path();
    //权限为0000时，源文件已经被删除，无需修改新建的文件的权限为0000
    if (permissions != 0000)
        handler->setPermissions(toInfo->url(), permissions);
}

/*!
 * \brief FileOperateBaseWorker::doReadFile  Read file contents
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param fileInfo file information
 * \param fromDevice file device
 * \param data Data buffer
 * \param blockSize Data buffer size
 * \param readSize Read size
 * \param result result Output parameter: whether skip
 * \return Read successfully
 */
bool FileOperateBaseWorker::doReadFile(const QUrl &fromUrl, const QUrl &toUrl, const AbstractFileInfoPointer &fileInfo, const QSharedPointer<DFile> &fromDevice, char *data, const qint64 &blockSize, qint64 &readSize, bool *result)
{
    readSize = 0;
    qint64 currentPos = fromDevice->pos();
    AbstractJobHandler::SupportAction actionForRead = AbstractJobHandler::SupportAction::kNoAction;

    if (Q_UNLIKELY(!stateCheck())) {
        return false;
    }
    do {
        readSize = fromDevice->read(data, blockSize);
        if (Q_UNLIKELY(!stateCheck())) {
            return false;
        }

        if (Q_UNLIKELY(readSize <= 0)) {
            if (readSize == 0 && fileInfo->size() == fromDevice->pos()) {
                return true;
            }
            fileInfo->refresh();
            AbstractJobHandler::JobErrorType errortype = fileInfo->exists() ? AbstractJobHandler::JobErrorType::kReadError : AbstractJobHandler::JobErrorType::kNonexistenceError;
            QString errorstr = fileInfo->exists() ? QString(QObject::tr("DFileCopyMoveJob", "Failed to read the file, cause: %1")).arg("to something!") : QString();

            actionForRead = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(), errortype, errorstr);

            if (actionForRead == AbstractJobHandler::SupportAction::kRetryAction) {
                if (!fromDevice->seek(currentPos)) {
                    AbstractJobHandler::SupportAction actionForReadSeek = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(),
                                                                                               AbstractJobHandler::JobErrorType::kSeekError);
                    *result = actionForReadSeek == AbstractJobHandler::SupportAction::kSkipAction;
                    return false;
                }
            }
        }
    } while (!isStopped() && actionForRead == AbstractJobHandler::SupportAction::kRetryAction);

    if (actionForRead != AbstractJobHandler::SupportAction::kNoAction) {
        *result = actionForRead == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::doWriteFile  Write file contents
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param fileInfo file information
 * \param fromDevice file device
 * \param data Data buffer
 * \param blockSize Data buffer size
 * \param readSize Write size
 * \param result result Output parameter: whether skip
 * \return Write successfully
 */
bool FileOperateBaseWorker::doWriteFile(const QUrl &fromUrl,
                                        const QUrl &toUrl,
                                        const AbstractFileInfoPointer &fileInfo,
                                        const QSharedPointer<DFile> &toDevice,
                                        const char *data, const qint64 &readSize, bool *result)
{
    qint64 currentPos = toDevice->pos();
    AbstractJobHandler::SupportAction actionForWrite = AbstractJobHandler::SupportAction::kNoAction;
    qint64 sizeWrite = 0;
    qint64 surplusSize = readSize;
    do {
        const char *surplusData = data;
        do {
            surplusData += sizeWrite;
            surplusSize -= sizeWrite;
            sizeWrite = toDevice->write(surplusData, surplusSize);
            if (Q_UNLIKELY(!stateCheck()))
                return false;
        } while (sizeWrite > 0 && sizeWrite < surplusSize);

        // 表示全部数据写入完成
        if (sizeWrite > 0) {
            break;
        }
        QString errorstr = QString(QObject::tr("Failed to write the file, cause: %1")).arg("some thing to do!");

        actionForWrite = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(), AbstractJobHandler::JobErrorType::kWriteError, errorstr);
        if (actionForWrite == AbstractJobHandler::SupportAction::kRetryAction) {
            if (!toDevice->seek(currentPos)) {
                AbstractJobHandler::SupportAction actionForWriteSeek = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(),
                                                                                            AbstractJobHandler::JobErrorType::kSeekError);
                *result = actionForWriteSeek == AbstractJobHandler::SupportAction::kSkipAction;

                return false;
            }
        }
    } while (!isStopped() && actionForWrite == AbstractJobHandler::SupportAction::kRetryAction);

    if (actionForWrite != AbstractJobHandler::SupportAction::kNoAction) {
        *result = actionForWrite == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    if (sizeWrite > 0) {
        toDevice->flush();
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::readAheadSourceFile Pre read source file content
 * \param fileInfo File information of source file
 */
void FileOperateBaseWorker::readAheadSourceFile(const AbstractFileInfoPointer &fileInfo)
{
    if (fileInfo->size() <= 0)
        return;
    std::string stdStr = fileInfo->url().path().toUtf8().toStdString();
    int fromfd = open(stdStr.data(), O_RDONLY);
    if (-1 != fromfd) {
        readahead(fromfd, 0, static_cast<size_t>(fileInfo->size()));
        close(fromfd);
    }
}

/*!
 * \brief FileOperateBaseWorker::checkDiskSpaceAvailable Check
 * whether the disk where the recycle bin directory is located
 * has space of the size of the source file
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param file URL of the source file
 * \param result Output parameter: whether skip
 * \return Is space available
 */
bool FileOperateBaseWorker::checkDiskSpaceAvailable(const QUrl &fromUrl,
                                                    const QUrl &toUrl,
                                                    QSharedPointer<QStorageInfo> targetStorageInfo,
                                                    bool *result)
{
    AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;

    do {
        targetStorageInfo->refresh();
        qint64 freeBytes = targetStorageInfo->bytesFree();

        if (!FileOperationsUtils::isFilesSizeOutLimit(fromUrl, freeBytes))
            action = doHandleErrorAndWait(fromUrl, toUrl, fromUrl, AbstractJobHandler::JobErrorType::kNotEnoughSpaceError);
    } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

    if (action != AbstractJobHandler::SupportAction::kNoAction) {
        *result = action == AbstractJobHandler::SupportAction::kSkipAction;
        return false;
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::deleteFile Delete file
 * \param fromUrl URL of the source file
 * \param toUrl Destination URL
 * \param fileInfo delete file information
 * \return Delete file successfully
 */
bool FileOperateBaseWorker::deleteFile(const QUrl &fromUrl, const QUrl &toUrl, const AbstractFileInfoPointer &fileInfo)
{
    if (!stateCheck())
        return false;

    AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;
    do {
        if (!handler->deleteFile(fileInfo->url())) {
            action = doHandleErrorAndWait(fromUrl, toUrl, fileInfo->url(), AbstractJobHandler::JobErrorType::kDeleteFileError, handler->errorString());
        }
    } while (isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);

    if (action == AbstractJobHandler::SupportAction::kSkipAction)
        return true;

    return action != AbstractJobHandler::SupportAction::kNoAction;
}

/*!
 * \brief FileOperateBaseWorker::copyFile Copy file
 * \param fromInfo File information of source file
 * \param toInfo File information of target file
 * \param reslut result Output parameter: whether skip
 * \return Whether the copied file is complete
 */
bool FileOperateBaseWorker::copyFile(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo, bool *reslut)
{
    //预先读取
    readAheadSourceFile(fromInfo);

    // 创建文件的divice
    QSharedPointer<DFile> fromDevice { nullptr }, toDevice { nullptr };
    if (!createFileDevices(fromInfo, toInfo, fromDevice, toDevice, reslut))
        return reslut;

    // 打开文件并创建
    if (!openFiles(fromInfo, toInfo, fromDevice, toDevice, reslut))
        return reslut;

    // 源文件大小如果为0
    if (fromInfo->size() <= 0) {
        // 对文件加权
        setTargetPermissions(fromInfo, toInfo);
        return true;
    }
    // 循环读取和写入文件，拷贝
    qint64 blockSize = fromInfo->size() > kMoveMaxBufferLength ? kMoveMaxBufferLength : fromInfo->size();
    char *data = new char[blockSize + 1];
    qint64 sizeRead = 0;
    do {
        if (!doReadFile(fromInfo->url(), toInfo->url(), fromInfo, fromDevice, data, blockSize, sizeRead, reslut)) {
            delete[] data;
            data = nullptr;
            return reslut;
        }

        if (!doWriteFile(fromInfo->url(), toInfo->url(), toInfo, toDevice, data, sizeRead, reslut)) {
            delete[] data;
            data = nullptr;
            return reslut;
        }
    } while (fromDevice->pos() == fromInfo->size());

    delete[] data;
    data = nullptr;

    // 对文件加权
    setTargetPermissions(fromInfo, toInfo);
    if (Q_UNLIKELY(!stateCheck())) {
        return false;
    }

    return true;
}

/*!
 * \brief FileOperateBaseWorker::copyDir Copy dir
 * \param fromInfo File information of source file
 * \param toInfo File information of target file
 * \param reslut result Output parameter: whether skip
 * \return Whether the copied dir is complete
 */
bool FileOperateBaseWorker::copyDir(const AbstractFileInfoPointer &fromInfo, const AbstractFileInfoPointer &toInfo, bool *reslut)
{
    // 检查文件的一些合法性，源文件是否存在，创建新的目标目录名称，检查新创建目标目录名称是否存在
    AbstractJobHandler::SupportAction action = AbstractJobHandler::SupportAction::kNoAction;
    QFileDevice::Permissions permissions = fromInfo->permissions();
    if (!toInfo->exists()) {
        do {
            if (handler->mkdir(toInfo->url())) {
                break;
            }
            action = doHandleErrorAndWait(fromInfo->url(), toInfo->url(), fromInfo->url(), AbstractJobHandler::JobErrorType::kMkdirError, QString(QObject::tr("Fail to create symlink, cause: %1")).arg(handler->errorString()));
        } while (!isStopped() && action == AbstractJobHandler::SupportAction::kRetryAction);
        if (AbstractJobHandler::SupportAction::kNoAction != action) {
            // skip write size += all file size in sources dir
            *reslut = AbstractJobHandler::SupportAction::kSkipAction == action;
            return false;
        }
    }

    if (fromInfo->countChildFile() <= 0) {
        handler->setPermissions(toInfo->url(), permissions);
        return true;
    }
    // 遍历源文件，执行一个一个的拷贝
    QString error;
    const AbstractDirIteratorPointer &iterator = DirIteratorFactory::create<AbstractDirIterator>(fromInfo->url(), &error);
    if (iterator) {
        doHandleErrorAndWait(fromInfo->url(), toInfo->url(), fromInfo->url(), AbstractJobHandler::JobErrorType::kProrogramError, QString(QObject::tr("create dir's iterator failed, case : %1")).arg(error));
        return false;
    }

    while (iterator->hasNext()) {
        if (!stateCheck()) {
            return false;
        }

        const QUrl &url = iterator->next();
        Q_UNUSED(url);
        const AbstractFileInfoPointer &info = iterator->fileInfo();
        if (!copyFile(info, toInfo, reslut)) {
            return false;
        }
    }

    handler->setPermissions(toInfo->url(), permissions);

    return true;
}

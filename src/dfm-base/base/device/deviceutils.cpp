// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "deviceutils.h"

#include "dfm-base/dfm_global_defines.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/base/application/settings.h"
#include "dfm-base/dbusservice/global_server_defines.h"
#include "dfm-base/utils/finallyutil.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/base/device/deviceproxymanager.h"
#include "dfm-base/dbusservice/global_server_defines.h"

#include <QVector>
#include <QDebug>
#include <QRegularExpressionMatch>
#include <QMutex>
#include <QSettings>

#include <libmount.h>
#include <fstab.h>
#include <sys/stat.h>

using namespace dfmbase;
using namespace GlobalServerDefines::DeviceProperty;

QString DeviceUtils::getBlockDeviceId(const QString &deviceDesc)
{
    QString dev(deviceDesc);
    if (dev.startsWith("/dev/"))
        dev.remove("/dev/");
    return kBlockDeviceIdPrefix + dev;
}

/*!
 * \brief DeviceUtils::getMountInfo
 * \param in: the mount src or target, /dev/sda is a source and /media/$USER/sda is target, e.g.
 * \param lookForMpt: if setted to true, then treat 'in' like a mount source
 * \return if lookForMpt is setted to true, then returns the mount target, otherwise returns mount source.
 */
QString DeviceUtils::getMountInfo(const QString &in, bool lookForMpt)
{
    libmnt_table *tab { mnt_new_table() };
    if (!tab)
        return {};
    FinallyUtil finally { [tab]() { if (tab) mnt_free_table(tab); } };
    if (mnt_table_parse_mtab(tab, nullptr) != 0) {
        qWarning() << "Invalid mnt_table_parse_mtab call";
        return {};
    }

    auto query = lookForMpt ? mnt_table_find_source : mnt_table_find_target;
    auto get = lookForMpt ? mnt_fs_get_target : mnt_fs_get_source;
    std::string stdPath { in.toStdString() };
    auto fs = query(tab, stdPath.c_str(), MNT_ITER_BACKWARD);
    if (fs)
        return { get(fs) };

    qWarning() << "Invalid libmnt_fs*";
    return {};
}

QUrl DeviceUtils::getSambaFileUriFromNative(const QUrl &url)
{
    Q_ASSERT(url.isValid());
    if (!DeviceUtils::isSamba(url))
        return url;

    QUrl smbUrl;
    smbUrl.setScheme(Global::Scheme::kSmb);
    static constexpr char kGvfsMatch[] { "^/root/\\.gvfs/smb|^/run/user/\\d+/gvfs/smb|^/root/\\.gvfs/smb" };
    static constexpr char kCifsMatch[] { "^/media/[\\s\\S]*/smbmounts/" };
    QString filePath { url.path() };
    if (hasMatch(filePath, kGvfsMatch)) {
        static QRegularExpression kGvfsSMBRegExp("/run/user/\\d+/gvfs/smb-share:server=(?<host>.*),share=(?<path>.*)",
                                                 QRegularExpression::DotMatchesEverythingOption
                                                         | QRegularExpression::DontCaptureOption
                                                         | QRegularExpression::OptimizeOnFirstUsageOption);
        const QRegularExpressionMatch &match { kGvfsSMBRegExp.match(filePath, 0, QRegularExpression::NormalMatch,
                                                                    QRegularExpression::DontCheckSubjectStringMatchOption) };
        const QString &host { match.captured("host") };
        const QString &path { match.captured("path") };
        smbUrl.setHost(host);
        smbUrl.setPath("/" + path);
    } else if (hasMatch(filePath, kCifsMatch)) {
        static QRegularExpression cifsMptPref { kCifsMatch };
        // mountRootDir like: `sharedir on 1.2.3.4`
        QString mountRootDir { filePath.remove(cifsMptPref).section("/", 0, 0) };
        const QString &shareDir { mountRootDir.section(" on ", 0, 0) };
        const QString &host { mountRootDir.section(" on ", 1, 1) };

        // subPath is after `sharedir on 1.2.3.4`
        const QString &suffixPath { filePath.remove(mountRootDir) };
        smbUrl.setHost(host);
        smbUrl.setPath("/" + shareDir + suffixPath);   // like : smb://1.2.3.4/sharedir/suffixPath
    } else {
        return url;
    }

    return smbUrl;
}

QString DeviceUtils::errMessage(dfmmount::DeviceError err)
{
    return DFMMOUNT::Utils::errorMessage(err);
}

/*!
 * \brief DeviceUtils::convertSuitableDisplayName
 * \param devInfo which is obtained by DeviceManager/DeviceProxyManger
 * \return a suitable device name,
 * if device's idLabel is empty, get the display name by size (if size is not 0) or Empty XXX disc (if it is empty disc)
 * and this function should never returns an empty string, if that happened, please check your input.
 */
QString DeviceUtils::convertSuitableDisplayName(const QVariantMap &devInfo)
{
    if (devInfo.value(kHintSystem).toBool()) {
        return nameOfSystemDisk(devInfo);
    } else if (devInfo.value(kIsEncrypted).toBool()) {
        return nameOfEncrypted(devInfo);
    } else if (devInfo.value(kOpticalDrive).toBool()) {
        return nameOfOptical(devInfo);
    } else {
        const QString &&label = devInfo.value(kIdLabel).toString();
        quint64 size = devInfo.value(kSizeTotal).toULongLong();
        return nameOfDefault(label, size);
    }
}

QString DeviceUtils::convertSuitableDisplayName(const QVariantHash &devInfo)
{
    QVariantMap map;
    for (auto iter = devInfo.cbegin(); iter != devInfo.cend(); ++iter)
        map.insert(iter.key(), iter.value());
    return convertSuitableDisplayName(map);
}

QString DeviceUtils::formatOpticalMediaType(const QString &media)
{
    static std::initializer_list<std::pair<QString, QString>> opticalmediakeys {
        { "optical", "Optical" },
        { "optical_cd", "CD-ROM" },
        { "optical_cd_r", "CD-R" },
        { "optical_cd_rw", "CD-RW" },
        { "optical_dvd", "DVD-ROM" },
        { "optical_dvd_r", "DVD-R" },
        { "optical_dvd_rw", "DVD-RW" },
        { "optical_dvd_ram", "DVD-RAM" },
        { "optical_dvd_plus_r", "DVD+R" },
        { "optical_dvd_plus_rw", "DVD+RW" },
        { "optical_dvd_plus_r_dl", "DVD+R/DL" },
        { "optical_dvd_plus_rw_dl", "DVD+RW/DL" },
        { "optical_bd", "BD-ROM" },
        { "optical_bd_r", "BD-R" },
        { "optical_bd_re", "BD-RE" },
        { "optical_hddvd", "HD DVD-ROM" },
        { "optical_hddvd_r", "HD DVD-R" },
        { "optical_hddvd_rw", "HD DVD-RW" },
        { "optical_mo", "MO" }
    };
    static QMap<QString, QString> opticalmediamap(opticalmediakeys);

    return opticalmediamap.value(media);
}

bool DeviceUtils::isAutoMountEnable()
{
    return Application::genericAttribute(Application::GenericAttribute::kAutoMount).toBool();
}

bool DeviceUtils::isAutoMountAndOpenEnable()
{
    return Application::genericAttribute(Application::GenericAttribute::kAutoMountAndOpen).toBool();
}

bool DeviceUtils::isWorkingOpticalDiscDev(const QString &dev)
{
    static constexpr char kBurnStateGroup[] { "BurnState" };
    static constexpr char kWoringKey[] { "Working" };

    if (dev.isEmpty())
        return false;

    if (Application::dataPersistence()->keys(kBurnStateGroup).contains(dev)) {
        const QMap<QString, QVariant> &info = Application::dataPersistence()->value(kBurnStateGroup, dev).toMap();
        return info.value(kWoringKey).toBool();
    }
    return false;
}

bool DeviceUtils::isWorkingOpticalDiscId(const QString &id)
{
    static constexpr char kBurnStateGroup[] { "BurnState" };
    static constexpr char kWoringKey[] { "Working" };
    static constexpr char kID[] { "id" };

    if (id.isEmpty())
        return false;

    auto &&keys { Application::dataPersistence()->keys(kBurnStateGroup) };
    for (const QString &dev : keys) {
        const QMap<QString, QVariant> &info = Application::dataPersistence()->value(kBurnStateGroup, dev).toMap();
        QString &&devID { info.value(kID).toString() };
        if (devID == id)
            return info.value(kWoringKey).toBool();
    }

    return false;
}

bool DeviceUtils::isSamba(const QUrl &url)
{
    if (url.scheme() == Global::Scheme::kSmb)
        return true;
    static const QString smbMatch { "(^/run/user/\\d+/gvfs/smb|^/root/\\.gvfs/smb|^/media/[\\s\\S]*/smbmounts)" };   // TODO(xust) /media/$USER/smbmounts might be changed in the future.}
    return hasMatch(url.path(), smbMatch);
}

bool DeviceUtils::isFtp(const QUrl &url)
{
    static const QString smbMatch { "(^/run/user/\\d+/gvfs/s?ftp|^/root/\\.gvfs/s?ftp)" };
    return hasMatch(url.path(), smbMatch);
}

bool DeviceUtils::isExternalBlock(const QUrl &url)
{
    return DeviceProxyManager::instance()->isFileOfExternalBlockMounts(url.path());
}

QMap<QString, QString> DeviceUtils::fstabBindInfo()
{
    // TODO(perf) this costs times when first painting. most of the time is spent on function 'stat'
    static QMutex mutex;
    static QMap<QString, QString> table;
    struct stat statInfo;
    int result = stat("/etc/fstab", &statInfo);

    QMutexLocker locker(&mutex);
    if (0 == result) {
        static quint32 lastModify = 0;
        if (lastModify != statInfo.st_mtime) {
            lastModify = static_cast<quint32>(statInfo.st_mtime);
            table.clear();
            struct fstab *fs;

            setfsent();
            while ((fs = getfsent()) != nullptr) {
                QString mntops(fs->fs_mntops);
                if (mntops.contains("bind"))
                    table.insert(fs->fs_spec, fs->fs_file);
            }
            endfsent();
        }
    }

    return table;
}

QString DeviceUtils::nameOfSystemDisk(const QVariantMap &datas)
{
    const auto &lst = Application::genericSetting()->value(BlockAdditionalProperty::kAliasGroupName, BlockAdditionalProperty::kAliasItemName).toList();

    for (const QVariant &v : lst) {
        const QVariantMap &map = v.toMap();
        if (map.value(BlockAdditionalProperty::kAliasItemUUID).toString() == datas.value(kUUID).toString()) {
            return map.value(BlockAdditionalProperty::kAliasItemAlias).toString();
        }
    }

    QString label = datas.value(kIdLabel).toString();
    qlonglong size = datas.value(kSizeTotal).toLongLong();

    // get system disk name if there is no alias
    if (datas.value(kMountPoint).toString() == "/")
        return QObject::tr("System Disk");
    if (datas.value(kIdLabel).toString().startsWith("_dde_"))
        return QObject::tr("Data Disk");
    return nameOfDefault(label, size);
}

QString DeviceUtils::nameOfOptical(const QVariantMap &datas)
{
    QString label = datas.value(kIdLabel).toString();
    if (!label.isEmpty())
        return label;

    static const std::initializer_list<std::pair<QString, QString>> opticalMedias {
        { "optical", "Optical" },
        { "optical_cd", "CD-ROM" },
        { "optical_cd_r", "CD-R" },
        { "optical_cd_rw", "CD-RW" },
        { "optical_dvd", "DVD-ROM" },
        { "optical_dvd_r", "DVD-R" },
        { "optical_dvd_rw", "DVD-RW" },
        { "optical_dvd_ram", "DVD-RAM" },
        { "optical_dvd_plus_r", "DVD+R" },
        { "optical_dvd_plus_rw", "DVD+RW" },
        { "optical_dvd_plus_r_dl", "DVD+R/DL" },
        { "optical_dvd_plus_rw_dl", "DVD+RW/DL" },
        { "optical_bd", "BD-ROM" },
        { "optical_bd_r", "BD-R" },
        { "optical_bd_re", "BD-RE" },
        { "optical_hddvd", "HD DVD-ROM" },
        { "optical_hddvd_r", "HD DVD-R" },
        { "optical_hddvd_rw", "HD DVD-RW" },
        { "optical_mo", "MO" }
    };
    static const QMap<QString, QString> discMapper(opticalMedias);
    static const QVector<std::pair<QString, QString>> discVector(opticalMedias);

    auto totalSize { datas.value(kSizeTotal).toULongLong() };

    if (datas.value(kOptical).toBool()) {   // medium loaded
        if (datas.value(kOpticalBlank).toBool()) {   // show empty disc name
            QString mediaType = datas.value(kMedia).toString();
            return QObject::tr("Blank %1 Disc").arg(discMapper.value(mediaType, QObject::tr("Unknown")));
        } else {
            // totalSize changed after disc mounted
            auto udiks2Size { datas.value(kUDisks2Size).toULongLong() };
            return nameOfDefault(label, udiks2Size > 0 ? udiks2Size : totalSize);
        }
    } else {   // show drive name, medium is not loaded
        auto medias = datas.value(kMediaCompatibility).toStringList();
        QString maxCompatibility;
        for (auto iter = discVector.crbegin(); iter != discVector.crend(); ++iter) {
            if (medias.contains(iter->first))
                return QObject::tr("%1 Drive").arg(iter->second);
        }
    }

    return nameOfDefault(label, totalSize);
}

QString DeviceUtils::nameOfEncrypted(const QVariantMap &datas)
{
    if (datas.value(kCleartextDevice).toString().length() > 1
        && !datas.value(BlockAdditionalProperty::kClearBlockProperty).toMap().isEmpty()) {
        auto clearDevData = datas.value(BlockAdditionalProperty::kClearBlockProperty).toMap();
        QString clearDevLabel = clearDevData.value(kIdLabel).toString();
        qlonglong clearDevSize = clearDevData.value(kSizeTotal).toLongLong();
        return nameOfDefault(clearDevLabel, clearDevSize);
    } else {
        return QObject::tr("%1 Encrypted").arg(nameOfSize(datas.value(kSizeTotal).toLongLong()));
    }
}

QString DeviceUtils::nameOfDefault(const QString &label, const quint64 &size)
{
    if (label.isEmpty())
        return QObject::tr("%1 Volume").arg(nameOfSize(size));
    return label;
}

/*!
 * \brief DeviceUtils::nameOfSize
 * \param size
 * \return
 * infact this function is basically the same as formatSize in FileUtils, but I don't want import any other
 * dfm-base files except device*, so I make a simple copy here.
 */
QString DeviceUtils::nameOfSize(const quint64 &size)
{
    quint64 num = size;
    if (num < 0) {
        qWarning() << "Negative number passed to formatSize():" << num;
        num = 0;
    }

    QStringList list;
    qreal fileSize(num);

    list << "B"
         << "KB"
         << "MB"
         << "GB"
         << "TB";   // should we use KiB since we use 1024 here?

    QStringListIterator i(list);
    QString unit = i.hasNext() ? i.next() : QStringLiteral("B");

    int index = 0;
    while (i.hasNext()) {
        if (fileSize < 1024) {
            break;
        }

        unit = i.next();
        fileSize /= 1024;
        index++;
    }
    return QString("%1 %2").arg(QString::number(fileSize, 'f', 1)).arg(unit);
}

bool DeviceUtils::checkDiskEncrypted()
{
    static bool isEncrypted = false;
    static std::once_flag flag;

    std::call_once(flag, [&] {
#ifdef COMPILE_ON_V23
    // TODO (liuzhangjian) check disk encrypted on v23
#elif COMPILE_ON_V20
                QSettings settings("/etc/deepin/deepin-user-experience", QSettings::IniFormat);
                isEncrypted = settings.value("ExperiencePlan/FullDiskEncrypt", false).toBool();
#endif
    });

    return isEncrypted;
}

QStringList DeviceUtils::encryptedDisks()
{
    static QStringList deviceList;
    static std::once_flag flag;

    std::call_once(flag, [&] {
#ifdef COMPILE_ON_V23
    // TODO (liuzhangjian) get encrypted disks on v23
#elif COMPILE_ON_V20
                QSettings settings("/etc/deepin-installer.conf", QSettings::IniFormat);
                const QString &value = settings.value("DI_CRYPT_INFO", "").toString();
                if (!value.isEmpty()) {
                    QStringList groupList = value.split(';');
                    for (const auto &group : groupList) {
                        QStringList device = group.split(':');
                        if (!device.isEmpty())
                            deviceList << device.first();
                    }
                }
#endif
    });

    return deviceList;
}

bool DeviceUtils::isSubpathOfDlnfs(const QString &path)
{
    return findDlnfsPath(path, [](const QString &target, const QString &compare) {
        return target.startsWith(compare);
    });
}

bool DeviceUtils::isMountPointOfDlnfs(const QString &path)
{
    return findDlnfsPath(path, [](const QString &target, const QString &compare) {
        return target == compare;
    });
}

bool DeviceUtils::findDlnfsPath(const QString &target, Compare func)
{
    Q_ASSERT(func);
    libmnt_table *tab { mnt_new_table() };
    libmnt_iter *iter = mnt_new_iter(MNT_ITER_BACKWARD);

    FinallyUtil finally([=] {
        if (tab) mnt_free_table(tab);
        if (iter) mnt_free_iter(iter);
    });
    Q_UNUSED(finally);

    auto unifyPath = [](const QString &path) {
        return path.endsWith("/") ? path : path + "/";
    };

    int ret = mnt_table_parse_mtab(tab, nullptr);
    if (ret != 0) {
        qWarning() << "device: cannot parse mtab" << ret;
        return false;
    }

    libmnt_fs *fs = nullptr;
    while (mnt_table_next_fs(tab, iter, &fs) == 0) {
        if (!fs)
            continue;
        if (strcmp("dlnfs", mnt_fs_get_source(fs)) == 0) {
            QString mpt = unifyPath(mnt_fs_get_target(fs));
            if (func(unifyPath(target), mpt))
                return true;
        }
    }

    return false;
}

bool DeviceUtils::hasMatch(const QString &txt, const QString &rex)
{
    QRegularExpression re(rex);
    QRegularExpressionMatch match = re.match(txt);
    return match.hasMatch();
}
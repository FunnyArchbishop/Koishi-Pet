/**
 * DeskPet Qt - 精灵加载器实现
 *
 * 智能加载: .gif 优先, 否则回退到 .png/.jpg/.bmp。
 * GIF 使用 QMovie 驱动动画, 静态图使用 QPixmap。
 */

#include "sprite_loader.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

// ============================================================
// 从图片文件加载 (自动检测格式)
// ============================================================
SpriteFrame loadSpriteFromFile(const QString& filePath) {
    SpriteFrame result;

    QFileInfo fi(filePath);
    if (!fi.exists()) return result;

    QString suffix = fi.suffix().toLower();

    if (suffix == "gif") {
        // ---- GIF 动画 ----
        result.movie.reset(new QMovie(filePath));
        if (result.movie->isValid()) {
            result.movie->start();
            result.pixmap = result.movie->currentPixmap();
        } else {
            result.movie.clear();
        }
    } else {
        // ---- 静态图片 ----
        result.pixmap = QPixmap(filePath);
    }

    return result;
}

// ============================================================
// 按基础名加载: xxx.gif 优先 → xxx.png 回退
// ============================================================
SpriteFrame loadSpriteByBaseName(const QString& baseNameWithoutExt) {
    // 先尝试 GIF
    QString gifPath = baseNameWithoutExt + ".gif";
    if (QFileInfo::exists(gifPath)) {
        return loadSpriteFromFile(gifPath);
    }

    // 回退到 PNG
    QString pngPath = baseNameWithoutExt + ".png";
    if (QFileInfo::exists(pngPath)) {
        return loadSpriteFromFile(pngPath);
    }

    // 最后尝试原始路径 (可能是其他格式)
    return loadSpriteFromFile(baseNameWithoutExt);
}

// ============================================================
// 从目录加载触发帧 (.gif 优先)
// ============================================================
TriggerMap loadTriggerFrames(const QString& dirPath) {
    TriggerMap triggers;

    QDir dir(dirPath);
    if (!dir.exists()) return triggers;

    // 收集所有 gif 和 png 文件, gif 优先
    QMap<QString, QString> bestFiles; // baseName → fullPath

    QStringList filters = {"*.gif", "*.png"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& info : files) {
        QString name = info.completeBaseName();
        QString path = info.absoluteFilePath();
        // gif 优先: 如果已存在则跳过 png
        if (!bestFiles.contains(name) || info.suffix().toLower() == "gif") {
            bestFiles[name] = path;
        }
    }

    // 加载
    for (auto it = bestFiles.begin(); it != bestFiles.end(); ++it) {
        SpriteFrame frame = loadSpriteFromFile(it.value());
        if (frame.isValid()) {
            triggers.insert(it.key(), frame);
        }
    }

    return triggers;
}

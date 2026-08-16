/**
 * DeskPet Qt - 精灵加载器接口
 *
 * 支持静态图片 (PNG/JPG/BMP) 和动画 GIF。
 * 同名 .gif 文件优先于 .png 加载，实现动画桌宠。
 * 用户可替换 assets/ 下的图片/GIF 自定义角色。
 */

#ifndef DESKPET_SPRITE_LOADER_H
#define DESKPET_SPRITE_LOADER_H

#include <QPixmap>
#include <QMovie>
#include <QString>
#include <QMap>
#include <QObject>
#include <QSharedPointer>

// ============================================================
// 精灵帧结构 — 支持静态图 & GIF 动画
// ============================================================
struct SpriteFrame {
    QPixmap pixmap;                              // 静态图片
    QSharedPointer<QMovie> movie;                // GIF 动画 (共享所有权, 安全拷贝)

    bool isValid() const { return !pixmap.isNull() || (movie && movie->isValid()); }
    bool isGif()    const { return movie && movie->isValid(); }

    /** 获取当前帧 (GIF 返回动画当前帧, 静态返回原图) */
    QPixmap currentFrame() const {
        if (movie && movie->state() != QMovie::NotRunning) {
            return movie->currentPixmap();
        }
        return pixmap;
    }

    /** 启动 GIF 播放 */
    void startGif() {
        if (movie && movie->state() == QMovie::NotRunning) {
            movie->start();
        }
    }
};

// ============================================================
// 触发帧映射: 触发名 → 精灵帧
// ============================================================
using TriggerMap = QMap<QString, SpriteFrame>;

// ============================================================
// 加载函数
// ============================================================

/** 从图片文件加载精灵帧 (自动检测 .gif 优先) */
SpriteFrame loadSpriteFromFile(const QString& filePath);

/**
 * 按基础名加载: 先尝试 baseName.gif, 不存在则 baseName.png
 * 例如 loadSpriteByBaseName("assets/sprite_idle") 会先找 sprite_idle.gif
 */
SpriteFrame loadSpriteByBaseName(const QString& baseNameWithoutExt);

/** 从目录加载所有触发帧 (.gif 优先, 其次 .png) */
TriggerMap loadTriggerFrames(const QString& dirPath);

#endif // DESKPET_SPRITE_LOADER_H

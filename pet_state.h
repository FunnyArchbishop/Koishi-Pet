/**
 * DeskPet Qt - koishi状态 & 配置常量
 *
 * 桌面koishi Qt 版 — 状态枚举 & 可调参数
 * 使用图片文件 (PNG) 作为koishi素材，支持透明通道
 */

#ifndef DESKPET_PET_STATE_H
#define DESKPET_PET_STATE_H

#include <QString>

// ============================================================
// 显示尺寸
// ============================================================
constexpr int PET_SIZE       = 200;   // 窗口显示尺寸 (原 512 缩小约 2.5 倍)
constexpr int PET_IMG_SIZE   = 128;   // 素材图片尺寸 (建议使用方形 PNG)

// ============================================================
// 行为参数
// ============================================================
constexpr int ANIM_INTERVAL   = 300;   // 动画帧间隔 (ms)
constexpr int STATE_CHANGE_MS = 5000;  // 状态切换间隔 (ms)
constexpr int TRIGGER_DURATION= 3000;  // 触发动画持续时间 (ms)

// ============================================================
// 物理参数 (速度单位 px/s)
// ============================================================
constexpr double GRAVITY_ACCEL    = 1200.0;  // 重力加速度
constexpr double MAX_FALL_SPEED   = 1400.0;  // 最大下落速度
constexpr double FLOOR_FRICTION   = 0.78;    // 落地摩擦 (每 tick)
constexpr double AIR_FRICTION     = 0.985;   // 空中摩擦 (每 tick)
constexpr double WALL_RESTITUTION = 0.45;    // 撞墙反弹系数
constexpr double MAX_THROW_SPEED  = 1600.0;  // 甩出最大速度
constexpr double JUMP_VELOCITY    = -520.0;  // 跳跃初速度
constexpr double WALK_SPEED_MIN   = 60.0;    // 地面行走最小速度
constexpr double WALK_SPEED_MAX   = 160.0;   // 地面行走最大速度

// ============================================================
// 睡眠 / 闲聊参数
// ============================================================
constexpr int SLEEP_AFTER_MS   = 20000;   // 闲置多久后入睡
constexpr int IDLE_LINE_MIN_MS = 12000;   // 闲聊最小间隔
constexpr int IDLE_LINE_MAX_MS = 35000;   // 闲聊最大间隔
constexpr int BUBBLE_GAP       = 6;       // 气泡与宠物间距
constexpr int HOVER_PET_MS     = 1500;    // 悬停抚摸间隔 (每 1.5s +1 好感度)

// ============================================================
// 阴影参数 (预留)
// ============================================================
constexpr int SHADOW_LEFT    = 3;
constexpr int SHADOW_BOTTOM  = 9;
constexpr int SHADOW_RIGHT   = 3;
constexpr int SHADOW_HEIGHT  = 3;

// ============================================================
// 精灵文件基础路径 (不带扩展名, 自动 .gif 优先 → .png 回退)
// 用户可替换 assets/ 下的 .gif 或 .png 文件自定义角色
// ============================================================
inline const QString SPRITE_IDLE   = QStringLiteral("assets/sprite_idle");
inline const QString SPRITE_BLINK  = QStringLiteral("assets/sprite_blink");
inline const QString SPRITE_WALK1  = QStringLiteral("assets/sprite_walk1");
inline const QString SPRITE_WALK2  = QStringLiteral("assets/sprite_walk2");
inline const QString SPRITE_SIT    = QStringLiteral("assets/sprite_sit");
inline const QString TRIGGER_DIR   = QStringLiteral("assets/triggers");

// ============================================================
// GIF 模式精灵路径
// ============================================================
inline const QString SPRITE_STAND  = QStringLiteral("assets/stand");
inline const QString SPRITE_CREEP  = QStringLiteral("assets/creep");

// ============================================================
// Trigger 显示名称映射
// ============================================================
inline QString triggerDisplayName(const QString& internalName) {
    if (internalName == "koishi") return QStringLiteral("koishi_cute");
    return internalName;
}

// ============================================================
// 好感度阈值对话 (20/40/60/80/100)
// ============================================================
constexpr int AFFECTION_PER_MINUTE = 5;   // 每分钟增加好感度
constexpr int AFFECTION_MAX        = 100;  // 好感度上限
constexpr int BUBBLE_DURATION_MS   = 3000; // 气泡显示时间 (ms)

inline QString affectionDialogue(int affection) {
    if (affection >= 100) return QStringLiteral("\u5982\u679c\u662f\u4f60\u7684\u8bdd\u2026\u4e00\u5b9a\u4e0d\u4f1a\u5fd8\u8bb0\u6211\u5427\u2026\u2665 \u59d0\u59d0\u2026\u6211\u597d\u50cf\u2026\u6709\u4e86\u91cd\u8981\u7684\u4eba\u4e86\u2026");
    if (affection >= 80)  return QStringLiteral("\u660e\u660e\u6211\u53ea\u662f\u8def\u8fb9\u7684\u5c0f\u77f3\u5b50\u2026\u4e3a\u4ec0\u4e48\u4f60\u603b\u80fd\u627e\u5230\u6211\u5462\uff1f\u4f60\u2026\u771f\u662f\u4e2a\u5947\u602a\u7684\u4eba\u5462\u2026");
    if (affection >= 60)  return QStringLiteral("\u6211\u628a\u7b2c\u4e09\u53ea\u773c\u95ed\u4e0a\u4e86\u2026\u56e0\u4e3a\u4e0d\u60f3\u88ab\u8ba8\u538c\u3002\u4f46\u662f\u2026\u59d0\u59d0\u7ed9\u4e86\u6211\u5ba0\u7269\uff0c\u8bf4\u8fd9\u6837\u4f1a\u6162\u6162\u597d\u8d77\u6765\u7684\u2026");
    if (affection >= 40)  return QStringLiteral("\u4f60\u2026\u7279\u610f\u6765\u627e\u6211\u7684\u5417\uff1f\u771f\u662f\u4e2a\u6709\u8da3\u7684\u4eba\u5462\u3002\u6211\u53ea\u662f\u6f2b\u65e0\u76ee\u7684\u5730\u5230\u5904\u95f2\u901b\u800c\u5df2\u2026");
    if (affection >= 20)  return QStringLiteral("\u554a\u2026\uff1f\u4f60\u2026\u80fd\u770b\u89c1\u6211\u5417\uff1f\u660e\u660e\u6211\u5e94\u8be5\u50cf\u8def\u8fb9\u7684\u5c0f\u77f3\u5b50\u4e00\u6837\u2026\u4e0d\u4f1a\u88ab\u4eba\u6ce8\u610f\u5230\u7684\u2026");
    return QStringLiteral("\u2026\u2026\uff1f\u2026\u2026\u2026\u2026");
}

// ============================================================
// 闲聊台词池 (发呆时随机冒泡)
// ============================================================
inline int idleDialogueCount() { return 16; }

inline QString idleDialogue(int index) {
    static const QString lines[] = {
        QStringLiteral("\u2026\u2026\uff1f"),
        QStringLiteral("\u3042\u308c\u2026\uff1f"),
        QStringLiteral("\u3053\u3044\u3057\u2026\u3053\u3044\u3057\u2026"),
        QStringLiteral("\u2026\u2026\u4f60\u80fd\u770b\u89c1\u6211\u5417\u2026\uff1f"),
        QStringLiteral("\u59d0\u59d0\u2026\u597d\u60f3\u59d0\u59d0\u2026"),
        QStringLiteral("\u8def\u8fb9\u7684\u5c0f\u77f3\u5b50\u2026\u4e5f\u4f1a\u7d2f\u7684\u2026"),
        QStringLiteral("\u4eca\u5929\u2026\u4e5f\u8bf7\u591a\u5173\u7167\u2026"),
        QStringLiteral("\u547c\u547c\u2026"),
        QStringLiteral("\u6211\u662f\u2026\u53e4\u660e\u5730\u604b\u2026"),
        QStringLiteral("\u2026\u2026\uff01\uff1f"),
        QStringLiteral("\u2026\u2026\u597d\u5b89\u9759\u2026\u2026"),
        QStringLiteral("\u4f60\u8fd8\u5728\u5417\u2026\u2026\uff1f"),
        QStringLiteral("\u604b\u604b\u2026\u2026\u6709\u70b9\u60f3\u4f60\u4e86\u2026\u2026"),
        QStringLiteral("\u5c0f\u77f3\u5b50\u2026\u2026\u5728\u54ea\u91cc\u2026\u2026"),
        QStringLiteral("\uff08\u5de6\u53f3\u5f20\u671b\uff09"),
        QStringLiteral("\u547c\u2026\u2026\u8212\u670d\u2026\u2026"),
    };
    int n = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    return lines[((index % n) + n) % n];
}

// ============================================================
// 时段问候 (每小时第一次点击)
// ============================================================
inline QString timeGreeting(int hour) {
    if (hour >= 5 && hour <= 9)   return QStringLiteral("\u65e9\u4e0a\u597d\u2026\u4eca\u5929\u4e5f\u8981\u52a0\u6cb9\u54e6\u2665");
    if (hour >= 10 && hour <= 13) return QStringLiteral("\u5348\u5b89\u2026\u8bb0\u5f97\u5403\u996d\u2026");
    if (hour >= 14 && hour <= 17) return QStringLiteral("\u4e0b\u5348\u597d\u2026\u4f60\u7d2f\u4e86\u5417\uff1f");
    if (hour >= 18 && hour <= 22) return QStringLiteral("\u665a\u4e0a\u597d\u2026\u4eca\u5929\u8f9b\u82e6\u4e86\u2026");
    return QStringLiteral("\u591c\u6df1\u4e86\u2026\u65e9\u70b9\u4f11\u606f\u5427\u2026");
}

// ============================================================
// 今日运势 (抽签)
// ============================================================
inline int fortuneCount() { return 8; }

inline QString fortuneLine(int index) {
    static const QString lines[] = {
        QStringLiteral("\u5927\u5409 \u2605\n\u4eca\u5929\u4e5f\u4f1a\u88ab\u604b\u604b\u8bb0\u4f4f\u54e6\u2665"),
        QStringLiteral("\u4e2d\u5409\n\u9002\u5408\u53d1\u5446\uff0c\u50cf\u604b\u604b\u4e00\u6837"),
        QStringLiteral("\u5c0f\u5409\n\u8def\u8fb9\u7684\u5c0f\u77f3\u5b50\u4eca\u5929\u4e5f\u95ea\u95ea\u53d1\u5149"),
        QStringLiteral("\u5409\n\u4f1a\u6709\u5c0f\u5c0f\u7684\u597d\u4e8b\u53d1\u751f"),
        QStringLiteral("\u672b\u5409\n\u6162\u6162\u6765\uff0c\u604b\u604b\u4f1a\u7b49\u4f60"),
        QStringLiteral("\u51f6\n\u2026\uff1f\u6ca1\u5173\u7cfb\u7684\uff0c\u604b\u604b\u966a\u7740\u4f60"),
        QStringLiteral("\u5927\u51f6\n\u4e0d\u7ba1\u53d1\u751f\u4ec0\u4e48\uff0c\u604b\u604b\u90fd\u4e0d\u4f1a\u5fd8\u8bb0\u4f60"),
        QStringLiteral("\u604b\u604b\u5409 \u2605\n\u88ab\u604b\u604b\u9009\u4e2d\u4e86\uff01\u4eca\u5929\u6574\u5929\u6709\u4eba\u966a"),
    };
    int n = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    return lines[((index % n) + n) % n];
}

// ============================================================
// 连击戳戳反应
// ============================================================
inline QString comboLine(int index) {
    static const QString lines[] = {
        QStringLiteral("\u545c\u54c7\u2026\u88ab\u4f60\u6233\u6655\u4e86\u5566\u2026\u4f46\u662f\u2026\u597d\u5f00\u5fc3\u2026\u2665"),
        QStringLiteral("\u522b\u3001\u522b\u6233\u90a3\u4e48\u5feb\u5566\u2026\u604b\u604b\u8981\u8f6c\u5708\u5708\u4e86\u2026"),
        QStringLiteral("\u5535\u2026\u4f60\u597d\u559c\u6b22\u604b\u604b\u5462\u2026"),
    };
    int n = static_cast<int>(sizeof(lines) / sizeof(lines[0]));
    return lines[((index % n) + n) % n];
}

// ============================================================
// 成就里程碑台词
// ============================================================
inline QString achievementDialogue(qint64 total) {
    if (total >= 5000) return QStringLiteral("\u2026\u604b\u604b\u5df2\u7ecf\u6570\u4e0d\u6e05\u8fd9\u662f\u7b2c\u51e0\u6b21\u4e86\u2026\u8c22\u8c22\u4f60\u2026");
    if (total >= 1000) return QStringLiteral("\u4e00\u5343\u6b21\u2026\u4f60\u2026\u771f\u7684\u4e0d\u4f1a\u5fd8\u8bb0\u6211\u5417\u2026");
    if (total >= 500)  return QStringLiteral("\u5df2\u7ecf\u4e94\u767e\u6b21\u4e86\u5462\u2026");
    if (total >= 100)  return QStringLiteral("\u4e00\u767e\u6b21\u2026\u4f60\u597d\u6e29\u67d4\u2026");
    if (total >= 50)   return QStringLiteral("\u4e94\u5341\u6b21\u4e86\u2026\u4f60\u771f\u7684\u5f88\u559c\u6b22\u604b\u604b\u5462\u2026");
    if (total >= 10)   return QStringLiteral("\u5341\u6b21\u4e86\u2026\u4f60\u2026\u662f\u5728\u627e\u6211\u5417\uff1f");
    return QString();
}

// ============================================================
// 显示模式枚举
// ============================================================
enum class PetMode {
    SPRITE_MODE,  // 多帧 PNG 精灵模式
    GIF_MODE      // stand.gif + creep.gif 模式
};

// ============================================================
// 宠物状态枚举
// ============================================================
enum class PetState {
    IDLE,
    WALKING,
    SITTING,
    SLEEPING,
    TRIGGER
};

#endif // DESKPET_PET_STATE_H

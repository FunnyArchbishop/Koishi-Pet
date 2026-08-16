/**
 * DeskPet Qt - 宠物窗口
 *
 * 透明置顶无边框窗口，显示koishi并处理交互动画。
 * 支持物理重力/落地/甩动、睡眠、闲聊、托盘、持久化设置。
 */

#ifndef DESKPET_PETWINDOW_H
#define DESKPET_PETWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QPoint>
#include <QMap>
#include <QString>
#include <QVector>
#include <QColor>
#include <QSystemTrayIcon>

#include "pet_state.h"
#include "sprite_loader.h"

class QMenu;
class QScreen;
class QPainter;
class QEnterEvent;

// ============================================================
// 飘浮爱心粒子
// ============================================================
struct HeartParticle {
    qreal x = 0, y = 0;   // 位置 (窗口坐标)
    qreal vx = 0, vy = 0; // 速度 (px/tick)
    int   age = 0;        // 已存活 tick 数
    int   life = 40;      // 总寿命 (tick)
    int   type = 0;       // 0=爱心, 1=星星
};

class PetWindow : public QWidget {
    Q_OBJECT

public:
    explicit PetWindow(QWidget* parent = nullptr);
    ~PetWindow() override;

protected:
    // ---- 事件处理 ----
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onTimerTick();         // 每 50ms 更新逻辑 + 重绘
    void onTriggerAction();     // 右键菜单触发动画
    void onResizeAction();      // 右键菜单调整大小
    void onSwitchMode();        // 切换图片/GIF 模式
    void onToggleWalk(bool checked);     // 自动行走开关
    void onToggleGravity(bool checked);  // 物理重力开关
    void onToggleOnTop(bool checked);    // 窗口置顶开关
    void onToggleClickThrough(bool checked); // 点击穿透(幽灵)开关
    void onToggleSound(bool checked);       // 音效开关
    void onOpacityAction();     // 透明度调整
    void onTintAction();        // 换装/换色
    void onOpenSettings();      // 设置对话框
    void onShowAffection();     // 显示当前好感度
    void onShowStats();         // 显示统计
    void onShowFortune();       // 今日运势抽签
    void onShowOmikuji();       // 东方幻存神签 (弹窗显示图片)
    void onSkinAction();        // 切换皮肤
    void onResetPosition();     // 回到屏幕中央
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShowHide();      // 托盘显示/隐藏

private:
    // ---- 精灵获取 ----
    SpriteFrame& currentSprite();
    QPixmap currentPixmap();

    // ---- 状态更新 ----
    void updateState();
    void physicsStep(double dt);
    void clampToScreen();
    QScreen* currentScreen() const;

    // ---- 气泡 ----
    void calcBubbleSize();
    void drawBubble(QPainter& painter);
    void drawShadow(QPainter& painter, int centerY, int bob);
    void spawnHearts(int count);
    void spawnSparkles(int count);
    void updateHearts();
    void drawHearts(QPainter& painter);
    void showBubble(const QString& text);
    void hideBubble();
    void syncWindowGeometry();
    QString getAffectionText() const;
    QString statsText() const;
    void checkMilestones();
    void playBeep();

    // ---- 设置 ----
    void loadSettings();
    void saveSettings();
    void applyOpacity();

    // ---- 托盘 ----
    void setupTray();

    // ---- 睡眠 / 触发 ----
    void enterSleep();
    void wakeUp();
    void playRandomTrigger();

    // ---- 皮肤 ----
    void loadSprites(const QString& baseDir);
    QStringList availableSkins() const;
    void updateTrayIcon();

    // ============================================================
    // 宠物状态
    // ============================================================
    QPoint   m_pos       = {100, 100};    // 宠物左上角位置 (屏幕坐标)
    double   m_vx = 0.0, m_vy = 0.0;      // 速度 (px/s)
    bool     m_onFloor   = false;         // 是否落地
    int      m_dirX      = 1;             // 朝向: 1=右, -1=左
    PetState m_state     = PetState::IDLE;
    PetState m_prevState = PetState::IDLE;
    int      m_animFrame = 0;

    // ---- 拖拽 ----
    bool    m_dragging     = false;
    QPoint  m_dragOffset;
    QPoint  m_clickPos;
    bool    m_wasClick     = false;
    qint64  m_lastDragTime = 0;
    double  m_throwVx = 0.0, m_throwVy = 0.0;

    // ---- 触发动画 ----
    QString m_activeTrigger;
    qint64  m_triggerStartTime = 0;

    // ---- 精灵帧 (PNG 模式) ----
    SpriteFrame m_sfIdle;
    SpriteFrame m_sfBlink;
    SpriteFrame m_sfWalk1;
    SpriteFrame m_sfWalk2;
    SpriteFrame m_sfSit;
    TriggerMap  m_triggers;

    // ---- 精灵帧 (GIF 模式) ----
    SpriteFrame m_sfStand;
    SpriteFrame m_sfCreep;

    // ---- 显示模式 ----
    PetMode m_mode = PetMode::SPRITE_MODE;

    // ---- 计时 ----
    qint64 m_lastAnim        = 0;
    qint64 m_lastStateChange = 0;
    qint64 m_lastTick        = 0;

    // ---- 定时器 ----
    QTimer* m_timer = nullptr;

    // ---- 可调大小 / 透明度 / 换装 ----
    int    m_petSize = PET_SIZE;
    double m_opacity = 1.0;
    double m_walkSpeed = 100.0;  // 基础行走速度 (px/s)
    QColor m_tint;               // 无效 QColor 表示原色
    QString m_skin;              // 当前皮肤目录名 (空 = 默认 assets/)

    // ---- 好感度系统 ----
    int     m_affection        = 0;
    qint64  m_lastAffectionInc = 0;
    bool    m_showBubble       = false;
    bool    m_sleepBubble      = false;
    QString m_bubbleText;
    qint64  m_bubbleShowTime   = 0;
    int     m_bubbleW          = 0;
    int     m_bubbleH          = 0;
    bool    m_bubbleUp         = false;
    int     m_petDrawY         = 0;   // 宠物在窗口内的纵向偏移 (气泡在上方时 >0)

    // ---- 睡眠 ----
    bool    m_sleeping         = false;
    qint64  m_lastInteraction  = 0;

    // ---- 闲聊 ----
    qint64 m_lastIdleLine  = 0;
    int    m_nextIdleLineIn = 0;

    // ---- 行为开关 ----
    bool m_walkEnabled    = true;
    bool m_gravityEnabled = true;
    bool m_onTop          = true;
    bool m_clickThrough   = false;
    bool m_soundEnabled   = false;

    // ---- 托盘 ----
    QSystemTrayIcon* m_tray = nullptr;
    QAction* m_trayWalk    = nullptr;
    QAction* m_trayGravity = nullptr;
    QAction* m_trayOnTop   = nullptr;
    QAction* m_trayGhost   = nullptr;
    QAction* m_traySound   = nullptr;

    void updateTrayTooltip();
    void syncTrayChecks();

    // ---- 爱心粒子 ----
    QVector<HeartParticle> m_hearts;

    // ---- 悬停抚摸 ----
    bool    m_hovering    = false;
    qint64  m_lastPetTime = 0;

    // ---- 问候 / 连击 ----
    int     m_lastGreetingHour = -1;
    qint64  m_lastClickTime    = 0;
    int     m_clickStreak      = 0;

    // ---- 动效 ----
    double m_squash = 0.0;   // 落地压缩量 (0~0.3, 随时间衰减)

    // ---- 统计 & 成就 ----
    qint64 m_totalClicks   = 0;
    qint64 m_totalPets     = 0;
    qint64 m_totalTriggers = 0;
    qint64 m_playTimeMs    = 0;
    qint64 m_playStart     = 0;
    int    m_clickMilestone = 0;
    int    m_petMilestone   = 0;
    qint64 m_lastAutoSave   = 0;
};

#endif // DESKPET_PETWINDOW_H

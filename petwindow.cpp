/**
 * DeskPet Qt - 宠物窗口实现
 *
 * 透明置顶窗口 + 精灵动画 + 拖拽/甩动 + 物理重力 + 睡眠/闲聊
 * 基于 QPainter 渲染，QTimer 驱动动画循环。
 */

#include "petwindow.h"

#include <QPainter>
#include <QIcon>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QFontMetrics>
#include <QRadialGradient>
#include <QDate>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QtMath>

// ============================================================
// 工具函数
// ============================================================
namespace {
// 返回 [low, high) 范围内的随机 double
double randRange(double low, double high) {
    return low + (high - low) * QRandomGenerator::global()->generateDouble();
}
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

PetWindow::PetWindow(QWidget* parent)
    : QWidget(parent)
{
    // ---- 无边框透明窗口 ----
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // ---- 读取持久化设置 (位置/大小/模式/好感度/开关) ----
    loadSettings();

    m_petSize = qBound(100, m_petSize, 400);
    setFixedSize(m_petSize, m_petSize);
    applyOpacity();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_onTop);
    setWindowFlag(Qt::WindowTransparentForInput, m_clickThrough);

    // ---- 加载精灵 (.gif 优先, .png 回退), 支持皮肤目录 ----
    loadSprites(m_skin.isEmpty()
                    ? QStringLiteral("assets/")
                    : QStringLiteral("assets/skins/") + m_skin + QStringLiteral("/"));

    // ---- 位置校正到屏幕内 ----
    clampToScreen();
    move(m_pos);

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastAnim          = now;
    m_lastStateChange   = now;
    m_lastAffectionInc  = now;
    m_lastInteraction   = now;
    m_lastIdleLine      = now;
    m_nextIdleLineIn    = QRandomGenerator::global()->bounded(IDLE_LINE_MIN_MS, IDLE_LINE_MAX_MS + 1);
    m_playStart         = now;
    m_lastAutoSave      = now;

    // ---- 系统托盘 ----
    setupTray();

    // ---- 动画循环 ----
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &PetWindow::onTimerTick);
    m_timer->start(50);

    show();
}

PetWindow::~PetWindow() {
    saveSettings();
}

// ============================================================
// 精灵获取
// ============================================================

SpriteFrame& PetWindow::currentSprite() {
    // ---- GIF 模式: stand.gif + creep.gif 为主, 缺失时优雅回退 ----
    if (m_mode == PetMode::GIF_MODE) {
        // 触发特效: 优先触发帧, 回退待机
        if (m_state == PetState::TRIGGER) {
            auto it = m_triggers.find(m_activeTrigger);
            if (it != m_triggers.end()) return it.value();
            if (m_sfStand.isValid()) return m_sfStand;
            return m_sfIdle;
        }

        switch (m_state) {
        case PetState::WALKING:
            // 优先 creep.gif, 缺则回退 PNG 走帧交替
            if (m_sfCreep.isValid()) return m_sfCreep;
            return (m_animFrame % 2 == 0) ? m_sfWalk1 : m_sfWalk2;

        case PetState::SITTING:
        case PetState::SLEEPING:
            // 坐下/睡眠使用 sit 素材 (sit.gif 优先, 回退 sprite_sit.png),
            // 再回退 stand
            if (m_sfSit.isValid()) return m_sfSit;
            if (m_sfStand.isValid()) return m_sfStand;
            return m_sfIdle;

        default: // IDLE
            // 优先 stand.gif, 缺则回退 PNG 眨眼/待机
            if (m_sfStand.isValid()) return m_sfStand;
            return (m_animFrame == 1) ? m_sfBlink : m_sfIdle;
        }
    }

    // ---- PNG 精灵模式 ----
    switch (m_state) {
    case PetState::SITTING:
    case PetState::SLEEPING:
        return m_sfSit;
    case PetState::WALKING:
        return (m_animFrame % 2 == 0) ? m_sfWalk1 : m_sfWalk2;
    case PetState::TRIGGER: {
        auto it = m_triggers.find(m_activeTrigger);
        if (it != m_triggers.end()) return it.value();
        return m_sfIdle;
    }
    default: // IDLE
        return (m_animFrame == 1) ? m_sfBlink : m_sfIdle;
    }
}

QPixmap PetWindow::currentPixmap() {
    SpriteFrame& sf = currentSprite();
    sf.startGif();  // 确保 GIF 在播放
    return sf.currentFrame();
}

void PetWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 全局透明度: 逐像素透明窗口在 Windows 下无法用 setWindowOpacity,
    // 改为在绘制时统一缩放 alpha, 对精灵/阴影/气泡/爱心整体生效。
    painter.setOpacity(m_opacity);

    QPixmap pm = currentPixmap();
    if (pm.isNull()) return;

    // ---- 待机呼吸浮动 (空闲/坐下/睡眠时轻微上下起伏) ----
    int bob = 0;
    if (!m_dragging
        && (m_state == PetState::IDLE
            || m_state == PetState::SITTING
            || m_state == PetState::SLEEPING)) {
        qint64 t = QDateTime::currentMSecsSinceEpoch();
        bob = qRound(qSin(t / 340.0) * 2.0);
    }

    // ---- 缩放到窗口尺寸 ----
    QPixmap scaled = pm.scaled(m_petSize, m_petSize,
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);

    // ---- 水平翻转 (朝向) ----
    if (m_dirX < 0) {
        scaled = scaled.transformed(QTransform().scale(-1, 1));
    }

    // ---- 换装/换色 (剪影染色 + 保留细节) ----
    if (m_tint.isValid()) {
        QPixmap tinted = scaled;
        QPainter tp(&tinted);
        tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tp.fillRect(tinted.rect(), m_tint);
        tp.setCompositionMode(QPainter::CompositionMode_SourceOver);
        tp.setOpacity(0.45);
        tp.drawPixmap(0, 0, scaled);
        tp.end();
        scaled = tinted;
    }

    // ---- 地面阴影 (先画在背后, 让角色看起来接地) ----
    drawShadow(painter, m_petDrawY + m_petSize - 3, bob);

    // ---- 空中倾斜 + 落地压缩动效 (围绕脚底中心) ----
    double tilt = 0.0;
    if (!m_onFloor && m_gravityEnabled && !m_dragging) {
        tilt = qBound(-1.0, m_vx / 900.0, 1.0) * 0.30;
    }

    painter.save();
    int pivotX = m_petSize / 2;
    int pivotY = m_petDrawY + m_petSize;
    QTransform tf;
    tf.translate(pivotX, pivotY);
    tf.rotate(qRadiansToDegrees(tilt));
    tf.scale(1.0 + m_squash, 1.0 - m_squash);
    tf.translate(-pivotX, -pivotY);
    painter.setTransform(tf, true);

    // ---- 绘制精灵 (透明背景自动处理), 支持气泡在上方时下移 ----
    painter.drawPixmap(0, m_petDrawY + bob, scaled);
    painter.restore();

    // ---- 对话气泡 ----
    if (m_showBubble) {
        drawBubble(painter);
    }

    // ---- 飘浮爱心 ----
    drawHearts(painter);
}

// ============================================================
// 定时器回调
// ============================================================

void PetWindow::onTimerTick() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastTick == 0) m_lastTick = now;
    double dt = (now - m_lastTick) / 1000.0;
    if (dt > 0.1) dt = 0.1;
    if (dt <= 0.0) dt = 0.02;
    m_lastTick = now;

    // ---- affection increment (+5 per minute) ----
    if (m_affection < AFFECTION_MAX
        && (now - m_lastAffectionInc) >= (60000 / AFFECTION_PER_MINUTE)) {
        m_lastAffectionInc = now;
        m_affection = qMin(m_affection + 1, AFFECTION_MAX);
    }

    // ---- 悬停抚摸: 鼠标悬停时每 1.5s +1 好感度 + 爱心 ----
    if (m_hovering && !m_dragging && m_state != PetState::TRIGGER) {
        if (m_lastPetTime == 0) m_lastPetTime = now;
        if ((now - m_lastPetTime) > HOVER_PET_MS) {
            m_lastPetTime = now;
            if (m_affection < AFFECTION_MAX) m_affection = qMin(m_affection + 1, AFFECTION_MAX);
            m_totalPets++;
            checkMilestones();
            spawnHearts(1);
        }
    } else {
        m_lastPetTime = now;
    }

    // ---- bubble timeout (睡眠气泡常驻, 直到醒来) ----
    if (m_showBubble && !m_sleepBubble && (now - m_bubbleShowTime) > BUBBLE_DURATION_MS) {
        hideBubble();
    }

    // ---- 触发动画持续计时 ----
    if (m_state == PetState::TRIGGER
        && (now - m_triggerStartTime) > TRIGGER_DURATION) {
        m_state = m_prevState;
        m_activeTrigger.clear();
        m_vx = m_vy = 0.0;
    }

    // ---- 睡眠判定 (落地 + 长时间无交互) ----
    if (!m_dragging && m_state != PetState::TRIGGER && !m_sleeping
        && m_gravityEnabled && m_onFloor
        && (now - m_lastInteraction) > SLEEP_AFTER_MS) {
        enterSleep();
    }

    // ---- 闲聊 (发呆时随机冒泡) ----
    if (!m_dragging && m_state != PetState::TRIGGER
        && !m_sleeping && !m_showBubble) {
        if ((now - m_lastIdleLine) > m_nextIdleLineIn) {
            m_lastIdleLine = now;
            m_nextIdleLineIn = QRandomGenerator::global()->bounded(
                IDLE_LINE_MIN_MS, IDLE_LINE_MAX_MS + 1);
            showBubble(idleDialogue(QRandomGenerator::global()->bounded(idleDialogueCount())));
        }
    }

    // ---- 状态切换 ----
    if (!m_dragging && m_state != PetState::TRIGGER
        && !m_sleeping && !m_showBubble
        && (now - m_lastStateChange) > STATE_CHANGE_MS) {
        m_lastStateChange = now;
        updateState();
    }

    // ---- 物理 & 移动 ----
    if (!m_dragging && m_state != PetState::TRIGGER) {
        physicsStep(dt);
    }

    // ---- 动画帧推进 ----
    if ((now - m_lastAnim) > ANIM_INTERVAL) {
        m_lastAnim = now;
        m_animFrame = (m_animFrame + 1) % 4;
    }

    // ---- 爱心粒子更新 ----
    updateHearts();

    // ---- 动效衰减 (落地压缩) ----
    if (m_squash > 0.001) m_squash *= 0.85;
    else                  m_squash = 0.0;

    // ---- 定期自动保存 (每 60s, 累积在线时长/统计) ----
    if ((now - m_lastAutoSave) > 60000) {
        m_lastAutoSave = now;
        saveSettings();
    }

    update();  // 触发重绘
}

// ============================================================
// 状态机
// ============================================================

void PetWindow::updateState() {
    int r = QRandomGenerator::global()->bounded(100);

    // ---- 关闭自动行走: 只发呆 / 坐下 ----
    if (!m_walkEnabled) {
        m_state = (r < 50) ? PetState::SITTING : PetState::IDLE;
        m_vx = 0.0;
        return;
    }

    if (m_gravityEnabled) {
        // ---- 地面模式: 行走 / 跳跃 / 坐下 / 发呆 ----
        if (r < 40) {
            m_state = PetState::WALKING;
            m_dirX  = (QRandomGenerator::global()->bounded(2) == 0) ? -1 : 1;
            m_vx = m_dirX * m_walkSpeed * randRange(0.8, 1.2);
        } else if (r < 60) {
            // 跳跃
            m_state = PetState::WALKING;
            m_dirX  = (QRandomGenerator::global()->bounded(2) == 0) ? -1 : 1;
            m_vx = m_dirX * m_walkSpeed * randRange(0.2, 0.6);
            if (m_onFloor) m_vy = JUMP_VELOCITY;
        } else if (r < 85) {
            m_state = PetState::SITTING;
            m_vx = 0.0;
        } else {
            m_state = PetState::IDLE;
            m_vx = 0.0;
        }
    } else {
        // ---- 自由漫游模式 (原逻辑) ----
        if (r < 40) {
            m_state = PetState::WALKING;
            m_vx = m_dirX * m_walkSpeed * randRange(0.8, 1.2);
            m_vy = randRange(-60.0, 60.0);
        } else if (r < 80) {
            m_state = PetState::SITTING;
            m_vx = m_vy = 0.0;
        } else {
            m_state = PetState::IDLE;
            m_vx = m_vy = 0.0;
        }
    }
}

// ============================================================
// 物理 & 移动 & 屏幕边界检测
// ============================================================

void PetWindow::physicsStep(double dt) {
    // ---- 重力 ----
    if (m_gravityEnabled) {
        m_vy += GRAVITY_ACCEL * dt;
        if (m_vy > MAX_FALL_SPEED) m_vy = MAX_FALL_SPEED;
    }

    // ---- 积分 ----
    m_pos += QPoint(qRound(m_vx * dt), qRound(m_vy * dt));

    QScreen* screen = currentScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    int w = m_petSize;
    int h = m_petSize;
    double floorY = avail.bottom() - h;

    m_onFloor = false;

    if (m_gravityEnabled) {
        // ---- 落地 (站在任务栏上方), 轻微回弹 ----
        if (m_pos.y() >= floorY) {
            m_pos.setY(static_cast<int>(floorY));
            if (m_vy > 250.0) {
                m_squash = qBound(0.0, (m_vy - 150.0) / 3000.0, 0.30);
                m_vy = -m_vy * 0.20;
            } else {
                m_vy = 0.0;
            }
            m_onFloor = true;
        }
        // ---- 天花板 ----
        if (m_pos.y() < avail.top()) {
            m_pos.setY(avail.top());
            if (m_vy < 0.0) m_vy = -m_vy * WALL_RESTITUTION;
        }
    } else {
        // ---- 自由模式: 四边弹跳 ----
        if (m_pos.y() < avail.top()) {
            m_pos.setY(avail.top());
            m_vy = -m_vy * WALL_RESTITUTION;
        } else if (m_pos.y() + h > avail.bottom()) {
            m_pos.setY(avail.bottom() - h);
            m_vy = -m_vy * WALL_RESTITUTION;
        }
    }

    // ---- 左右墙 ----
    if (m_pos.x() < avail.left()) {
        m_pos.setX(avail.left());
        m_vx = -m_vx * WALL_RESTITUTION;
        m_dirX = 1;
    } else if (m_pos.x() + w > avail.right()) {
        m_pos.setX(avail.right() - w);
        m_vx = -m_vx * WALL_RESTITUTION;
        m_dirX = -1;
    }

    // ---- 摩擦 ----
    bool walking = (m_state == PetState::WALKING);
    if (walking) {
        // 行走保持速度, 不额外摩擦
    } else if (m_gravityEnabled) {
        m_vx *= (m_onFloor ? FLOOR_FRICTION : AIR_FRICTION);
    } else {
        m_vx *= AIR_FRICTION;
        m_vy *= AIR_FRICTION;
    }

    // ---- 静止阈值 ----
    if (qAbs(m_vx) < 1.0) m_vx = 0.0;
    if (!m_gravityEnabled && qAbs(m_vy) < 1.0) m_vy = 0.0;

    // ---- 朝向 ----
    if (m_vx > 1.0)       m_dirX = 1;
    else if (m_vx < -1.0) m_dirX = -1;

    syncWindowGeometry();
}

void PetWindow::clampToScreen() {
    QScreen* screen = currentScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    int w = m_petSize;
    int h = m_petSize;
    if (m_pos.x() < avail.left())           m_pos.setX(avail.left());
    if (m_pos.x() + w > avail.right())      m_pos.setX(avail.right() - w);
    if (m_pos.y() < avail.top())            m_pos.setY(avail.top());
    if (m_pos.y() + h > avail.bottom())     m_pos.setY(avail.bottom() - h);
}

QScreen* PetWindow::currentScreen() const {
    QPoint center = m_pos + QPoint(m_petSize / 2, m_petSize / 2);
    QScreen* s = QGuiApplication::screenAt(center);
    if (s) return s;
    return QGuiApplication::primaryScreen();
}

// ============================================================
// 鼠标交互 - 拖拽 / 甩动 / 点击
// ============================================================

void PetWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int oldDrawY = m_petDrawY;   // 在收起气泡前记录偏移
        wakeUp();          // 唤醒睡眠
        m_lastInteraction = QDateTime::currentMSecsSinceEpoch();

        hideBubble();      // 抓取时收起气泡, 简化拖拽几何

        m_dragging   = true;
        m_dragOffset = event->pos() - QPoint(0, oldDrawY);
        m_clickPos   = m_dragOffset;
        m_wasClick   = true;  // 先假设是点击, 移动超过阈值则视为拖拽

        m_throwVx = m_throwVy = 0.0;
        m_lastDragTime = 0;

        if (m_state != PetState::TRIGGER) {
            m_state = PetState::IDLE;
        }
        m_vx = m_vy = 0.0;
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void PetWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        // detect drag: if moved > 5px, it's a drag not a click
        if ((event->pos() - m_clickPos).manhattanLength() > 5) {
            m_wasClick = false;
        }

        QPoint newPos = event->globalPosition().toPoint() - m_dragOffset;

        // ---- 计算甩动速度 (指数平滑) ----
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastDragTime != 0) {
            double ddt = (now - m_lastDragTime) / 1000.0;
            if (ddt > 0.0001) {
                double vx = (newPos.x() - m_pos.x()) / ddt;
                double vy = (newPos.y() - m_pos.y()) / ddt;
                m_throwVx = m_throwVx * 0.5 + vx * 0.5;
                m_throwVy = m_throwVy * 0.5 + vy * 0.5;
            }
        }
        m_lastDragTime = now;

        m_pos = newPos;
        move(m_pos);
    }
    QWidget::mouseMoveEvent(event);
}

void PetWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_wasClick && m_dragging) {
            // ---- 点击 (非拖拽) ----
            m_totalClicks++;

            // 连击检测: 800ms 内连续点击 3 次触发特殊反应
            qint64 clickNow = QDateTime::currentMSecsSinceEpoch();
            if (m_lastClickTime > 0 && (clickNow - m_lastClickTime) < 800) {
                m_clickStreak++;
            } else {
                m_clickStreak = 1;
            }
            m_lastClickTime = clickNow;

            if (m_clickStreak >= 3) {
                // 连击反应
                m_clickStreak = 0;
                showBubble(comboLine(QRandomGenerator::global()->bounded(3)));
                spawnHearts(6);
                spawnSparkles(8);
                playBeep();
            } else {
                // 每小时第一次点击 → 时段问候, 其余显示好感度
                int hour = QDateTime::currentDateTime().time().hour();
                if (m_lastGreetingHour != hour) {
                    m_lastGreetingHour = hour;
                    showBubble(timeGreeting(hour));
                } else {
                    showBubble(getAffectionText());
                }
                checkMilestones();   // 里程碑台词优先覆盖
                spawnHearts(3);
                playBeep();
            }
            m_vx = m_vy = 0.0;
        } else {
            // ---- 甩动: 继承拖拽速度 ----
            m_vx = qBound(-MAX_THROW_SPEED, m_throwVx, MAX_THROW_SPEED);
            m_vy = qBound(-MAX_THROW_SPEED, m_throwVy, MAX_THROW_SPEED);
        }
        m_dragging = false;
        m_wasClick = false;
        m_throwVx = m_throwVy = 0.0;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void PetWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        playRandomTrigger();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PetWindow::enterEvent(QEnterEvent* event) {
    m_hovering = true;
    QWidget::enterEvent(event);
}

void PetWindow::leaveEvent(QEvent* event) {
    m_hovering = false;
    QWidget::leaveEvent(event);
}

// ============================================================
// 右键菜单
// ============================================================

void PetWindow::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background: #2b2b2b;
            color: #e0e0e0;
            border: 1px solid #555;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: #4a6fa5;
        }
        QMenu::separator {
            height: 1px;
            background: #555;
            margin: 4px 8px;
        }
    )");

    // ---- 触发动画 (如果有) ----
    if (!m_triggers.isEmpty()) {
        for (auto it = m_triggers.begin(); it != m_triggers.end(); ++it) {
            QString displayName = triggerDisplayName(it.key());
            QString label = QString::fromUtf8("\u2728 ") + displayName;
            QAction* act = menu.addAction(label);
            act->setData(it.key());
            connect(act, &QAction::triggered, this, &PetWindow::onTriggerAction);
        }
        menu.addSeparator();
    }

    // ---- 模式切换 ----
    QMenu* modeMenu = menu.addMenu(QString::fromUtf8("\u6a21\u5f0f\u5207\u6362"));
    QAction* spriteMode = modeMenu->addAction(QString::fromUtf8("\u56fe\u7247\u6a21\u5f0f (PNG\u591a\u5e27)"));
    QAction* gifMode    = modeMenu->addAction(QString::fromUtf8("GIF\u6a21\u5f0f (stand+creep)"));
    spriteMode->setCheckable(true);
    gifMode->setCheckable(true);
    spriteMode->setChecked(m_mode == PetMode::SPRITE_MODE);
    gifMode->setChecked(m_mode == PetMode::GIF_MODE);
    spriteMode->setData((int)PetMode::SPRITE_MODE);
    gifMode->setData((int)PetMode::GIF_MODE);
    connect(spriteMode, &QAction::triggered, this, &PetWindow::onSwitchMode);
    connect(gifMode,    &QAction::triggered, this, &PetWindow::onSwitchMode);

    // ---- 皮肤子菜单 ----
    QMenu* skinMenu = menu.addMenu(QString::fromUtf8("\u76ae\u80a4"));
    QAction* defaultSkin = skinMenu->addAction(QString::fromUtf8("\u9ed8\u8ba4"));
    defaultSkin->setData(QString());
    defaultSkin->setCheckable(true);
    defaultSkin->setChecked(m_skin.isEmpty());
    connect(defaultSkin, &QAction::triggered, this, &PetWindow::onSkinAction);
    const QStringList skins = availableSkins();
    for (const QString& sk : skins) {
        QAction* act = skinMenu->addAction(sk);
        act->setData(sk);
        act->setCheckable(true);
        act->setChecked(m_skin == sk);
        connect(act, &QAction::triggered, this, &PetWindow::onSkinAction);
    }

    // ---- 大小调整子菜单 ----
    QMenu* sizeMenu = menu.addMenu(QString::fromUtf8("\u8c03\u6574\u5927\u5c0f"));
    struct SizeOption { int size; QString label; };
    const SizeOption sizes[] = {
        { 100, QString::fromUtf8("\u2606 \u5c0f (100)") },
        { 150, QString::fromUtf8("\u2606 \u8f83\u5c0f (150)") },
        { 200, QString::fromUtf8("\u2605 \u4e2d (200)") },
        { 300, QString::fromUtf8("\u2606 \u8f83\u5927 (300)") },
        { 400, QString::fromUtf8("\u2606 \u5927 (400)") },
    };
    for (const auto& opt : sizes) {
        QAction* act = sizeMenu->addAction(opt.label);
        act->setData(opt.size);
        act->setCheckable(true);
        if (opt.size == m_petSize) act->setChecked(true);
        connect(act, &QAction::triggered, this, &PetWindow::onResizeAction);
    }

    // ---- 透明度子菜单 ----
    QMenu* opacityMenu = menu.addMenu(QString::fromUtf8("\u900f\u660e\u5ea6"));
    const int opacities[] = { 20, 40, 60, 80, 100 };
    for (int op : opacities) {
        QAction* act = opacityMenu->addAction(QString::number(op) + QStringLiteral("%"));
        act->setData(op);
        act->setCheckable(true);
        if (qRound(m_opacity * 100) == op) act->setChecked(true);
        connect(act, &QAction::triggered, this, &PetWindow::onOpacityAction);
    }

    // ---- 换装/换色子菜单 ----
    QMenu* tintMenu = menu.addMenu(QString::fromUtf8("\u6362\u88c5\u6362\u8272"));
    struct TintOption { QString label; QColor color; };
    const TintOption tints[] = {
        { QStringLiteral("\u539f\u8272"), QColor() },
        { QStringLiteral("\u7c89\u7ea2"), QColor(255, 90, 140) },
        { QStringLiteral("\u5929\u84dd"), QColor(90, 150, 255) },
        { QStringLiteral("\u7d2b\u8272"), QColor(170, 100, 240) },
        { QStringLiteral("\u68ee\u7eff"), QColor(90, 200, 120) },
        { QStringLiteral("\u6a59\u91d1"), QColor(255, 180, 60) },
        { QStringLiteral("\u51b0\u84dd"), QColor(120, 220, 240) },
    };
    for (const auto& t : tints) {
        QAction* act = tintMenu->addAction(t.label);
        act->setData(QVariant(t.color));
        act->setCheckable(true);
        bool cur = m_tint.isValid() ? (m_tint == t.color) : (!t.color.isValid());
        act->setChecked(cur);
        connect(act, &QAction::triggered, this, &PetWindow::onTintAction);
    }

    // ---- 行为开关 ----
    QMenu* behaviorMenu = menu.addMenu(QString::fromUtf8("\u884c\u4e3a\u8bbe\u7f6e"));
    QAction* walkAct = behaviorMenu->addAction(QString::fromUtf8("\u81ea\u52a8\u884c\u8d70"));
    walkAct->setCheckable(true);
    walkAct->setChecked(m_walkEnabled);
    connect(walkAct, &QAction::toggled, this, &PetWindow::onToggleWalk);

    QAction* gravityAct = behaviorMenu->addAction(QString::fromUtf8("\u7269\u7406\u91cd\u529b"));
    gravityAct->setCheckable(true);
    gravityAct->setChecked(m_gravityEnabled);
    connect(gravityAct, &QAction::toggled, this, &PetWindow::onToggleGravity);

    QAction* onTopAct = behaviorMenu->addAction(QString::fromUtf8("\u7a97\u53e3\u7f6e\u9876"));
    onTopAct->setCheckable(true);
    onTopAct->setChecked(m_onTop);
    connect(onTopAct, &QAction::toggled, this, &PetWindow::onToggleOnTop);

    QAction* ghostAct = behaviorMenu->addAction(QString::fromUtf8("\u70b9\u51fb\u7a7f\u900f (\u5e7d\u7075)"));
    ghostAct->setCheckable(true);
    ghostAct->setChecked(m_clickThrough);
    connect(ghostAct, &QAction::toggled, this, &PetWindow::onToggleClickThrough);

    QAction* soundAct = behaviorMenu->addAction(QString::fromUtf8("\u97f3\u6548"));
    soundAct->setCheckable(true);
    soundAct->setChecked(m_soundEnabled);
    connect(soundAct, &QAction::toggled, this, &PetWindow::onToggleSound);

    menu.addSeparator();

    // ---- 其它操作 ----
    QAction* affectionAct = menu.addAction(QString::fromUtf8("\u2665 \u67e5\u770b\u597d\u611f\u5ea6"));
    connect(affectionAct, &QAction::triggered, this, &PetWindow::onShowAffection);

    QAction* fortuneAct = menu.addAction(QString::fromUtf8("\u2728 \u4eca\u65e5\u8fd0\u52bf"));
    connect(fortuneAct, &QAction::triggered, this, &PetWindow::onShowFortune);

    QAction* omikujiAct = menu.addAction(QString::fromUtf8("\u5e7b\u5b58\u795e\u7b7e"));
    connect(omikujiAct, &QAction::triggered, this, &PetWindow::onShowOmikuji);

    QAction* statsAct = menu.addAction(QString::fromUtf8("\u7edf\u8ba1\u4fe1\u606f"));
    connect(statsAct, &QAction::triggered, this, &PetWindow::onShowStats);

    QAction* centerAct = menu.addAction(QString::fromUtf8("\u56de\u5230\u5c4f\u5e55\u4e2d\u592e"));
    connect(centerAct, &QAction::triggered, this, &PetWindow::onResetPosition);

    QAction* settingsAct = menu.addAction(QString::fromUtf8("\u8bbe\u7f6e\u2026"));
    connect(settingsAct, &QAction::triggered, this, &PetWindow::onOpenSettings);

    menu.addSeparator();

    QAction* exitAction = menu.addAction(QString::fromUtf8("\u9000\u51fa DeskPet"));
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    menu.exec(event->globalPos());
}

// ============================================================
// 菜单槽函数
// ============================================================

void PetWindow::onTriggerAction() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;

    QString name = act->data().toString();
    if (!m_triggers.contains(name)) return;

    wakeUp();
    m_lastInteraction = QDateTime::currentMSecsSinceEpoch();

    m_prevState         = m_state;
    m_state             = PetState::TRIGGER;
    m_activeTrigger     = name;
    m_triggerStartTime  = QDateTime::currentMSecsSinceEpoch();
    m_vx = m_vy = 0.0;
    m_totalTriggers++;
    spawnHearts(5);
    playBeep();
}

void PetWindow::onResizeAction() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;

    int newSize = act->data().toInt();
    if (newSize == m_petSize) return;

    // 保持宠物中心不变
    QPoint center = m_pos + QPoint(m_petSize / 2, m_petSize / 2);
    m_petSize = newSize;
    m_pos = center - QPoint(m_petSize / 2, m_petSize / 2);
    hideBubble();
    clampToScreen();
    syncWindowGeometry();

    // 确保置顶状态正确
    setWindowFlag(Qt::WindowStaysOnTopHint, m_onTop);
    show();

    saveSettings();
    update();
}

void PetWindow::onSwitchMode() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;

    PetMode newMode = (PetMode)act->data().toInt();
    if (newMode == m_mode) return;

    m_mode = newMode;

    if (m_mode == PetMode::GIF_MODE) {
        m_sfStand.startGif();
        m_sfCreep.startGif();
        m_sfSit.startGif();   // 坐下/睡眠若为 GIF 也提前启动
    }

    saveSettings();
    update();
}

void PetWindow::onToggleWalk(bool checked) {
    m_walkEnabled = checked;
    if (!checked) {
        m_state = PetState::IDLE;
        m_vx = m_vy = 0.0;
    }
    syncTrayChecks();
    saveSettings();
}

void PetWindow::onToggleGravity(bool checked) {
    m_gravityEnabled = checked;
    m_onFloor = false;
    syncTrayChecks();
    saveSettings();
}

void PetWindow::onToggleOnTop(bool checked) {
    m_onTop = checked;
    setWindowFlag(Qt::WindowStaysOnTopHint, m_onTop);
    show();
    syncTrayChecks();
    saveSettings();
}

void PetWindow::onToggleClickThrough(bool checked) {
    m_clickThrough = checked;
    setWindowFlag(Qt::WindowTransparentForInput, m_clickThrough);
    show();
    syncTrayChecks();
    updateTrayTooltip();
    if (checked && m_tray) {
        // 开启时提示如何关闭, 避免宠物"消失"后找不到
        m_tray->showMessage(
            QStringLiteral("DeskPet - \u70b9\u51fb\u7a7f\u900f"),
            QStringLiteral("\u5e7d\u7075\u6a21\u5f0f\u5df2\u5f00\u542f\n"
                           "\u53f3\u952e\u6258\u76d8\u56fe\u6807\uff0c\u6216\u5355\u51fb\u6258\u76d8\u56fe\u6807\uff0c"
                           "\u5373\u53ef\u5173\u95ed\u5e76\u627e\u56de\u5ba0\u7269"),
            QSystemTrayIcon::Information, 5000);
    }
    saveSettings();
}

void PetWindow::onToggleSound(bool checked) {
    m_soundEnabled = checked;
    syncTrayChecks();
    saveSettings();
}

void PetWindow::onOpacityAction() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    m_opacity = act->data().toInt() / 100.0;
    applyOpacity();
    saveSettings();
}

void PetWindow::onTintAction() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    m_tint = act->data().value<QColor>();
    saveSettings();
    update();
}

void PetWindow::onShowAffection() {
    showBubble(getAffectionText());
}

void PetWindow::onShowStats() {
    QMessageBox::information(this, QStringLiteral("DeskPet \u7edf\u8ba1"), statsText());
}

void PetWindow::onShowFortune() {
    // 今日运势每天只抽一次, 当天重复查看返回同一支签
    QSettings s(QStringLiteral("DeskPetQt"), QStringLiteral("KoishiPet"));
    QString today = QDate::currentDate().toString(Qt::ISODate);
    int idx;
    if (s.value(QStringLiteral("fortuneDate")).toString() == today) {
        idx = s.value(QStringLiteral("fortuneIndex"), 0).toInt();
    } else {
        idx = QRandomGenerator::global()->bounded(fortuneCount());
        s.setValue(QStringLiteral("fortuneDate"), today);
        s.setValue(QStringLiteral("fortuneIndex"), idx);
    }
    showBubble(fortuneLine(idx));
    spawnSparkles(6);
    playBeep();
}

void PetWindow::onShowOmikuji() {
    // ---- 读取御神签清单 (crawl_omikuji.py 生成) ----
    QString manifestPath = QStringLiteral("assets/omikuji/manifest.txt");
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::information(this,
            QStringLiteral("\u5e7b\u5b58\u795e\u7b7e"),
            QStringLiteral("\u672a\u627e\u5230\u5fa1\u795e\u7b7e\u8d44\u6e90\uff1aassets/omikuji/manifest.txt\n\u8bf7\u8fd0\u884c python crawl_omikuji.py \u751f\u6210\u3002"));
        return;
    }
    QStringList lines;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.isEmpty()) lines << line;
    }
    f.close();
    if (lines.isEmpty()) {
        QMessageBox::information(this,
            QStringLiteral("\u5e7b\u5b58\u795e\u7b7e"),
            QStringLiteral("\u5fa1\u795e\u7b7e\u5217\u8868\u4e3a\u7a7a\u3002"));
        return;
    }

    // ---- 每天只抽一次 ----
    QSettings s(QStringLiteral("DeskPetQt"), QStringLiteral("KoishiPet"));
    QString today = QDate::currentDate().toString(Qt::ISODate);
    int idx;
    if (s.value(QStringLiteral("omikujiDate")).toString() == today) {
        idx = s.value(QStringLiteral("omikujiIndex"), 0).toInt();
    } else {
        idx = QRandomGenerator::global()->bounded(lines.size());
        s.setValue(QStringLiteral("omikujiDate"), today);
        s.setValue(QStringLiteral("omikujiIndex"), idx);
    }
    idx = qBound(0, idx % lines.size(), lines.size() - 1);

    QStringList parts = lines[idx].split(QStringLiteral("|"));
    QString filename  = parts.value(0);
    QString character = parts.value(1);
    QString fortune   = parts.value(2);

    QString imgPath = QStringLiteral("assets/omikuji/") + filename;
    QPixmap img(imgPath);
    if (img.isNull()) {
        QMessageBox::information(this,
            QStringLiteral("\u5e7b\u5b58\u795e\u7b7e"),
            QStringLiteral("\u56fe\u7247\u52a0\u8f7d\u5931\u8d25\uff1a") + filename);
        return;
    }

    // ---- 弹窗显示 ----
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("\u4e1c\u65b9\u5e7b\u5b58\u795e\u7b7e"));
    dlg.setWindowFlags(dlg.windowFlags() | Qt::WindowStaysOnTopHint);

    auto* layout = new QVBoxLayout(&dlg);

    int maxW = 480;
    if (img.width() > maxW) {
        img = img.scaledToWidth(maxW, Qt::SmoothTransformation);
    }
    auto* imgLabel = new QLabel;
    imgLabel->setPixmap(img);
    imgLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imgLabel);

    auto* textLabel = new QLabel(QStringLiteral("%1 \u00b7 %2").arg(character, fortune));
    textLabel->setAlignment(Qt::AlignCenter);
    QFont tf = textLabel->font();
    tf.setPointSize(12);
    tf.setBold(true);
    textLabel->setFont(tf);
    layout->addWidget(textLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    dlg.exec();
}

void PetWindow::onResetPosition() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();
    m_pos.setX((avail.width()  - m_petSize) / 2);
    m_pos.setY((avail.height() - m_petSize) / 2);
    m_vx = m_vy = 0.0;
    hideBubble();
    syncWindowGeometry();
    saveSettings();
}

void PetWindow::onOpenSettings() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("DeskPet \u8bbe\u7f6e"));
    dlg.setWindowFlags(dlg.windowFlags() | Qt::WindowStaysOnTopHint);
    dlg.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&dlg);
    auto* form   = new QFormLayout;

    // ---- 大小 ----
    auto* sizeSlider = new QSlider(Qt::Horizontal);
    sizeSlider->setRange(100, 400);
    sizeSlider->setValue(m_petSize);
    auto* sizeSpin = new QSpinBox;
    sizeSpin->setRange(100, 400);
    sizeSpin->setValue(m_petSize);
    sizeSpin->setSuffix(QStringLiteral(" px"));
    connect(sizeSlider, &QSlider::valueChanged, sizeSpin, &QSpinBox::setValue);
    connect(sizeSpin, qOverload<int>(&QSpinBox::valueChanged), sizeSlider, &QSlider::setValue);
    auto* sizeBox = new QHBoxLayout;
    sizeBox->addWidget(sizeSlider);
    sizeBox->addWidget(sizeSpin);
    form->addRow(QString::fromUtf8("\u5927\u5c0f"), sizeBox);

    // ---- 行走速度 ----
    auto* speedSlider = new QSlider(Qt::Horizontal);
    speedSlider->setRange((int)WALK_SPEED_MIN, (int)WALK_SPEED_MAX);
    speedSlider->setValue((int)m_walkSpeed);
    auto* speedSpin = new QSpinBox;
    speedSpin->setRange((int)WALK_SPEED_MIN, (int)WALK_SPEED_MAX);
    speedSpin->setValue((int)m_walkSpeed);
    speedSpin->setSuffix(QStringLiteral(" px/s"));
    connect(speedSlider, &QSlider::valueChanged, speedSpin, &QSpinBox::setValue);
    connect(speedSpin, qOverload<int>(&QSpinBox::valueChanged), speedSlider, &QSlider::setValue);
    auto* speedBox = new QHBoxLayout;
    speedBox->addWidget(speedSlider);
    speedBox->addWidget(speedSpin);
    form->addRow(QString::fromUtf8("\u884c\u8d70\u901f\u5ea6"), speedBox);

    // ---- 透明度 ----
    auto* opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(20, 100);
    opacitySlider->setValue(qRound(m_opacity * 100));
    auto* opacitySpin = new QSpinBox;
    opacitySpin->setRange(20, 100);
    opacitySpin->setValue(qRound(m_opacity * 100));
    opacitySpin->setSuffix(QStringLiteral("%"));
    connect(opacitySlider, &QSlider::valueChanged, opacitySpin, &QSpinBox::setValue);
    connect(opacitySpin, qOverload<int>(&QSpinBox::valueChanged), opacitySlider, &QSlider::setValue);
    auto* opacityBox = new QHBoxLayout;
    opacityBox->addWidget(opacitySlider);
    opacityBox->addWidget(opacitySpin);
    form->addRow(QString::fromUtf8("\u900f\u660e\u5ea6"), opacityBox);

    layout->addLayout(form);

    // ---- 开关 ----
    auto* walkChk  = new QCheckBox(QString::fromUtf8("\u81ea\u52a8\u884c\u8d70"));
    walkChk->setChecked(m_walkEnabled);
    auto* gravChk  = new QCheckBox(QString::fromUtf8("\u7269\u7406\u91cd\u529b"));
    gravChk->setChecked(m_gravityEnabled);
    auto* topChk   = new QCheckBox(QString::fromUtf8("\u7a97\u53e3\u7f6e\u9876"));
    topChk->setChecked(m_onTop);
    auto* ghostChk = new QCheckBox(QString::fromUtf8("\u70b9\u51fb\u7a7f\u900f (\u5e7d\u7075)"));
    ghostChk->setChecked(m_clickThrough);
    auto* soundChk = new QCheckBox(QString::fromUtf8("\u97f3\u6548"));
    soundChk->setChecked(m_soundEnabled);
    layout->addWidget(walkChk);
    layout->addWidget(gravChk);
    layout->addWidget(topChk);
    layout->addWidget(ghostChk);
    layout->addWidget(soundChk);

    // ---- 好感度 ----
    auto* affBar = new QProgressBar;
    affBar->setRange(0, AFFECTION_MAX);
    affBar->setValue(m_affection);
    affBar->setFormat(QStringLiteral("\u2665 \u597d\u611f\u5ea6 %v/%m"));
    layout->addWidget(affBar);

    // ---- 按钮 ----
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    // ---- 应用设置 ----
    QPoint center = m_pos + QPoint(m_petSize / 2, m_petSize / 2);
    m_petSize        = sizeSpin->value();
    m_walkSpeed      = speedSpin->value();
    m_opacity        = opacitySpin->value() / 100.0;
    m_walkEnabled    = walkChk->isChecked();
    m_gravityEnabled = gravChk->isChecked();
    m_onTop          = topChk->isChecked();
    m_clickThrough   = ghostChk->isChecked();
    m_soundEnabled   = soundChk->isChecked();

    m_pos = center - QPoint(m_petSize / 2, m_petSize / 2);
    hideBubble();
    clampToScreen();
    syncWindowGeometry();
    applyOpacity();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_onTop);
    setWindowFlag(Qt::WindowTransparentForInput, m_clickThrough);
    show();
    if (!m_walkEnabled) {
        m_state = PetState::IDLE;
        m_vx = m_vy = 0.0;
    }
    saveSettings();
    update();
}

void PetWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick) {
        // 安全找回: 若处于点击穿透, 单击/双击托盘先关闭穿透并显示宠物
        if (m_clickThrough) {
            onToggleClickThrough(false);
            show();
            raise();
        } else {
            onTrayShowHide();
        }
    }
}

void PetWindow::onTrayShowHide() {
    if (isVisible()) {
        hide();
    } else {
        show();
        raise();
        activateWindow();
    }
}

// ============================================================
// 系统托盘
// ============================================================

void PetWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    updateTrayIcon();
    updateTrayTooltip();

    // ---- 右下角控制面板: 完整功能托盘菜单 ----
    QMenu* trayMenu = new QMenu(this);

    QAction* showHide = trayMenu->addAction(QString::fromUtf8("\u663e\u793a/\u9690\u85cf"));
    connect(showHide, &QAction::triggered, this, &PetWindow::onTrayShowHide);

    trayMenu->addSeparator();

    m_trayWalk = trayMenu->addAction(QString::fromUtf8("\u81ea\u52a8\u884c\u8d70"));
    m_trayWalk->setCheckable(true);
    m_trayWalk->setChecked(m_walkEnabled);
    connect(m_trayWalk, &QAction::toggled, this, &PetWindow::onToggleWalk);

    m_trayGravity = trayMenu->addAction(QString::fromUtf8("\u7269\u7406\u91cd\u529b"));
    m_trayGravity->setCheckable(true);
    m_trayGravity->setChecked(m_gravityEnabled);
    connect(m_trayGravity, &QAction::toggled, this, &PetWindow::onToggleGravity);

    m_trayOnTop = trayMenu->addAction(QString::fromUtf8("\u7a97\u53e3\u7f6e\u9876"));
    m_trayOnTop->setCheckable(true);
    m_trayOnTop->setChecked(m_onTop);
    connect(m_trayOnTop, &QAction::toggled, this, &PetWindow::onToggleOnTop);

    m_trayGhost = trayMenu->addAction(QString::fromUtf8("\u70b9\u51fb\u7a7f\u900f (\u5e7d\u7075)"));
    m_trayGhost->setCheckable(true);
    m_trayGhost->setChecked(m_clickThrough);
    connect(m_trayGhost, &QAction::toggled, this, &PetWindow::onToggleClickThrough);

    m_traySound = trayMenu->addAction(QString::fromUtf8("\u97f3\u6548"));
    m_traySound->setCheckable(true);
    m_traySound->setChecked(m_soundEnabled);
    connect(m_traySound, &QAction::toggled, this, &PetWindow::onToggleSound);

    trayMenu->addSeparator();

    QAction* quit = trayMenu->addAction(QString::fromUtf8("\u9000\u51fa"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
    m_tray->setContextMenu(trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated,
            this, &PetWindow::onTrayActivated);

    m_tray->show();
}

void PetWindow::updateTrayTooltip() {
    if (!m_tray) return;
    if (m_clickThrough) {
        m_tray->setToolTip(QStringLiteral(
            "DeskPet - \u53e4\u660e\u5730\u604b\n"
            "\u70b9\u51fb\u7a7f\u900f\u5df2\u5f00\u542f\n"
            "\u53f3\u952e\u6216\u5355\u51fb\u6258\u76d8\u56fe\u6807\u53ef\u5173\u95ed"));
    } else {
        m_tray->setToolTip(QStringLiteral("DeskPet - \u53e4\u660e\u5730\u604b"));
    }
}

void PetWindow::syncTrayChecks() {
    if (m_trayWalk)    { QSignalBlocker b(m_trayWalk);    m_trayWalk->setChecked(m_walkEnabled); }
    if (m_trayGravity) { QSignalBlocker b(m_trayGravity); m_trayGravity->setChecked(m_gravityEnabled); }
    if (m_trayOnTop)   { QSignalBlocker b(m_trayOnTop);   m_trayOnTop->setChecked(m_onTop); }
    if (m_trayGhost)   { QSignalBlocker b(m_trayGhost);   m_trayGhost->setChecked(m_clickThrough); }
    if (m_traySound)   { QSignalBlocker b(m_traySound);   m_traySound->setChecked(m_soundEnabled); }
}

void PetWindow::updateTrayIcon() {
    if (!m_tray) return;

    // 优先使用自定义托盘图标 (assets/tray_icon.png), 缺失则回退到待机精灵
    QPixmap iconPm(QStringLiteral("assets/tray_icon.png"));
    if (iconPm.isNull() && m_sfIdle.isValid()) {
        iconPm = m_sfIdle.currentFrame();
    }
    if (iconPm.isNull()) {
        iconPm = QPixmap(64, 64);
        iconPm.fill(Qt::transparent);
        QPainter p(&iconPm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(QColor(120, 200, 120));
        p.setPen(Qt::NoPen);
        p.drawEllipse(8, 8, 48, 48);
        p.end();
    }
    m_tray->setIcon(QIcon(iconPm.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void PetWindow::loadSprites(const QString& baseDir) {
    const QString def = QStringLiteral("assets/");

    // 单个精灵加载: 皮肤目录缺失时回退到默认素材
    auto load = [&](const QString& name) {
        SpriteFrame f = loadSpriteByBaseName(baseDir + name);
        if (!f.isValid() && baseDir != def) f = loadSpriteByBaseName(def + name);
        return f;
    };

    m_sfIdle   = load(QStringLiteral("sprite_idle"));
    m_sfBlink  = load(QStringLiteral("sprite_blink"));
    m_sfWalk1  = load(QStringLiteral("sprite_walk1"));
    m_sfWalk2  = load(QStringLiteral("sprite_walk2"));
    m_sfSit    = load(QStringLiteral("sprite_sit"));
    m_sfStand  = load(QStringLiteral("stand"));
    m_sfCreep  = load(QStringLiteral("creep"));

    m_triggers = loadTriggerFrames(baseDir + QStringLiteral("triggers"));
    if (m_triggers.isEmpty() && baseDir != def) {
        m_triggers = loadTriggerFrames(def + QStringLiteral("triggers"));
    }
}

QStringList PetWindow::availableSkins() const {
    QStringList skins;
    QDir dir(QStringLiteral("assets/skins"));
    if (!dir.exists()) return skins;
    const QFileInfoList subs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : subs) {
        QDir d(fi.absoluteFilePath());
        if (!d.entryList(QStringList{ QStringLiteral("*.png"), QStringLiteral("*.gif") },
                         QDir::Files).isEmpty()) {
            skins << fi.fileName();
        }
    }
    skins.sort();
    return skins;
}

void PetWindow::onSkinAction() {
    QAction* act = qobject_cast<QAction*>(sender());
    if (!act) return;
    QString name = act->data().toString();

    m_skin = name;
    loadSprites(name.isEmpty()
                    ? QStringLiteral("assets/")
                    : QStringLiteral("assets/skins/") + name + QStringLiteral("/"));

    updateTrayIcon();
    if (m_mode == PetMode::GIF_MODE) {
        m_sfStand.startGif();
        m_sfCreep.startGif();
    }
    saveSettings();
    update();
}

// ============================================================
// 对话气泡 & 好感度
// ============================================================

QString PetWindow::getAffectionText() const {
    QString dialogue = affectionDialogue(m_affection);
    if (!dialogue.isEmpty()) {
        return dialogue;
    }
    return QString::fromUtf8("\u2665 \u597d\u611f\u5ea6: ") + QString::number(m_affection) + QStringLiteral(" / 100");
}

QString PetWindow::statsText() const {
    qint64 secs = m_playTimeMs / 1000;
    qint64 h    = secs / 3600;
    qint64 mi   = (secs % 3600) / 60;
    QString timeStr = (h > 0)
        ? QStringLiteral("%1\u5c0f\u65f6%2\u5206").arg(h).arg(mi)
        : QStringLiteral("%1\u5206%2\u79d2").arg(mi).arg(secs % 60);
    return QStringLiteral("\u2665 \u597d\u611f\u5ea6: %1 / 100\n"
                          "\u70b9\u51fb\u6b21\u6570: %2\n"
                          "\u629a\u6478\u6b21\u6570: %3\n"
                          "\u7279\u6548\u6b21\u6570: %4\n"
                          "\u966a\u4f34\u65f6\u957f: %5")
        .arg(m_affection)
        .arg(m_totalClicks)
        .arg(m_totalPets)
        .arg(m_totalTriggers)
        .arg(timeStr);
}

void PetWindow::checkMilestones() {
    static const qint64 thresholds[] = {10, 50, 100, 500, 1000, 5000};
    for (qint64 t : thresholds) {
        if (m_totalClicks >= t && m_clickMilestone < t) {
            m_clickMilestone = t;
            showBubble(achievementDialogue(t));
            spawnHearts(4);
            spawnSparkles(4);
            playBeep();
        }
        if (m_totalPets >= t && m_petMilestone < t) {
            m_petMilestone = t;
            showBubble(achievementDialogue(t));
            spawnHearts(4);
            spawnSparkles(4);
            playBeep();
        }
    }
}

void PetWindow::playBeep() {
    if (m_soundEnabled) QApplication::beep();
}

void PetWindow::calcBubbleSize() {
    QFont font(QStringLiteral("Microsoft YaHei"), 11);
    QFontMetrics fm(font);

    // 限制在宠物宽度内, 长文本自动换行
    int maxW = m_petSize - 12;
    int padX = 12, padY = 10;

    QRect textRect = fm.boundingRect(QRect(0, 0, maxW - padX * 2, 2000),
                                     Qt::AlignLeft | Qt::TextWordWrap,
                                     m_bubbleText);
    m_bubbleW = qMin(textRect.width() + padX * 2, maxW);
    m_bubbleH = textRect.height() + padY * 2;
    m_bubbleW = qMax(m_bubbleW, 60);
}

void PetWindow::showBubble(const QString& text) {
    m_bubbleText = text;
    m_bubbleShowTime = QDateTime::currentMSecsSinceEpoch();
    m_showBubble = true;
    calcBubbleSize();
    syncWindowGeometry();
}

void PetWindow::hideBubble() {
    m_showBubble  = false;
    m_sleepBubble = false;
    m_bubbleText.clear();
    syncWindowGeometry();
}

void PetWindow::syncWindowGeometry() {
    if (m_showBubble && m_bubbleH > 0) {
        int extra = m_bubbleH + BUBBLE_GAP;
        QScreen* s = currentScreen();
        QRect avail = s ? s->availableGeometry() : QRect();
        bool roomBelow = s && ((m_pos.y() + m_petSize + extra) <= avail.bottom());
        m_bubbleUp = !roomBelow;

        if (m_bubbleUp) {
            int winY = m_pos.y() - extra;
            if (s && winY < avail.top()) winY = avail.top();
            m_petDrawY = m_pos.y() - winY;
            setFixedSize(m_petSize, m_petSize + extra);
            move(m_pos.x(), winY);
        } else {
            m_petDrawY = 0;
            setFixedSize(m_petSize, m_petSize + extra);
            move(m_pos);
        }
    } else {
        m_bubbleUp = false;
        m_petDrawY = 0;
        setFixedSize(m_petSize, m_petSize);
        move(m_pos);
    }
}

void PetWindow::drawShadow(QPainter& painter, int centerY, int bob) {
    Q_UNUSED(bob);

    int cx = m_petSize / 2;
    qreal rw = m_petSize * 0.34;
    qreal rh = m_petSize * 0.06;

    // 阴影随呼吸浮动轻微缩放
    qreal breath = 1.0;
    if (bob != 0) breath = 1.0 - bob * 0.03;

    QRadialGradient g(cx, centerY, rw * breath);
    g.setColorAt(0.0, QColor(0, 0, 0, 70));
    g.setColorAt(0.7, QColor(0, 0, 0, 30));
    g.setColorAt(1.0, QColor(0, 0, 0, 0));

    painter.setPen(Qt::NoPen);
    painter.setBrush(g);
    painter.drawEllipse(QPointF(cx, centerY), rw * breath, rh * breath);
}

void PetWindow::spawnHearts(int count) {
    for (int i = 0; i < count; ++i) {
        HeartParticle h;
        h.x   = m_petSize * (0.15 + QRandomGenerator::global()->generateDouble() * 0.7);
        h.y   = m_petDrawY + m_petSize * 0.15;
        h.vx  = randRange(-0.6, 0.6);
        h.vy  = -(0.8 + QRandomGenerator::global()->generateDouble() * 1.0);
        h.life = 30 + QRandomGenerator::global()->bounded(20);
        h.type = 0;
        m_hearts.append(h);
    }
}

void PetWindow::spawnSparkles(int count) {
    for (int i = 0; i < count; ++i) {
        HeartParticle h;
        h.x   = m_petSize * (0.1 + QRandomGenerator::global()->generateDouble() * 0.8);
        h.y   = m_petDrawY + m_petSize * (0.2 + QRandomGenerator::global()->generateDouble() * 0.5);
        h.vx  = randRange(-0.8, 0.8);
        h.vy  = -(0.6 + QRandomGenerator::global()->generateDouble() * 1.2);
        h.life = 25 + QRandomGenerator::global()->bounded(20);
        h.type = 1;
        m_hearts.append(h);
    }
}

void PetWindow::updateHearts() {
    for (int i = m_hearts.size() - 1; i >= 0; --i) {
        HeartParticle& h = m_hearts[i];
        h.age++;
        h.y += h.vy;
        h.x += h.vx + qSin(h.age * 0.25) * 0.5;
        if (h.age >= h.life) m_hearts.removeAt(i);
    }
}

void PetWindow::drawHearts(QPainter& painter) {
    if (m_hearts.isEmpty()) return;
    painter.save();
    QFont f(QStringLiteral("Segoe UI Symbol"), qMax(10, m_petSize / 8));
    painter.setFont(f);
    for (const HeartParticle& h : m_hearts) {
        int alpha = qBound(0, qRound(255 * (1.0 - double(h.age) / h.life)), 255);
        if (h.type == 1) {
            painter.setPen(QColor(255, 210, 90, alpha));
            painter.drawText(QPointF(h.x, h.y), QStringLiteral("\u2726"));
        } else {
            painter.setPen(QColor(255, 80, 120, alpha));
            painter.drawText(QPointF(h.x, h.y), QStringLiteral("\u2665"));
        }
    }
    painter.restore();
}

void PetWindow::drawBubble(QPainter& painter) {
    if (m_bubbleText.isEmpty() || m_bubbleW <= 0) return;

    painter.save();

    int padX = 12, padY = 10;
    int gap  = BUBBLE_GAP;

    // ---- 气泡位置: 默认在宠物下方, 若空间不足则在上方 ----
    int bx = (m_petSize - m_bubbleW) / 2;
    int by;
    if (m_bubbleUp) {
        by = 0;
    } else {
        by = m_petSize + gap;
    }

    // ---- 背景 ----
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawRoundedRect(bx, by, m_bubbleW, m_bubbleH, 10, 10);

    // ---- 边框 ----
    painter.setPen(QPen(QColor(200, 180, 220), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bx, by, m_bubbleW, m_bubbleH, 10, 10);

    // ---- 文本 ----
    QFont font(QStringLiteral("Microsoft YaHei"), 11);
    painter.setFont(font);
    painter.setPen(QColor(80, 40, 80));
    painter.drawText(bx + padX, by + padY, m_bubbleW - padX * 2,
                     m_bubbleH - padY * 2,
                     Qt::AlignLeft | Qt::TextWordWrap, m_bubbleText);

    // ---- 三角形指针 (指向宠物) ----
    int cx = m_petSize / 2;
    QPointF pts[3];
    if (m_bubbleUp) {
        // 气泡在上方, 指针朝下
        pts[0] = QPointF(cx, by + m_bubbleH);
        pts[1] = QPointF(cx - 5, by + m_bubbleH + 6);
        pts[2] = QPointF(cx + 5, by + m_bubbleH + 6);
    } else {
        // 气泡在下方, 指针朝上
        pts[0] = QPointF(cx, by);
        pts[1] = QPointF(cx - 5, by - 6);
        pts[2] = QPointF(cx + 5, by - 6);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawPolygon(pts, 3);

    painter.restore();
}

// ============================================================
// 睡眠 & 触发
// ============================================================

void PetWindow::enterSleep() {
    m_sleeping = true;
    m_state = PetState::SLEEPING;
    m_vx = m_vy = 0.0;
    showBubble(QStringLiteral("Zzz\u2026"));
    m_sleepBubble = true;
}

void PetWindow::wakeUp() {
    m_lastInteraction = QDateTime::currentMSecsSinceEpoch();
    if (m_sleeping) {
        m_sleeping = false;
        m_sleepBubble = false;
        m_state = PetState::IDLE;
        hideBubble();
    }
}

void PetWindow::playRandomTrigger() {
    if (m_triggers.isEmpty()) return;

    wakeUp();
    m_lastInteraction = QDateTime::currentMSecsSinceEpoch();

    int n = m_triggers.size();
    int idx = QRandomGenerator::global()->bounded(n);
    auto it = m_triggers.constBegin();
    for (int i = 0; i < idx; ++i) ++it;

    m_prevState         = m_state;
    m_state             = PetState::TRIGGER;
    m_activeTrigger     = it.key();
    m_triggerStartTime  = QDateTime::currentMSecsSinceEpoch();
    m_vx = m_vy = 0.0;
    m_totalTriggers++;
    spawnHearts(5);
    playBeep();
}

// ============================================================
// 设置持久化
// ============================================================

void PetWindow::loadSettings() {
    QSettings s(QStringLiteral("DeskPetQt"), QStringLiteral("KoishiPet"));

    m_pos            = s.value(QStringLiteral("pos"), m_pos).toPoint();
    m_petSize        = s.value(QStringLiteral("size"), PET_SIZE).toInt();
    m_mode           = (PetMode)s.value(QStringLiteral("mode"), (int)PetMode::SPRITE_MODE).toInt();
    m_affection      = s.value(QStringLiteral("affection"), 0).toInt();
    m_opacity        = s.value(QStringLiteral("opacity"), 1.0).toDouble();
    m_walkSpeed      = s.value(QStringLiteral("walkSpeed"), 100).toInt();
    m_walkEnabled    = s.value(QStringLiteral("walkEnabled"), true).toBool();
    m_gravityEnabled = s.value(QStringLiteral("gravityEnabled"), true).toBool();
    m_onTop          = s.value(QStringLiteral("onTop"), true).toBool();
    m_clickThrough   = s.value(QStringLiteral("clickThrough"), false).toBool();
    m_soundEnabled   = s.value(QStringLiteral("soundEnabled"), false).toBool();
    QString tintName = s.value(QStringLiteral("tint"), QString()).toString();
    m_tint = tintName.isEmpty() ? QColor() : QColor(tintName);

    m_totalClicks   = s.value(QStringLiteral("clicks"), 0).toLongLong();
    m_totalPets     = s.value(QStringLiteral("pets"), 0).toLongLong();
    m_totalTriggers = s.value(QStringLiteral("triggers"), 0).toLongLong();
    m_playTimeMs    = s.value(QStringLiteral("playTimeMs"), 0).toLongLong();
    m_skin          = s.value(QStringLiteral("skin"), QString()).toString();

    m_petSize   = qBound(100, m_petSize, 400);
    m_affection = qBound(0, m_affection, AFFECTION_MAX);
    m_opacity   = qBound(0.2, m_opacity, 1.0);
}

void PetWindow::saveSettings() {
    QSettings s(QStringLiteral("DeskPetQt"), QStringLiteral("KoishiPet"));

    // 累积本次运行时长
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_playStart > 0) {
        m_playTimeMs += (now - m_playStart);
        m_playStart = now;
    }

    s.setValue(QStringLiteral("pos"), m_pos);
    s.setValue(QStringLiteral("size"), m_petSize);
    s.setValue(QStringLiteral("mode"), (int)m_mode);
    s.setValue(QStringLiteral("affection"), m_affection);
    s.setValue(QStringLiteral("opacity"), m_opacity);
    s.setValue(QStringLiteral("walkSpeed"), qRound(m_walkSpeed));
    s.setValue(QStringLiteral("walkEnabled"), m_walkEnabled);
    s.setValue(QStringLiteral("gravityEnabled"), m_gravityEnabled);
    s.setValue(QStringLiteral("onTop"), m_onTop);
    s.setValue(QStringLiteral("clickThrough"), m_clickThrough);
    s.setValue(QStringLiteral("soundEnabled"), m_soundEnabled);
    s.setValue(QStringLiteral("tint"), m_tint.isValid() ? m_tint.name() : QString());
    s.setValue(QStringLiteral("clicks"), m_totalClicks);
    s.setValue(QStringLiteral("pets"), m_totalPets);
    s.setValue(QStringLiteral("triggers"), m_totalTriggers);
    s.setValue(QStringLiteral("playTimeMs"), m_playTimeMs);
    s.setValue(QStringLiteral("skin"), m_skin);
}

void PetWindow::applyOpacity() {
    // 透明度在 paintEvent 里通过 QPainter::setOpacity 实现, 这里只需触发重绘。
    update();
}

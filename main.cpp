/**
 * DeskPet Qt - 程序入口
 *
 * 桌面 koishi Qt 版
 * 编译: cmake -B build && cmake --build build
 *
 * 一个可爱的koishi桌宠，
 * 基于 Qt6 Widgets，透明置顶无边框窗口
 */

#include <QApplication>
#include "petwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("DeskPet Qt");
    app.setApplicationVersion("1.0");
    app.setQuitOnLastWindowClosed(false);  // 右键菜单退出

    PetWindow pet;
    // pet 窗口保持显示直到用户退出

    return app.exec();
}

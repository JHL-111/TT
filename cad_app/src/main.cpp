/**
 * @file main.cpp
 * @brief 主程序入口 
 * 负责初始化所有必要的组件，
 * 设置应用程序属性，创建主窗口，然后把控制权交给Qt的事件循环。
 * 
 */

#include <QApplication>          // Qt应用程序的"心脏"
#include <QMessageBox>           // 消息对话框 
#include <QStyleFactory>         // 样式工厂 
#include <QDir>                  // 目录操作
#include <QStandardPaths>        // 标准路径 
#include <QSettings>             // 设置管理
#include <QSplashScreen>         // 启动画面 
#include <QPixmap>               // 像素图 
#include <QTimer>                // 定时器

#include "cad_ui/MainWindow.h"   // 主窗口 

#include "cad_sketch/ConstraintSolverTest.h"

// QRC资源初始化函数声明 - 手动初始化静态库中的资源
extern int qInitResources_resources();

// OpenCASCADE的初始化相关头文件 - 让几何计算引擎苏醒
#include <Standard_Version.hxx>      // 版本信息 - 知己知彼
#include <Message.hxx>               // 消息系统 - OpenCASCADE的"嘴巴"
#include <Message_PrinterOStream.hxx> // 输出流打印器 - 把消息送到控制台

/**
 * 主函数 - 程序的"总指挥官"
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序退出码，0表示成功，其他值表示出错了
 */
int main(int argc, char *argv[])
{
    // 创建Qt应用程序对象
    QApplication app(argc, argv);
    
    // 手动初始化QRC资源,确保静态库中的资源能被正确加载
    qInitResources_resources();
    
    // 设置应用程序属性 
    app.setApplicationName("JLi CAD");       
    app.setApplicationVersion("1.0.0");        
    app.setOrganizationName("JLiCAD");       

    
    // 初始化OpenCASCADE消息系统 - 让几何引擎能够"说话"
    // 这样我们就能知道OpenCASCADE在干什么，出了什么问题
    Handle(Message_PrinterOStream) printer = new Message_PrinterOStream();
    Message::DefaultMessenger()->AddPrinter(printer);
    
    // 创建启动画面
    QSplashScreen* splash = nullptr;

    
    // 创建主窗口
    cad_ui::MainWindow mainWindow;
    
    // 初始化主窗口
    if (!mainWindow.Initialize()) {
      
        return -1;  
    }
    
    // 加载用户设置
    QSettings settings;
    if (settings.contains("geometry")) {
        // 恢复窗口大小和位置 
        mainWindow.restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        // 恢复窗口状态（最大化、停靠面板位置等）
        mainWindow.restoreState(settings.value("windowState").toByteArray());
    }
    
    // 应用主题 
    QString theme = settings.value("theme", "light").toString();  // 默认浅色主题
    mainWindow.SetTheme(theme);
    
    //RunConstraintTests();

    // 显示主窗口 
    mainWindow.show();
    
    // 关闭启动画面（如果存在的话）
    if (splash) {
        splash->finish(&mainWindow); 
        delete splash;          
    }
    
    
    // 运行应用程序 
    int result = app.exec();
    
    // 退出前保存设置 
    QSettings saveSettings;
    saveSettings.setValue("geometry", mainWindow.saveGeometry());      // 保存窗口大小位置
    saveSettings.setValue("windowState", mainWindow.saveState());      // 保存窗口状态
  

    // 返回程序退出码
    return result;
}
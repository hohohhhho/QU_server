#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QMutex>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QByteArray readSocket(QTcpSocket* socket);//解析大小头，防止粘包，返回除了大小头外的内容
    void readMsg(const QByteArray &msg, QTcpSocket *socket=nullptr);//解析并处理消息
    void sendMsg(int id_receiver, const QByteArray &content, const char &style_head);
    void sendMsg(QTcpSocket* socket, const QByteArray &content, const char &style_head);

    void sqlQuery(QTcpSocket *socket, QByteArray data);//直接读取除了协议大小头和种类头外的部分

private:
    struct USER{
        int id;//用户id
        QTcpSocket* socket;//用户通信套接字
    };
    Ui::MainWindow *ui;
    QTcpServer* server;

    QMutex mutex_user_list;
    QList<USER*> list_user;


};
#endif // MAINWINDOW_H

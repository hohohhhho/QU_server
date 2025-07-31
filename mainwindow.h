#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
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

    struct GroupMember{
        GroupMember();
        GroupMember(int id,int role):id(id),role(role){};
        // bool operator==(const MainWindow::GroupMember& other)const{
        //     return this->id == other.id;
        // }
        int id;//用户id
        int role = 0;//用户在群聊中的身份
    };

    QByteArray readSocket(QTcpSocket* socket);//解析大小头，防止粘包，返回除了大小头外的内容
    void readMsg(const QByteArray &msg, QTcpSocket *socket=nullptr);//解析并处理消息
    void readUdpMsg(QByteArray msg, QHostAddress address, quint16 port);//解析udp报文
    void sendMsg(int id_receiver, const QByteArray &content, const char &style_head);
    void sendMsg(QTcpSocket* socket, const QByteArray &content, const char &style_head);

    void sqlQuery(QTcpSocket *socket, QByteArray data);//直接读取除了协议大小头和种类头外的部分

    QString handleDataHead(QByteArray &data, const QChar &split='*');

private:
    struct UdpInfo{
        QHostAddress ip;
        quint16 port;
    };

    Ui::MainWindow *ui;
    QTcpServer* server;
    QUdpSocket* socket_udp_server;

    QMutex mutex_tcp_map;
    QHash<int,QTcpSocket*> map_to_tcp;//用户id到tcp套接字的映射，根据该映射分发信息
    QMutex mutex_udp_map;
    QHash<int,UdpInfo> map_to_udp;//用户id到udp套接字的映射，根据该映射分发信息
    QMutex mutex_members_map;
    QHash<int,QList<GroupMember>> map_to_members;

};

inline bool operator==(const MainWindow::GroupMember& m1,const MainWindow::GroupMember& m2){
    return m1.id == m2.id;
}
#endif // MAINWINDOW_H

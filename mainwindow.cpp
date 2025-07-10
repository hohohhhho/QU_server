#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("服务器1.2");
    this->server=new QTcpServer(this);
    server->listen(QHostAddress::Any,8899);

    connect(server,&QTcpServer::newConnection,this,[=](){
        QTcpSocket* socket=server->nextPendingConnection();

        connect(socket,&QTcpSocket::readyRead,this,[=](){
            QByteArray data=readSocket(socket);
            readMsg(data,socket);
        });

    });
    {
        QSqlDatabase db=QSqlDatabase::addDatabase("QMYSQL","load_data");
        db.setDatabaseName("work5_qq");
        db.setHostName("127.0.0.1");
        db.setUserName("testuser");
        db.setPassword("222222");
        if(db.open()){
            qDebug()<<"数据库打开成功";
            QSqlQuery query(db);
            if(query.exec("select id from account;")){
                while(query.next()){
                    int id=query.value(0).toInt();
                    USER* user=new USER;
                    user->id=id;
                    user->socket=nullptr;
                    this->list_user.append(user);
                }
            }else{
                qWarning()<<"query1执行失败"<<query.lastError().text();
            }
        }else{
            qWarning()<<"数据库打开失败"<<db.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase("load_data");

    QSqlDatabase db_r=QSqlDatabase::addDatabase("QMYSQL","register");
    db_r.setDatabaseName("work5_qq");
    db_r.setHostName("127.0.0.1");
    db_r.setUserName("testuser");
    db_r.setPassword("222222");
    QSqlDatabase db_q=QSqlDatabase::addDatabase("QMYSQL","query");
    db_q.setDatabaseName("work5_qq");
    db_q.setHostName("127.0.0.1");
    db_q.setUserName("testuser");
    db_q.setPassword("222222");
}

MainWindow::~MainWindow()
{
    QSqlDatabase::removeDatabase("register");
    qDeleteAll(list_user);
    delete ui;
}

QByteArray MainWindow::readSocket(QTcpSocket *socket)
{
    // static QHash<QTcpSocket*, quint32> expectedSizes;  // 保存待接收数据长度

    // QDataStream in(socket);
    // in.setVersion(QDataStream::Qt_6_8);  // 必须与发送方一致

    // if (!expectedSizes.contains(socket)) {
    //     if (socket->bytesAvailable() < static_cast<qint32>(sizeof(quint32)))
    //         return QByteArray();  // 数据不足，等待下次

    //     in >> expectedSizes[socket];  // 读取8字节长度头
    // }

    // // 阶段2：检查数据体是否完整
    // if (socket->bytesAvailable() < static_cast<qint32>(expectedSizes[socket]))
    //     return QByteArray();  // 数据不足，等待下次

    // // 阶段3：读取实际内容（不含长度头）
    // QByteArray content;
    // in>>content;
    // // 清理状态
    // expectedSizes.remove(socket);

    // return content;  // 返回纯内容
    // static QHash<QTcpSocket*, quint32> expectedSizes;
    static QHash<QTcpSocket*,QByteArray> buffers;

    if(buffers[socket].isEmpty()){
        buffers[socket]=QByteArray();
    }
    buffers[socket]+=socket->readAll();
    QDataStream in(&buffers[socket],QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_8);
    in.startTransaction();

    QByteArray content;
    in>>content;
    if(in.commitTransaction()){
        buffers[socket]=buffers[socket].mid(in.device()->pos());
        // in.device()->seek(0);
    }else{
        in.rollbackTransaction();
    }

    // expectedSizes.remove(socket);

    return content;

}

void MainWindow::readMsg(const QByteArray &msg,QTcpSocket* socket)
{
    // QString fmsg(msg);
    qDebug()<<"接收到信息:msg"<<msg;
    QByteArray data(msg);//格式：/消息类型*接收方id**发送方id*消息主体
    if(data.isEmpty()){
        return;
    }

    char first_char=data[0];
    if(first_char=='/'){
        char second_char=data[1];
        data=data.mid(2);//移除/s/a等
        if(second_char=='s' || second_char=='a' || second_char=='f' || second_char=='d'){//发送消息send,添加好友add,被接受好友申请（直接添加）friend,被删除好友delete


            QString num;
            int counter=0;
            int id_sender;
            int id_receiver;

            data=data.mid(1);//移除"*"
            for(char& c:data){
                if(c=='*'){
                    data=data.mid(counter+1);//移除所有参与了计数的字符再移除“*”
                    break;
                }else{
                    counter++;
                    num.append(c);
                }
            }
            id_receiver=num.toInt();

            num="";
            counter=0;
            data=data.mid(1);//移除"*"
            for(char& c:data){
                if(c=='*'){
                    data=data.mid(counter+1);//移除所有参与了计数的字符再移除“*”
                    break;
                }else{
                    counter++;
                    num.append(c);
                }
            }
            id_sender=num.toInt();

            if(second_char=='f' || second_char=='d'){
                QSqlDatabase db=QSqlDatabase::database("query");
                if(db.open()){
                    QSqlQuery query(db);
                    QString sql1=QString("insert into friends(id_user1,id_user2) value(:id_user1,:id_user2);");
                    QString sql2=QString("delete from friends where (id_user1=:id_user1 and id_user2=:id_user2)"
                                  "or(id_user1=:id_user2 and id_user2=:id_user1);");
                    if(second_char=='f'){
                        query.prepare(sql1);
                    }else{
                        query.prepare(sql2);
                    }
                    query.bindValue(":id_user1",id_sender);
                    query.bindValue(":id_user2",id_receiver);
                    if(query.exec()){
                        qDebug()<<"好友关系同步数据库成功";
                    }else{
                        qDebug()<<"用户数据加载失败"<<query.lastError();
                    }
                }else{
                    qDebug()<<"数据库打开失败";
                }
            }

            QByteArray head=QString("*%1*").arg(id_sender).toUtf8();//发送方

            qDebug()<<"/send data"<<data;
            sendMsg(id_receiver,head.append(data),second_char);

        }else if(second_char=='r'){//注册register

            QString account;
            QString password;
            int counter=0;

            data=data.mid(1);//移除"*"
            for(char& c:data){
                if(c=='*'){
                    data=data.mid(counter+1);//移除所有参与了计数的字符再移除“*”
                    break;
                }else{
                    counter++;
                    account.append(c);
                }
            }
            counter=0;
            data=data.mid(1);//移除"*"
            for(char& c:data){
                if(c=='*'){
                    data=data.mid(counter+1);//移除所有参与了计数的字符再移除“*”
                    break;
                }else{
                    counter++;
                    password.append(c);
                }
            }

            {
                QSqlDatabase db = QSqlDatabase::database("register");
                if (db.open()) {
                    db.transaction(); // 开启事务
                    QSqlQuery query(db);

                    query.prepare("select * from account where account=:account;");
                    query.bindValue(":account", account);
                    if(query.exec() && query.next()){
                        sendMsg(socket,"账号已存在",'f');
                    }

                    query.prepare("INSERT INTO account (account, password) VALUES (:account, :password);");
                    query.bindValue(":account", account);
                    query.bindValue(":password", password);
                    if (query.exec()) {
                        // 获取自增id
                        if (query.exec("SELECT LAST_INSERT_ID();") && query.next()) {
                            int id = query.value(0).toInt();
                            db.commit(); // 提交事务

                            USER* user = new USER;
                            user->id = id;
                            user->socket = socket;

                            mutex_user_list.lock();
                            list_user.append(user);
                            mutex_user_list.unlock();

                            QByteArray content=QString("/%1").arg(id).toUtf8();
                            sendMsg(socket,content,'s');
                        } else {
                            db.rollback(); // 回滚事务
                            qWarning() << "获取自增ID失败";
                            sendMsg(socket,"获取id失败",'f');
                        }
                    } else {
                        db.rollback(); // 回滚事务
                        qWarning()<<query.lastError().text();
                        sendMsg(socket,query.lastError().text().toUtf8(),'f');

                        if (query.lastError().text() == "1062") { // 唯一约束冲突
                            qWarning()<<"账号已存在";
                        } else {
                            qWarning()<<"插入数据失败";
                        }
                    }
                } else {
                    qWarning()<<"数据库打开失败";
                    sendMsg(socket,"数据库连接失败",'f');
                }
            }

        }else if(second_char=='l'){//登录login
            data.chop(1);//移除末尾的*
            data=data.mid(1);//移除头部的*
            int id=data.toInt();
            for(USER* user:list_user){
                if(user->id==id){
                    if(user->socket!=nullptr){
                        user->socket->deleteLater();
                    }
                    user->socket=socket;
                    connect(socket,&QTcpSocket::readyRead,this,[=](){
                        QByteArray data=readSocket(socket);
                        readMsg(data,socket);
                    });
                }
            }
        }else if(second_char=='m'){//数据库查询MySQL,/m/login*para1**para2*

            sqlQuery(socket,data);

        }else if(second_char=='p'){
            int id;
            QByteArray profile;
            QDataStream in(&data,QIODevice::ReadOnly);
            in>>id>>profile;

            QSqlDatabase db= QSqlDatabase::database("query");
            if(db.open()){
                QSqlQuery query(db);
                QString sql=QString("update account set profile=:profile where id=:id;");
                query.prepare(sql);
                query.bindValue(":profile",profile);
                query.bindValue(":id",id);
                if(query.exec()){
                    sendMsg(socket,"",'s');
                }else{
                    sendMsg(socket,"",'f');
                }
            }else{
                qDebug()<<"数据库打开失败";
            }
        }else if(second_char=='e'){//错误error

        }
    }else{
        qWarning()<<"读取了意外的字符2:"<<first_char;
    }
}

void MainWindow::sendMsg(int id_receiver,const QByteArray& content,const char& style_head)
{
    bool send=false;
    for(USER* user:this->list_user){
        if(user->id==id_receiver){
            if(user->socket!=nullptr){
                send=true;
                qDebug()<<"已找到发送目标"<<id_receiver;
                sendMsg(user->socket,content,style_head);
            }else{
                //此处添加离线消息队列
            }
        }
    }
    if(!send){
        qDebug()<<"未找到已注册的接收方id"<<id_receiver;
    }
    // QByteArray data=(QString("/")+style_head).toUtf8()+content;
    // QString head=QString("*%1*").arg(data.size());//大小头
    // bool send=false;
    // for(USER* user:this->list_user){
    //     if(user->id==id_receiver){
    //         send=true;
    //         if(user->socket!=nullptr){
    //             user->socket->write( (head).toUtf8()+data );
    //             qDebug()<<"send:"<<head+data;
    //         }else{
    //             //此处添加离线消息队列
    //         }
    //     }
    // }
    // if(!send){
    //     qDebug()<<"未找到已注册的接收方id"<<id_receiver;
    // }
}

void MainWindow::sendMsg(QTcpSocket *socket, const QByteArray &content, const char &style_head)
{

    QByteArray data;//临时数据存储
    QDataStream out(&data,QIODevice::WriteOnly);//创建数据流写入data
    // out.setVersion(QDataStream::Qt_6_8);
    // out<<quint64(0)<<'/'+style_head+content;
    // out.device()->seek(0);
    // out<<out.device()->size()-sizeof(quint64);
    QByteArray packet = QByteArray("/").append(style_head).append(content);
    out<<packet;
    socket->write(data);
    qDebug()<<"send:"<<data;
}

void MainWindow::sqlQuery(QTcpSocket *socket,QByteArray data)
{
    data=data.mid(1);//去掉/
    QStringList qstr_list=QString::fromUtf8(data).split('*');
    QString type=qstr_list.first();//获取用*分割的第一位，即查询业务的类别
    QSqlDatabase db=QSqlDatabase::database("query");
    if(db.open()){
        QSqlQuery query(db);
        QString sql;
        if(type=="same_account"){
            QString account=qstr_list[1];
            sql=QString("select id from account where account=:account;");
            query.prepare(sql);
            query.bindValue(":account",&account);
            if(query.exec() && query.next()){
                qDebug()<<"数据查询成功";
                int id=query.value("id").toInt();
                sendMsg(socket,QString::number(id).toUtf8(),'s');
            }else{
                QString error=query.lastError().text();
                qDebug()<<"same_account sql语句执行失败"<<error;
                sendMsg(socket,error.toUtf8(),'f');
            }
        }else if(type=="login"){
            QString account=qstr_list[1];
            QString password=qstr_list[3];
            qDebug()<<"account"<<account<<"password"<<password;
            sql=QString("select id from account where account=:account and password=:password;");
            query.prepare(sql);
            query.bindValue(":account",account);
            query.bindValue(":password",password);
            if(query.exec() && query.next()){
                qDebug()<<"数据查询成功";
                int id=query.value("id").toInt();
                sendMsg(socket,'/'+QString::number(id).toUtf8(),'s');
            }else{
                QString error=query.lastError().text();
                qDebug()<<"login sql语句执行失败"<<error;
                sendMsg(socket,error.toUtf8(),'f');
            }
        }else if(type=="patch_user"){
            int id=qstr_list[1].toInt();
            sql=QString("select * from account where id=:id;");
            query.prepare(sql);
            query.bindValue(":id",id);

            auto errorFunc=[=](){
                QByteArray buffer;
                QDataStream out(&buffer,QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_8);
                out<<QByteArray()<<QByteArray();
                socket->write(buffer);
            };

            if(query.exec() && query.next()){

                if(query.value("id").toInt()==id){
                    QByteArray data=query.value("profile").toByteArray();

                    QByteArray nickname=query.value("nickname").toByteArray();

                    QByteArray state=query.value("state").toByteArray();

                    QByteArray likes=query.value("likes").toByteArray();

                    QByteArray content=QString("/s/%1/%2/%3").arg(nickname,state,likes).toUtf8();

                    QByteArray buffer;//临时数据存储
                    QDataStream out(&buffer,QIODevice::WriteOnly);//创建数据流写入data
                    out.setVersion(QDataStream::Qt_6_8);
                    out<<data<<content;
                    socket->write(buffer);
                    // sendMsg(socket,content,'s');
                    // qDebug()<<"用户信息完善成功 buffer"<<buffer;
                }else{
                    qDebug()<<"未找到该用户:id="<<id;
                    errorFunc();
                }
            }else{
                QString error=query.lastError().text();
                qDebug()<<"patch_user sql语句执行失败"<<error;
                // sendMsg(socket,error.toUtf8(),'f');
                errorFunc();
            }
        }else if(type=="num_friends"){
            int id=qstr_list[1].toInt();
            sql=QString("select count(*) from friends where id_user1=:id or id_user2=:id;");
            query.prepare(sql);
            query.bindValue(":id",id);
            if(query.exec() && query.next()){
                int num=query.value("count(*)").toInt();
                QByteArray content=QString("/%1").arg(num).toUtf8();
                sendMsg(socket,content,'s');
                qDebug()<<"好友数量获取成功";
            }else{
                QString error=query.lastError().text();
                qDebug()<<"num_friends sql语句执行失败"<<error;
                sendMsg(socket,error.toUtf8(),'f');
            }
        }else if(type=="load_friends"){
            int id=qstr_list[1].toInt();
            QSet<int> friends;
            sql="select id_user1 as friend_id from friends where id_user2=:id\n"
                "union\n"
                "select id_user2 as friend_id from friends where id_user1=:id;";
            query.prepare(sql);
            query.bindValue(":id",id);
            if(query.exec()){
                while(query.next()){
                    friends.insert(query.value("friend_id").toInt());
                }
                QByteArray content;
                for(const int& friend_id:friends){
                    content+=QString("/%1").arg(friend_id).toUtf8();
                }
                sendMsg(socket,content,'s');
            }else{
                QString error=query.lastError().text();
                qDebug()<<"load_friends sql语句执行失败"<<error;
                sendMsg(socket,error.toUtf8(),'f');
            }
        }else if(type=="make_friend"){
            int id_user1=qstr_list[1].toInt();
            int id_user2=qstr_list[3].toInt();
            QSqlQuery query(db);
            QString sql=QString("insert into friends(id_user1,id_user2) value(:id_user1,:id_user2);");
            query.prepare(sql);
            query.bindValue(":id_user1",id_user1);
            query.bindValue(":id_user2",id_user2);
            if(query.exec()){
                sendMsg(socket,"",'s');
                qDebug()<<"好友关系同步数据库成功";
            }else{
                sendMsg(socket,"",'f');
                qDebug()<<"用户数据加载失败"<<query.lastError();
            }
        }else if(type=="delete_friend"){
            int id_user1=qstr_list[1].toInt();
            int id_user2=qstr_list[3].toInt();
            sql=QString("delete from friends where (id_user1=:id_user1 and id_user2=:id_user2)"
                                                "or(id_user1=:id_user2 and id_user2=:id_user1);");
            query.prepare(sql);
            query.bindValue(":id_user1",id_user1);
            query.bindValue(":id_user2",id_user2);
            if(query.exec()){
                sendMsg(socket,"",'s');
                qDebug()<<"删除好友操作同步成功";
            }else{
                sendMsg(socket,"",'f');
                qDebug()<<"删除好友操作同步失败"<<query.lastError().text();
            }
        }else if(type=="update_password"){
            QString new_password=qstr_list[1];
            int id=qstr_list[3].toInt();
            sql=QString("update account set password=:password where id=:id;");
            query.prepare(sql);
            query.bindValue(":password",new_password);
            query.bindValue(":id",id);
            if(query.exec()){
                sendMsg(socket,"",'s');
            }else{
                sendMsg(socket,"",'f');
            }
        }else if(type=="update_user_info"){
            QString nickname=qstr_list[1];
            QString state=qstr_list[3];
            int id=qstr_list[5].toInt();
            QString sql=QString("update account set nickname=:nickname,state=:state where id=:id;");
            query.prepare(sql);
            query.bindValue(":id",id);
            query.bindValue(":nickname",nickname);
            query.bindValue(":state",state);
            if(query.exec()){
                sendMsg(socket,"",'s');
            }else{
                sendMsg(socket,"",'f');
            }
        }else{
            qDebug()<<"未能识别的业务类型:"<<type;
        }
    }else{
        qWarning()<<"数据库打开失败"<<db.lastError().text();
    }
    db.close();
}

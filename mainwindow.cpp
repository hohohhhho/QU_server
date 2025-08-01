#include "mainwindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSystemTrayIcon>
#include <QMenu>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("服务器1.3");
    //托盘界面
    {
        QSystemTrayIcon* tray = new QSystemTrayIcon(this);
        QMenu* menu = new QMenu(this);
        tray->show();
        QAction* action_exit = new QAction(menu);
        tray->setContextMenu(menu);
        menu->addAction(action_exit);
        connect(action_exit,&QAction::triggered,this,[=](){
            this->close();
        });
    }

    this->server=new QTcpServer(this);
    this->socket_udp_server = new QUdpSocket(this);
    server->listen(QHostAddress::Any,8899);
    socket_udp_server->bind(8899);

    connect(server,&QTcpServer::newConnection,this,[=](){
        QTcpSocket* socket=server->nextPendingConnection();

        connect(socket,&QTcpSocket::readyRead,this,[=](){
            QByteArray data=readSocket(socket);
            readMsg(data,socket);
        });

    });
    connect(socket_udp_server,&QUdpSocket::readyRead,this,[=](){
        QByteArray data;//数据
        QHostAddress address;//地址
        quint16 port;//端口
        data.resize(socket_udp_server->pendingDatagramSize());
        socket_udp_server->readDatagram(data.data(),data.size(),&address,&port);
        readUdpMsg(data,address,port);
    });
    //从数据库初始化动态表
    {
        QSqlDatabase db=QSqlDatabase::addDatabase("QMYSQL","load_data");
        db.setDatabaseName("work5_qq");
        db.setHostName("127.0.0.1");
        db.setUserName("testuser");
        db.setPassword("222222");
        if(db.open()){
            qDebug()<<"数据库打开成功";
            QSqlQuery query(db);
            QString sql("select id from account;");//获取所有注册了账号的用户id
            query.prepare(sql);
            if(query.exec()){
                while(query.next()){
                    int id = query.value(0).toInt();
                    map_to_tcp.insert(id,nullptr);
                }
            }else{
                qWarning()<<"query1执行失败"<<query.lastError().text();
            }

            QSet<int> groups;//所有群聊的id
            sql = ("select id from chatgroup;");//获取所有群聊的id
            query.prepare(sql);
            if(query.exec()){
                while(query.next()){
                    int id = query.value(0).toInt();//结果只有一列
                    groups.insert(id);
                }
                //遍历获取每个群聊内的成员，分为3个等级查询：群主、管理员、普通成员
                // sql = ("select owner as owner from chatgroup where id=:id_group "
                //        "union all "
                //        "select id_user as manage from manage_group where id_group=:id_group "
                //        "union all "
                //        "select id_user as common from join_group where id_group=:id_group;");
                QString sql_owner = "select owner    from chatgroup    where id=:id_group;";
                QString sql_manage = "select id_user from manage_group where id_group=:id_group;";
                QString sql_common = "select id_user from join_group   where id_group=:id_group;";
                for(const int& id_group : groups){
                    QList<GroupMember> list_groupMember;
                    //加载群主
                    query.prepare(sql_owner);
                    query.bindValue(":id_group",id_group);
                    query.exec();
                    while(query.next()){
                        int id_owner = query.value(0).toInt();
                        GroupMember member(id_owner,3);
                        list_groupMember.append(member);
                    }
                    //加载管理员
                    query.prepare(sql_manage);
                    query.bindValue(":id_group",id_group);
                    query.exec();
                    while(query.next()){
                        int id_manage = query.value(0).toInt();
                        GroupMember member(id_manage,2);
                        list_groupMember.append(member);
                    }
                    //加载普通群员
                    query.prepare(sql_common);
                    query.bindValue(":id_group",id_group);
                    query.exec();
                    while(query.next()){
                        int id_common = query.value(0).toInt();
                        GroupMember member(id_common,1);
                        list_groupMember.append(member);
                    }
                    map_to_members.insert(id_group,list_groupMember);
                }
            }else{
                qWarning()<<"query2执行失败"<<query.lastError().text();
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
    qDeleteAll(map_to_tcp);
}

QByteArray MainWindow::readSocket(QTcpSocket *socket)
{
    // static QHash<QTcpSocket*, quint32> expectedSizes;  // 保存待接收数据长度

    // QDataStream in(socket);
    // in.setVersion(QDataStream::Qt_5_15);  // 必须与发送方一致

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
    in.setVersion(QDataStream::Qt_5_15);
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
    if(msg.size()<1024){
        qDebug()<<"接收到信息 msg"<<msg;
    }else{
        qDebug()<<"接收到信息 msg.size()"<<msg.size();
    }

    QByteArray data(msg);//格式：/消息类型*接收方id**发送方id*消息主体
    if(data.isEmpty()){
        return;
    }

    char first_char=data[0];
    if(first_char=='/'){
        char second_char=data[1];
        data=data.mid(2);//移除/s/a等
        QList<char> general={'s','a','f','d','r','i','h','u','g'};
        /*
        发送消息send,添加好友add,被接受好友申请（直接添加）friend,被删除好友delete
        申请视频通话request,通话时传递视频帧image,被挂断hang up
        这些均有id_receiver，id_sender信息，统一处理
        */
        if(general.contains(second_char)){

            int id_sender,id_receiver;

            id_receiver = handleDataHead(data).toInt();
            id_sender   = handleDataHead(data).toInt();

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
            }else if(second_char=='g'){
                //获取收到消息的群聊的所有成员
                int id_group = id_receiver;
                auto list_member = map_to_members.value(id_group);
                for(GroupMember& member : list_member){
                    if(member.id == id_sender){
                        continue;//不发送给发送人
                    }
                    QByteArray head=QString("*%1**%2*").arg(id_sender).arg(id_group).toUtf8();
                    sendMsg(member.id,head.append(data),'g');//把该群发的消息分发给所有成员
                }
                return ;//提前返回,这个逻辑分支的id_receiver是群id，后续会将其视为用户id转发消息
            }

            QByteArray head=QString("*%1*").arg(id_sender).toUtf8();//发送方

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

                            mutex_tcp_map.lock();
                            map_to_tcp.insert(id,socket);
                            mutex_tcp_map.unlock();

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
            data = data.mid(1);//移除头部的*
            int id = data.toInt();

            auto user_socket = map_to_tcp.value(id);
            if(user_socket != nullptr){//如果已经登录过，需要更新新的套接字
                user_socket->deleteLater();
            }
            map_to_tcp.insert(id,socket);
            connect(socket,&QTcpSocket::readyRead,this,[=](){
                QByteArray data = readSocket(socket);
                readMsg(data,socket);
            });
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
        }else if(second_char=='q'){
            int id;
            QByteArray profile;
            QDataStream in(&data,QIODevice::ReadOnly);
            in>>id>>profile;

            QSqlDatabase db= QSqlDatabase::database("query");
            if(db.open()){
                QSqlQuery query(db);
                QString sql=QString("update chatgroup set profile=:profile where id=:id;");
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

void MainWindow::readUdpMsg(QByteArray msg, QHostAddress address, quint16 port)
{
    if(msg.size() < 1024){
        qDebug()<<"收到udp消息:"<<QString::fromUtf8(msg)<<"来自ip"<<address.toString()<<"端口"<<port;
    }else{
        qDebug()<<"收到udp消息，大小："<<msg.size()<<"来自ip"<<address.toString()<<"端口"<<port;
    }

    QString type = handleDataHead(msg);
    if(type == "r"){//request请求视频通话
        int id_sender,id_receiver;
        id_sender = handleDataHead(msg).toInt();
        id_receiver   = handleDataHead(msg).toInt();

        UdpInfo info;
        info.ip = address;
        info.port = port;
        map_to_udp.insert(id_sender,info);//保存该次的用户NAT至表

        msg = QString("*%1*").arg(id_sender).toUtf8() + msg;
        sendMsg(id_receiver,msg,'r');//把申请消息通过tcp转发给目标用户
    }else if(type == "i"){//video image 传输视频通话画面
        int receiver = handleDataHead(msg).toInt();
        mutex_udp_map.lock();
        const QHostAddress& ip = map_to_udp.value(receiver).ip;
        const quint16& port = map_to_udp.value(receiver).port;
        mutex_udp_map.unlock();
        qDebug()<<"传输视频画面给(ip"<<ip<<",端口"<<port<<"),大小"<<msg.size();
        socket_udp_server->writeDatagram(msg,ip,port);
    }
}

void MainWindow::sendMsg(int id_receiver,const QByteArray& content,const char& style_head)
{
    if(!map_to_tcp.contains(id_receiver)){
        qDebug()<<"未找到已注册的接收方id"<<id_receiver;
        return;
    }
    auto target_socket = map_to_tcp.value(id_receiver);
    if(target_socket!=nullptr){
        sendMsg(target_socket,content,style_head);
    }else{
        qDebug()<<"该用户离线中:id"<<id_receiver;
    }
}

void MainWindow::sendMsg(QTcpSocket *socket, const QByteArray &content, const char &style_head)
{

    QByteArray data;//临时数据存储
    QDataStream out(&data,QIODevice::WriteOnly);//创建数据流写入data
    // out.setVersion(QDataStream::Qt_5_15);
    // out<<quint64(0)<<'/'+style_head+content;
    // out.device()->seek(0);
    // out<<out.device()->size()-sizeof(quint64);
    QByteArray packet = QByteArray("/").append(style_head).append(content);
    out<<packet;
    socket->write(data);
    if(data.size()<1024){
        qDebug()<<"send:"<<data;
    }else{
        qDebug()<<"send data.size()"<<data.size();
    }
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
            query.bindValue(":account",account);
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
                out.setVersion(QDataStream::Qt_5_15);
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
                    out.setVersion(QDataStream::Qt_5_15);
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
        }else if(type=="patch_group"){
            int id=qstr_list[1].toInt();
            sql=QString("select * from chatgroup where id=:id;");
            query.prepare(sql);
            query.bindValue(":id",id);

            auto errorFunc=[=](){
                QByteArray buffer;
                QDataStream out(&buffer,QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_5_15);
                out<<QByteArray()<<QByteArray();
                socket->write(buffer);
            };

            if(query.exec() && query.next()){

                if(query.value("id").toInt()==id){
                    QByteArray data=query.value("profile").toByteArray();

                    QByteArray name=query.value("name").toByteArray();

                    QByteArray intro=query.value("intro").toByteArray();

                    QByteArray owner=query.value("owner").toByteArray();

                    QByteArray content=QString("/s/%1/%2/%3").arg(name,intro,owner).toUtf8();

                    QByteArray buffer;//临时数据存储
                    QDataStream out(&buffer,QIODevice::WriteOnly);//创建数据流写入data
                    out.setVersion(QDataStream::Qt_5_15);
                    out<<data<<content;
                    socket->write(buffer);
                }else{
                    qDebug()<<"未找到该群聊:id="<<id;
                    errorFunc();
                }
            }else{
                QString error=query.lastError().text();
                qDebug()<<"patch_group sql语句执行失败"<<error;
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
            sql="select id_user1 as friend_id from friends where id_user2=:id "
                "union all "
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
        }else if(type=="load_groups"){
            int id = qstr_list[1].toInt();
            QSet<int> groups;
            sql="select id as id_group from chatgroup where owner=:id "
                "union all "
                "select id_group       from join_group where id_user=:id "
                "union all "
                "select id_group       from manage_group where id_user=:id;";
            query.prepare(sql);
            query.bindValue(":id",id);
            if(query.exec()){
                while(query.next()){
                    groups.insert(query.value(0).toInt());
                }
                QByteArray content;
                for(const int& id_group : groups){
                    content += QString("/%1").arg(id_group).toUtf8();
                }
                sendMsg(socket,content,'s');
            }else{
                QString error=query.lastError().text();
                qDebug()<<"load_groups sql语句执行失败"<<error;
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
        }else if(type=="create_chatgroup"){
            QString name = qstr_list[1];
            int owner =  qstr_list[3].toInt();
            QString sql = QString("insert into chatgroup(name,owner) value(:name,:owner);");
            query.prepare(sql);
            query.bindValue(":name",name);
            query.bindValue(":owner",owner);

            if(query.exec()){
                QString sql2 = QString("select id from chatgroup where name=:name and owner=:owner;");
                query.prepare(sql2);
                query.bindValue(":name",name);
                query.bindValue(":owner",owner);
                if(query.exec() && query.next()){
                    int id_group = query.value(0).toInt();
                    QByteArray content = "/" + QString::number(id_group).toUtf8();
                    sendMsg(socket,content,'s');

                    qDebug()<<QString("为%1创建群聊成功,群聊id%2").arg(owner).arg(id_group);
                }else{
                    sendMsg(socket,"",'f');
                }
            }else{
                sendMsg(socket,"",'f');
            }
        }else if(type=="modify_chatgroup"){
            QString name = qstr_list[1];
            QString intro = qstr_list[3];
            int id = qstr_list[5].toInt();
            QString sql=QString("update chatgroup set name=:name,intro=:intro where id=:id;");
            query.prepare(sql);
            query.bindValue(":id",id);
            query.bindValue(":name",name);
            query.bindValue(":intro",intro);
            if(query.exec()){
                sendMsg(socket,"",'s');
            }else{
                sendMsg(socket,"",'f');
            }
        }else if(type=="join_chatgroup"){
            int id_user = qstr_list[1].toInt();
            int id_group = qstr_list[3].toInt();
            QString sql=QString("insert into join_group(id_user,id_group) value(:id_user,:id_group);");
            query.prepare(sql);
            query.bindValue(":id_user",id_user);
            query.bindValue(":id_group",id_group);
            if(query.exec()){
                sendMsg(socket,"",'s');
                map_to_members[id_group].append(GroupMember(id_user,1));
            }else{
                sendMsg(socket,"",'f');
            }
        }else if(type=="role_of_chatgroup"){
            int id_user = qstr_list[1].toInt();
            int id_group = qstr_list[3].toInt();
            QString sql=QString(
                "select case "
                "when exists(select 1 from chatgroup    where id      =:id_group and owner  =:id_user) then 3 "
                "when exists(select 1 from manage_group where id_group=:id_group and id_user=:id_user) then 2 "
                "when exists(select 1 from join_group   where id_group=:id_group and id_user=:id_user) then 1 "
                "else 0 "
                "end as role;");
            query.prepare(sql);
            query.bindValue(":id_user",id_user);
            query.bindValue(":id_group",id_group);
            if(query.exec() && query.next()){
                int role =query.value(0).toInt();
                QByteArray msg = QString("/%1").arg(role).toUtf8();
                sendMsg(socket,msg,'s');
            }else{
                sendMsg(socket,"",'f');
                qDebug()<<"sql执行失败"<<query.lastQuery();
            }
        }else if(type=="leave_chatgroup"){
            int id_user = qstr_list[1].toInt();
            int id_group = qstr_list[3].toInt();
            QString sql="delete from manage_group where id_user=:id_user and id_group=:id_group;"
                        "delete from join_group   where id_user=:id_user and id_group=:id_group;";
            query.prepare(sql);
            query.bindValue(":id_user",id_user);
            query.bindValue(":id_group",id_group);
            if(query.exec()){
                sendMsg(socket,"",'s');
                map_to_members[id_group].removeOne(GroupMember(id_user,1));
            }else{
                sendMsg(socket,"",'f');
            }
        }else{
            qDebug()<<"未能识别的业务类型:"<<type;
            sendMsg(socket,"业务错误",'f');
        }
    }else{
        qWarning()<<"数据库打开失败"<<db.lastError().text();
    }
    db.close();
}

QString MainWindow::handleDataHead(QByteArray &data, const QChar &split)
{
    if(!data.isEmpty() && data[0]!='*'){
        return "handleError";
    }

    QString result;
    int counter=0;
    data = data.mid(1);//移除"*"
    for(char& c:data){
        if(c == split){
            data=data.mid(counter+1);//移除所有参与了计数的字符再移除“*”
            break;
        }else{
            counter++;
            result.append(c);
        }
    }
    return result;
}

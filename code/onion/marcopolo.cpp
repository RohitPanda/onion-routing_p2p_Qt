#include "marcopolo.h"
#include "QTimer"

MarcoPolo::MarcoPolo(QObject *parent) : QObject(parent)
{

}

void MarcoPolo::setMarco(Binding peer)
{
    peer_ = peer;
}

void MarcoPolo::setPolo(bool enable)
{
    polo_ = enable;
}

void MarcoPolo::start()
{
    if(peer_.isValid()) {
        // wait a little
        QTimer::singleShot(5000, [=]() {
            qDebug() << "Marco building tunnel...";
            requestBuildTunnel(peer_.address, peer_.port, "MARCO->POLO", nullptr);
        });
    }
}

void MarcoPolo::onTunnelReady(QTcpSocket *requester, quint32 tunnelId, QByteArray hostkey)
{
    Q_UNUSED(requester);
    poloTunnelId = tunnelId;
    qDebug() << "marco->polo :=" << tunnelId << "hostkey =>" << hostkey;
    marco();
}

void MarcoPolo::onTunnelIncoming(quint32 tunnelId)
{
    qDebug() << "incoming tunnel:" << tunnelId;
}

void MarcoPolo::onTunnelData(quint32 tunnelId, QByteArray data)
{
    qDebug() << "incoming data on tunnel" << tunnelId << "->" << data;
    if(data == "MARCO") {
        qDebug() << "marco on" << tunnelId;
        if(polo_) {
            qDebug() << "sending polo";
            requestSendTunnel(tunnelId, "POLO");
        }
    } else if(data == "POLO") {
        qDebug() << "polo on " << tunnelId;
        polos_++;
        if(poloTunnelId != 0) {
            if(polos_ > 100) {
                qDebug() << "tearing tunnel round after 100 polos..";
                requestDestroyTunnel(poloTunnelId);
            } else {
                marco();
            }
        }
    }
}

void MarcoPolo::onTunnelError(quint32 tunnelId, MessageType requestType)
{
    Q_UNUSED(requestType);
    qDebug() << "error on tunnel" << tunnelId;
}

void MarcoPolo::marco()
{
    qDebug() << "sending marco";
    requestSendTunnel(poloTunnelId, "MARCO");
}

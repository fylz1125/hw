/*
 * Hedgewars, a worms-like game
 * Copyright (c) 2005 Andrey Korotaev <unC0Rr@gmail.com>
 *
 * Distributed under the terms of the BSD-modified licence:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QByteArray>
#include <QTextStream>
#include <QFile>
#include "game.h"
#include "hwconsts.h"

HWGame::HWGame(int Resolution, bool Fullscreen)
{
	vid_Resolution = Resolution;
	vid_Fullscreen = Fullscreen;
	TeamCount = 0;
	seed = "";
	cfgdir.setPath(cfgdir.homePath());
	cfgdir.cd(".hedgewars");
}

void HWGame::NewConnection()
{
	QTcpSocket * client = IPCServer->nextPendingConnection();
	if(!IPCSocket)
	{
		IPCServer->close();
		IPCSocket = client;
		connect(client, SIGNAL(disconnected()), this, SLOT(ClientDisconnect()));
		connect(client, SIGNAL(readyRead()), this, SLOT(ClientRead()));
		msgsize = 0;
		if (toSendBuf.size() > 0)
			SENDIPC("?");
	} else
	{
		qWarning("2nd IPC client?!");
		client->disconnectFromHost();
	}
}

void HWGame::ClientDisconnect()
{
	SaveDemo("demo.hwd_1");
    IPCSocket->deleteLater();
	IPCSocket = 0;
	deleteLater();
}

void HWGame::SendTeamConfig(int index)
{
	LocalCFG(teams[index]);
}

void HWGame::SendConfig()
{
	SENDIPC("TL");
//	SENDIPC("e$gmflags 0");
	SENDIPC("eaddteam");
	SendTeamConfig(0);
	SENDIPC("ecolor 65535");
	SENDIPC("eadd hh0 0");
	SENDIPC("eadd hh1 0");
	SENDIPC("eadd hh2 0");
	SENDIPC("eadd hh3 0");
	SENDIPC("eadd hh4 0");
	SENDIPC("eaddteam");
	SendTeamConfig(1);
	SENDIPC("ecolor 16776960");
	SENDIPC("eadd hh0 1");
	SENDIPC("eadd hh1 1");
	SENDIPC("eadd hh2 1");
	SENDIPC("eadd hh3 1");
	SENDIPC("eadd hh4 1");
}

void HWGame::ParseMessage()
{
	switch(msgbuf[0])
	{
		case '?':
		{
			if (gameType == gtNet)
				emit SendNet(QByteArray("\x01""?"));
			else
				SENDIPC("!");
			break;
		}
		case 'C':
		{
			if (gameType == gtNet)
			{
				SENDIPC("TN");
				emit SendNet(QByteArray("\x01""C"));
			}
			else
			{
				if (gameType == gtLocal)
				 	SendConfig();
			}
			break;
		}
		case '+':
		{
			if (gameType == gtNet)
			{
				QByteArray tmpbuf = QByteArray::fromRawData((char *)&msgsize, 1) + QByteArray::fromRawData(msgbuf, msgsize);
				emit SendNet(tmpbuf);
			}
			break;
		}
		default:
		{
			QByteArray tmpbuf = QByteArray::fromRawData((char *)&msgsize, 1) + QByteArray::fromRawData(msgbuf, msgsize);
			if (gameType == gtNet)
			{
				emit SendNet(tmpbuf);
			}
			demo->append(tmpbuf);
		}
	}
}

void HWGame::SendIPC(const char * msg, quint8 len)
{
	SendIPC(QByteArray::fromRawData(msg, len));
}

void HWGame::SendIPC(const QByteArray & buf)
{
	if (buf.size() > MAXMSGCHARS) return;
	quint8 len = buf.size();
	RawSendIPC(QByteArray::fromRawData((char *)&len, 1) + buf);
}

void HWGame::RawSendIPC(const QByteArray & buf)
{
	if (!IPCSocket)
	{
		toSendBuf += buf;
	} else
	{
		if (toSendBuf.size() > 0)
		{
			IPCSocket->write(toSendBuf);
			demo->append(toSendBuf);
			toSendBuf.clear();
		}
		IPCSocket->write(buf);
		demo->append(buf);
	}
}

void HWGame::FromNet(const QByteArray & msg)
{
	RawSendIPC(msg);
}

void HWGame::ClientRead()
{
	qint64 readbytes = 1;
	while (readbytes > 0)
	{
		if (msgsize == 0)
		{
			msgbufsize = 0;
			readbytes = IPCSocket->read((char *)&msgsize, 1);
		} else
		{
			msgbufsize +=
			readbytes = IPCSocket->read((char *)&msgbuf[msgbufsize], msgsize - msgbufsize);
			if (msgbufsize = msgsize)
			{
				ParseMessage();
				msgsize = 0;
			}
		}
	}
}

void HWGame::Start()
{
	IPCServer = new QTcpServer(this);
	connect(IPCServer, SIGNAL(newConnection()), this, SLOT(NewConnection()));
	IPCServer->setMaxPendingConnections(1);
	IPCSocket = 0;
	if (!IPCServer->listen(QHostAddress::LocalHost, IPC_PORT))
	{
		QMessageBox::critical(0, tr("Error"),
				tr("Unable to start the server: %1.")
				.arg(IPCServer->errorString()));
	}

	QProcess * process;
	QStringList arguments;
	process = new QProcess;
	arguments << resolutions[0][vid_Resolution];
	arguments << resolutions[1][vid_Resolution];
	arguments << GetThemeBySeed();
	arguments << "46631";
	arguments << seed;
	arguments << (vid_Fullscreen ? "1" : "0");
	process->start("./hw", arguments);
}

void HWGame::AddTeam(const QString & teamname)
{
	if (TeamCount == 5) return;
	teams[TeamCount] = teamname;
	TeamCount++;
}

QString HWGame::GetThemeBySeed()
{
	QFile themesfile(QString(DATA_PATH) + "/Themes/themes.cfg");
	QStringList themes;
	if (themesfile.open(QIODevice::ReadOnly))
	{
		QTextStream stream(&themesfile);
		QString str;
		while (!stream.atEnd())
		{
			themes << stream.readLine();
		}
		themesfile.close();
	}
	quint32 len = themes.size();
	if (len == 0)
	{
		QMessageBox::critical(0, "Error", "Cannot access themes.cfg or bad data", "OK");
		return "avematan";
	}
	if (seed.isEmpty())
	{
		QMessageBox::critical(0, "Error", "seed not defined", "OK");
		return "avematan";
	}
	quint32 k = 0;
	for (int i = 0; i < seed.length(); i++)
	{
		k += seed[i].cell();
	}
	return themes[k % len];
}

void HWGame::SaveDemo(const QString & filename)
{
	QFile demofile(filename);
	if (!demofile.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(0,
				tr("Error"),
				tr("Cannot save demo to file %s").arg(filename),
				tr("Quit"));
		return ;
	}
	QDataStream stream(&demofile);
	stream.writeRawData(demo->constData(), demo->size());
	demofile.close();
	delete demo;
}

void HWGame::PlayDemo(const QString & demofilename)
{
	gameType = gtDemo;
	QFile demofile(demofilename);
	if (!demofile.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(0,
				tr("Error"),
				tr("Cannot open demofile %s").arg(demofilename),
				tr("Quit"));
		return ;
	}

	// read demo
	QDataStream stream(&demofile);
	char buf[512];
	quint32 readbytes;
	do
	{
		readbytes = stream.readRawData((char *)&buf, 512);
		toSendBuf.append(QByteArray((char *)&buf, readbytes));

	} while (readbytes > 0);
	demofile.close();

	// cut seed
	quint32 index = toSendBuf.indexOf(10);
	if ((index < 3) || (index > 20))
	{
		QMessageBox::critical(0,
				tr("Error"),
				tr("Corrupted demo file %1").arg(demofilename),
				tr("Quit"));
		return ;
	}
	seed = QString(toSendBuf.left(index++));
	toSendBuf.remove(0, index);

	toSendBuf = QByteArray::fromRawData("\x02TD", 3) + toSendBuf;
	// run engine
	demo = new QByteArray;
	Start();
}

void HWGame::StartNet(const QString & netseed)
{
	gameType = gtNet;
	seed = netseed;
	demo = new QByteArray;
	demo->append(seed.toLocal8Bit());
	demo->append(10);
	Start();
}

void HWGame::StartLocal()
{
	gameType = gtLocal;
	if (TeamCount < 2) return;
	seedgen.GenRNDStr(seed, 10);
	demo = new QByteArray;
	demo->append(seed.toLocal8Bit());
	demo->append(10);
	Start();
}

void HWGame::LocalCFG(const QString & teamname)
{
	QFile teamcfg(cfgdir.absolutePath() + "/" + teamname + ".cfg");
	if (!teamcfg.open(QIODevice::ReadOnly))
	{
		return ;
	}
	QTextStream stream(&teamcfg);
	stream.setCodec("UTF-8");
	QString str;

	while (!stream.atEnd())
	{
		str = stream.readLine();
		if (str.startsWith(";") || (str.length() > 254)) continue;
		str.prepend("e");
		SendIPC(str.toLocal8Bit());
	}
	teamcfg.close();
}

/* Copyright (C) 2016  François Blondel

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  (this is the zlib license)
*/

#include <QHostAddress>
#include <QNetworkInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QVector>
#include <QGestureEvent>

#include "worker.h"
#include "window.h"
#include "receiveosc.h"
#include "oscpkt.hh"



QString ipAdrr;
QList<QHostAddress> addr;


#if defined(Q_OS_ANDROID)   // sur android
QString locSettings = "/sdcard/settingsTeleco.ini";

#else                       //autres
QString locSettings ="settingsTeleco.ini";
#endif

QSettings settings(locSettings, QSettings::IniFormat);
QString   PORT_NUMin   = settings.value("portin", "7001").toString();
int       PORT         = PORT_NUMin.toInt();
QString   ipDlight     = settings.value("ipdl","192.168.1.103").toString();
QString   PORT_NUMsend = settings.value("portsend", "7000").toString();

bool multiCastOn       = false;
bool nameAndNotContent = true;
bool premiereErreure   = true;

QString onglet;
int     freescaleValue;
QString selection;

extern QVector<int> chSceneSelected;
extern QVector<QString> chSceneSelectedLevels;

extern QVector<int> chPrepaSelected;
extern QVector<QString> chPrepaSelectedLevels;

extern QVector<int> subsSelected;
extern QVector<QString> subsSelectedLevels;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Worker::Worker(QObject *parent) :
    QObject(parent),
    ui(new WindowTeleco)
{
    ui->show();

    thread = new QThread();
    listener = new Listener();
    listener->moveToThread(thread);

    groupAddress = QHostAddress("225.9.99.9");
    IpSend = groupAddress;

    connect(listener, SIGNAL(workRequested()),   thread, SLOT(start()));
    connect(thread,   SIGNAL(started()),       listener, SLOT(goOSC()));
    connect(listener, SIGNAL(finished()),        thread, SLOT(quit()));
    connect(listener, SIGNAL(reboot()),        listener, SLOT(goOSC()));

    connect(ui->tabs, SIGNAL(currentChanged(int)), this, SLOT(tabindex(int)));

    timer1 = new QTimer(ui->magicSlider);
    connect(timer1, SIGNAL(timeout()), this, SLOT(freescale()));

#if defined(Q_OS_IOS)
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(supensionEtReveil(Qt::ApplicationState)));
#endif

//affichage page OSC

    ui->portTeleco->setText(PORT_NUMin);
    ui->iPDLight->setText(ipDlight);
    ui->portDlight->setText(PORT_NUMsend);


//OSC

    connect(ui->refresh,     SIGNAL(pressed()),                   this, SLOT(freshIp()));
    connect(ui->iPTeleco,    SIGNAL(currentTextChanged(QString)), this, SLOT(IPTelecoChanged(QString)));
    connect(ui->iPDLight,    SIGNAL(textChanged(QString)),        this, SLOT(valideIpDlight()));
    connect(ui->portDlight,  SIGNAL(textChanged(QString)),        this, SLOT(validePortSend()));
    connect(ui->portTeleco,  SIGNAL(textChanged(QString)),        this, SLOT(validePortIn()));
    connect(ui->request,     SIGNAL(pressed()),                   this, SLOT(scan()));
    connect(ui->scanReseau,  SIGNAL(pressed()),                   this, SLOT(startOSC()));
    connect(ui->castButton,  SIGNAL(clicked()),                   this, SLOT(castModeSelect()));


//home
    connect(ui->chiffresPad,  SIGNAL(buttonClicked(int)),         this, SLOT(pad(int)));
    connect(ui->padPoint,     SIGNAL(clicked()),                  this, SLOT(padPoint()));
    connect(ui->padClear,     SIGNAL(clicked()),                  this, SLOT(padclear()));
    connect(ui->padClear,     SIGNAL(doubleClick()),              this, SLOT(paddoubleclear()));
    connect(ui->padPlus,      SIGNAL(clicked()),                  this, SLOT(padplus()));
    connect(ui->padMoins,     SIGNAL(clicked()),                  this, SLOT(padmoins()));
    connect(ui->padTHRU,      SIGNAL(clicked()),                  this, SLOT(padthru()));
    connect(ui->padCH,        SIGNAL(clicked()),                  this, SLOT(padchannel()));
    connect(ui->padAT,        SIGNAL(clicked()),                  this, SLOT(padlevel()));
    connect(ui->padEnter,     SIGNAL(clicked()),                  this, SLOT(padenter()));
    connect(ui->padFULL,      SIGNAL(clicked()),                  this, SLOT(padfull()));
    connect(ui->padALL,       SIGNAL(clicked()),                  this, SLOT(padall()));
    connect(ui->padNext,      SIGNAL(clicked()),                  this, SLOT(padNext()));
    connect(ui->padPrev,      SIGNAL(clicked()),                  this, SLOT(padPrev()));
    connect(ui->padSub,       SIGNAL(clicked()),                  this, SLOT(padSub()));
    connect(ui->padGrp,       SIGNAL(clicked()),                  this, SLOT(padGrp()));
    connect(ui->padRec,       SIGNAL(clicked()),                  this, SLOT(padRec()));
    connect(ui->padUpdate,    SIGNAL(clicked()),                  this, SLOT(padUpdate()));
    connect(ui->padUpdate,    SIGNAL(doubleClick()),              this, SLOT(padDoubleUpdate()));
    connect(ui->padGoto,      SIGNAL(clicked()),                  this, SLOT(padGoto()));
    connect(ui->padGoto,      SIGNAL(doubleClick()),              this, SLOT(padDoubleGoto()));
    connect(ui->padGotoCombo, SIGNAL(currentIndexChanged(int)),   this, SLOT(gotoCombo(int)));
    connect(ui->padScene,     SIGNAL(clicked(bool)),              this, SLOT(scene(bool)));
    connect(ui->yes,          SIGNAL(clicked()),                  this, SLOT(yES()));
    connect(ui->no,           SIGNAL(clicked()),                  this, SLOT(nO()));
    connect(ui->masterSubsT,  SIGNAL(slideValue(int)),            this, SLOT(masterSubs(int)));
    connect(ui->masterSceneT, SIGNAL(slideValue(int)),            this, SLOT(masterScene(int)));
    connect(ui->magicSliderT,  SIGNAL(slideValue(int)),           this, SLOT(valueMS(int)));
    connect(ui->magicSliderT,  SIGNAL(sliderReleased()),           this, SLOT(endMSlider()));


//SUBS   
    connect(ui->subnumpage, SIGNAL(currentIndexChanged(int)), this, SLOT(subNumPage(int)));

    connect(ui->Sub01T,          SIGNAL(slideValue(int)),    this, SLOT(sub01(int)));
    connect(ui->Sub02T,          SIGNAL(slideValue(int)),    this, SLOT(sub02(int)));
    connect(ui->Sub03T,          SIGNAL(slideValue(int)),    this, SLOT(sub03(int)));
    connect(ui->Sub04T,          SIGNAL(slideValue(int)),    this, SLOT(sub04(int)));
    connect(ui->Sub05T,          SIGNAL(slideValue(int)),    this, SLOT(sub05(int)));
    connect(ui->Sub06T,          SIGNAL(slideValue(int)),    this, SLOT(sub06(int)));
    connect(ui->Sub07T,          SIGNAL(slideValue(int)),    this, SLOT(sub07(int)));
    connect(ui->Sub08T,          SIGNAL(slideValue(int)),    this, SLOT(sub08(int)));
    connect(ui->Sub09T,          SIGNAL(slideValue(int)),    this, SLOT(sub09(int)));
    connect(ui->Sub10T,          SIGNAL(slideValue(int)),    this, SLOT(sub10(int)));



    connect(ui->Flash01, SIGNAL(buttonPressed()),  this, SLOT(flashOn1()));
    connect(ui->Flash02, SIGNAL(buttonPressed()),  this, SLOT(flashOn2()));
    connect(ui->Flash03, SIGNAL(buttonPressed()),  this, SLOT(flashOn3()));
    connect(ui->Flash04, SIGNAL(buttonPressed()),  this, SLOT(flashOn4()));
    connect(ui->Flash05, SIGNAL(buttonPressed()),  this, SLOT(flashOn5()));
    connect(ui->Flash06, SIGNAL(buttonPressed()),  this, SLOT(flashOn6()));
    connect(ui->Flash07, SIGNAL(buttonPressed()),  this, SLOT(flashOn7()));
    connect(ui->Flash08, SIGNAL(buttonPressed()),  this, SLOT(flashOn8()));
    connect(ui->Flash09, SIGNAL(buttonPressed()),  this, SLOT(flashOn9()));
    connect(ui->Flash10, SIGNAL(buttonPressed()),  this, SLOT(flashOn10()));

    connect(ui->Flash01, SIGNAL(buttonReleased()), this, SLOT(flashOff1()));
    connect(ui->Flash02, SIGNAL(buttonReleased()), this, SLOT(flashOff2()));
    connect(ui->Flash03, SIGNAL(buttonReleased()), this, SLOT(flashOff3()));
    connect(ui->Flash04, SIGNAL(buttonReleased()), this, SLOT(flashOff4()));
    connect(ui->Flash05, SIGNAL(buttonReleased()), this, SLOT(flashOff5()));
    connect(ui->Flash06, SIGNAL(buttonReleased()), this, SLOT(flashOff6()));
    connect(ui->Flash07, SIGNAL(buttonReleased()), this, SLOT(flashOff7()));
    connect(ui->Flash08, SIGNAL(buttonReleased()), this, SLOT(flashOff8()));
    connect(ui->Flash09, SIGNAL(buttonReleased()), this, SLOT(flashOff9()));
    connect(ui->Flash10, SIGNAL(buttonReleased()), this, SLOT(flashOff10()));

    connect(ui->masterpagemoins, SIGNAL(clicked()),           this, SLOT(mpmoins()));
    connect(ui->masterpageplus,  SIGNAL(clicked()),           this, SLOT(mpplus()));
    connect(ui->nameOrContent,   SIGNAL(clicked(bool)),       this, SLOT(nameOrContent(bool)));


//scene
    connect(ui->stepPlus,  SIGNAL(clicked()),        this, SLOT(seqplus()));
    connect(ui->stepMoins, SIGNAL(clicked()),        this, SLOT(seqmoins()));
    connect(ui->Pause,     SIGNAL(clicked()),        this, SLOT(seqpause()));
    connect(ui->GoBack,    SIGNAL(clicked()),        this, SLOT(seqgoback()));
    connect(ui->GO,        SIGNAL(clicked()),        this, SLOT(seqgo()));
    connect(ui->sliderX1T, SIGNAL(slideValue(int)),  this, SLOT(slideX1(int)));
    connect(ui->sliderX2T, SIGNAL(slideValue(int)),  this, SLOT(slideX2(int)));
    connect(ui->joystickT, SIGNAL(slideValue(int)),  this, SLOT(joystick(int)));


//patch

    connect(ui->chiffresPatch, SIGNAL(buttonClicked(int)), this, SLOT(padPatch(int)));
    connect(ui->padPoint_2,    SIGNAL(clicked()),          this, SLOT(padPointp()));
    connect(ui->padClear_2,    SIGNAL(clicked()),          this, SLOT(padclearp()));
    connect(ui->padPlus_2,     SIGNAL(clicked()),          this, SLOT(padplusp()));
    connect(ui->padMoins_2,    SIGNAL(clicked()),          this, SLOT(padmoinsp()));
    connect(ui->padTHRU_2,     SIGNAL(clicked()),          this, SLOT(padthrup()));
    connect(ui->padEnter_2,    SIGNAL(clicked()),          this, SLOT(padenterp()));
    connect(ui->padPATCH,      SIGNAL(clicked()),          this, SLOT(patch()));
    connect(ui->padUNPATCH,    SIGNAL(clicked()),          this, SLOT(unpatch()));
    connect(ui->padCh_2,       SIGNAL(clicked(bool)),      this, SLOT(patchchannel(bool)));
    connect(ui->dimmer,        SIGNAL(clicked(bool)),      this, SLOT(patchdimmer(bool)));
    connect(ui->padPREV,       SIGNAL(clicked()),          this, SLOT(prev()));
    connect(ui->padNEXT,       SIGNAL(clicked()),          this, SLOT(next()));
    connect(ui->channelTest,   SIGNAL(clicked(bool)),      this, SLOT(channeltest(bool)));
    connect(ui->dimmerTest,    SIGNAL(clicked(bool)),      this, SLOT(dimmertest(bool)));
    connect(ui->testLevelT,    SIGNAL(slideValue(int)),    this, SLOT(testlevel(int)));
    connect(ui->yes_2,         SIGNAL(clicked()),          this, SLOT(yESp()));
    connect(ui->no_2,          SIGNAL(clicked()),          this, SLOT(nOp()));




/////inputs//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////



// OSC
    connect(listener, SIGNAL(connexionOk(bool)),           this, SLOT(connexionIsOk(bool)));
    connect(listener, SIGNAL(answer(QString, int)),        this, SLOT(answerDlight(QString, int)));
    connect(listener, SIGNAL(changeIpDl(QString)), ui->iPDLight, SLOT(setText(QString)));
    connect(listener, SIGNAL(changeIpDl(QString)),         this, SLOT(valideIpDlight()));
    connect(listener, SIGNAL(gotoMode(QString)),           this, SLOT(gotoMode(QString)));

// Home
    connect(listener, SIGNAL(mscLvl(int)),                  ui->masterScene, SLOT(setValue(int)));
    connect(listener, SIGNAL(msubLvl(int)),                  ui->masterSubs, SLOT(setValue(int)));
    connect(listener, SIGNAL(txtHome(QString)),                ui->ecrantxt, SLOT(setText(QString)));
    connect(listener, SIGNAL(txtSelect(QString, QString)),             this, SLOT(textSelect(QString, QString)));
    connect(listener, SIGNAL(joystickLevelText(QString)), ui->joystickLevel, SLOT(setText(QString)));
    connect(listener, SIGNAL(joystickLevel(int)),              ui->joystick, SLOT(setValue(int)));
    connect(listener, SIGNAL(scene(bool)),                     ui->padScene, SLOT(setChecked(bool)));
    connect(listener, SIGNAL(padInfo(QString)),                        this, SLOT(padInfo(QString)));
    connect(listener, SIGNAL(error(int)),                              this, SLOT(errorIN(int)));
    connect(listener, SIGNAL(majNiveauxSelection()),                   this, SLOT(majNiveauxSelection()));

// Substicks
    connect(listener, SIGNAL(subPageText(QString)),    ui->nomPageSub, SLOT(setText(QString)));
    connect(listener, SIGNAL(subPageNo(QString)), ui->subnumpageLabel, SLOT(setText(QString)));
    connect(listener, SIGNAL(subNo(QString, int)),               this, SLOT(setSubNum(QString, int)));
    connect(listener, SIGNAL(subMode(QString, int)),             this, SLOT(chgText(QString,int)));
    connect(listener, SIGNAL(subName(QString, int)),             this, SLOT(setSubName(QString, int)));
    connect(listener, SIGNAL(subLvl(int, int)),                  this, SLOT(setSubValue(int, int)));
    connect(listener, SIGNAL(flashFullOn(int)),                  this, SLOT(setSubFlashOn(int)));
    connect(listener, SIGNAL(flashFullOff(int)),                 this, SLOT(setSubFlashOff(int)));
    
// Scene
    connect(listener, SIGNAL(lvlX1(int)),        ui->sliderX1, SLOT(setValue(int)));
    connect(listener, SIGNAL(lvlX2(int)),        ui->sliderX2, SLOT(setValue(int)));
    connect(listener, SIGNAL(noX1(QString)),      ui->nStepX1, SLOT(setText(QString)));
    connect(listener, SIGNAL(noX2(QString)),      ui->nStepX2, SLOT(setText(QString)));
    connect(listener, SIGNAL(cueX1(QString)),      ui->nCueX1, SLOT(setText(QString)));
    connect(listener, SIGNAL(cueX2(QString)),      ui->nCueX2, SLOT(setText(QString)));
    connect(listener, SIGNAL(X1text(QString)), ui->textStepX1, SLOT(setText(QString)));
    connect(listener, SIGNAL(X2text(QString)), ui->textStepX2, SLOT(setText(QString)));
    connect(listener, SIGNAL(goSignal(int)),             this, SLOT(goButton(int)));
    connect(listener, SIGNAL(goBackSignal(int)),         this, SLOT(goBackButton(int)));
    connect(listener, SIGNAL(pauseSignal(int)),          this, SLOT(pauseButton(int)));

// Patch
    connect(listener, SIGNAL(txtPatch(QString)), ui->ecrantxt_2, SLOT(setText(QString)));
    connect(listener, SIGNAL(errorPatch(int)),             this, SLOT(errorINPatch(int)));
    connect(listener, SIGNAL(testLvl(int)),                this, SLOT(affichelevel(int)));
    connect(listener, SIGNAL(chantest(QString)),   ui->nChannel, SLOT(setText(QString)));
    connect(listener, SIGNAL(dimtest(QString)),     ui->nDimmer, SLOT(setText(QString)));
    connect(listener, SIGNAL(testChan(bool)),   ui->channelTest, SLOT(setChecked(bool)));
    connect(listener, SIGNAL(testDim(bool)),     ui->dimmerTest, SLOT(setChecked(bool)));
    connect(listener, SIGNAL(chan(bool)),           ui->padCh_2, SLOT(setChecked(bool)));
    connect(listener, SIGNAL(dim(bool)),             ui->dimmer, SLOT(setChecked(bool)));
    connect(listener, SIGNAL(chan(bool)),          ui->nChannel, SLOT(setEnabled(bool)));
    connect(listener, SIGNAL(dim(bool)),            ui->nDimmer, SLOT(setEnabled(bool)));


#if defined (Q_OS_ANDROID) || defined (Q_OS_OSX)
    udpSocketSend = new QUdpSocket(this);
    freshIp();
    startOSC();
    scan();
#endif
}

Worker::~Worker()
{
    delete ui;
}





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Worker::sendOSC(Message msg)
    {QHostAddress myIP;
    if((premiereErreure)&!(myIP.setAddress( ui->iPDLight->text())))
          {QMessageBox msgBox;
           msgBox.setText("Invalid D::Light Ip Adress");
           msgBox.exec();
           premiereErreure=false;
           return;
           }
    PacketWriter pw;
    pw.addMessage(msg);
    udpSocketSend->writeDatagram(pw.packetData(), pw.packetSize(), IpSend, PORT_NUMsend.toInt());
    }

void Worker::tabindex(int index)
    {if (index == 0)
    {onglet="OSC";
    Message msg("/pad/launch"); msg.pushInt32(1); sendOSC(msg);
     }
    if (index == 1)
    {onglet="home";
    Message msg("/pad/launch"); msg.pushInt32(1); sendOSC(msg);
     }
    if (index == 2)
    {onglet="subs";
    Message msg("/sub/launch"); msg.pushInt32(1); sendOSC(msg);
     }
    if (index == 3)
    {onglet="seq";
    Message msg("/seq/launch"); msg.pushInt32(1); sendOSC(msg);
     }
    if (index == 4)
    {onglet="patch";
    Message msg("/patch/launch"); msg.pushInt32(1); sendOSC(msg);
     }
    }


//OSC///////////////////////////////////////////////////////////////////////////////

void Worker::IPTelecoChanged(QString ip)
    { ipAdrr = ip; }

void Worker::valideIpDlight()
    { ipDlight = ui->iPDLight->text();
     QSettings settings(locSettings, QSettings::IniFormat);
     settings.setValue("ipdl", ipDlight);
     premiereErreure=true;
     }

void Worker::validePortSend()
    { PORT_NUMsend = ui->portDlight->text();
     QSettings settings(locSettings, QSettings::IniFormat);
     settings.setValue("portsend", PORT_NUMsend);
     }

void Worker::validePortIn()
    { PORT_NUMin = ui->portTeleco->text();
     QSettings settings(locSettings, QSettings::IniFormat);
     settings.setValue("portin", PORT_NUMin);
     PORT = PORT_NUMin.toInt();
     }


//home///////////////////////////////////////////////////////////////////////////////

void Worker::pad(int id)
    { QString id2 = QString("/pad/%1").arg(-id-2);
    Message msg(id2.toStdString()); msg.pushInt32(1);sendOSC(msg); }

void Worker::padPoint()
    { Message msg("/pad/dot"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padclear()
    { Message msg("/pad/clear"); msg.pushInt32(1); sendOSC(msg); }

void Worker::paddoubleclear()
    { Message msg("/pad/clearclear"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padplus()
    {  Message msg("/pad/plus"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padmoins()
    { Message msg("/pad/moins"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padthru()
    { Message msg("/pad/thru"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padchannel()
    { Message msg("/pad/channel"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padlevel()
    { Message msg("/pad/level"); msg.pushInt32(1);sendOSC(msg); }

void Worker::padenter()
    {Message msg("/pad/enter"); msg.pushInt32(1);sendOSC(msg); }

void Worker::padfull()
    { Message msg("/pad/ff"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padall()
    { Message msg("/pad/all"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padPrev()
    { Message msg("/pad/prev"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padNext()
    { Message msg("/pad/next"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padSub()
    { Message msg("/pad/sub"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padGrp()
    { Message msg("/pad/group"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padRec()
    { Message msg("/pad/record"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padUpdate()
    { timer2 = new QTimer(ui->ecrantxt);
    timer2->setInterval(300);
    connect(timer2, SIGNAL(timeout()), this, SLOT(afficheUpdate()));
    timer2->start();
    }

void Worker::afficheUpdate()
    { timer2->stop();
    ui->ecrantxt->setText("double Click to Update");
    timer = new QTimer(ui->ecrantxt);
    timer->setInterval(1000);
    connect(timer, SIGNAL(timeout()), this, SLOT(erase()));
    timer->start();
    }

void Worker::padDoubleUpdate()
    { timer2->stop();
      Message msg("/pad/update"); msg.pushInt32(1); sendOSC(msg);
     }

void Worker::gotoMode(QString mode)
    { if (mode == "Cue")  { ui->padGoto->setText("Goto\nCue");  }
      if (mode == "Step") { ui->padGoto->setText("Goto\nStep"); }
      if (mode == "ID")   { ui->padGoto->setText("Goto\nstID"); }
     }

void Worker::padGoto()
    { ui->padGotoCombo->close();
      timer2 = new QTimer;
      timer2->setInterval(300);
      connect(timer2, SIGNAL(timeout()), this, SLOT(envoiGoto()));
      timer2->start();
     }

void Worker::envoiGoto()
    { timer2->stop();
      Message msg("/seq/genericGoto"); msg.pushInt32(-1); sendOSC(msg);
     }

void Worker::padDoubleGoto()
    { timer2->stop();
      ui->padGotoCombo->setEnabled(true);
      ui->padGotoCombo->showPopup();
     }

void Worker::gotoCombo(int index)
    { Message msg("/request/gotoMode");
      if (index == 0)  {msg.pushStr("Cue");}
      if (index == 1)  {msg.pushStr("Step");}
      if (index == 2)  {msg.pushStr("ID");}
      sendOSC(msg);
      ui->padGotoCombo->setEnabled(false);
     }

void Worker::textSelect(QString sel, QString content)
    { if (sel=="X1")
         {selection="X1";
          chSceneSelected.clear();
          chSceneSelectedLevels.clear();

          QStringList eclate = content.split(" ");

          for (int i=0; i<eclate.count(); i++)
              {QStringList atEclate = eclate[i].split("@");
               if ((eclate[i])==""){}
                  else {
                        int num = atEclate[0].toInt();
                        QString lvl;
                        lvl = atEclate[1];
                        chSceneSelected << num;
                        chSceneSelectedLevels << lvl;}}
                ui->ecranSelect->setStyleSheet("QHeaderView::section          {border-color : red; color : red; background:transparent;}"
                                               "QHeaderView                   {border-color : red; color : red; background:transparent;}"
                                               "QTableView                    {gridline-color : red; color : red}"
                                               "QHeaderView::section:disabled {border-color : transparent; color : grey; background:transparent;}"
                                               "QHeaderView:disabled          {border-color : transparent; color : grey; background:transparent;}"
                                               "QTableView:disabled           {gridline-color : transparent; color:grey; border-color:transparent;}");
                QStringList names;
                names << "Ch" << "Level";
                ui->modele->setHorizontalHeaderLabels(names);
                ui->modele->setRowCount(chSceneSelected.count());
                majNiveauxSelection();
    }
    if (sel=="X2")
    {selection="X2";
            chPrepaSelected.clear();
            chPrepaSelectedLevels.clear();

            QStringList eclate = content.split(" ");

            for (int i=0; i<eclate.count(); i++)
            {QStringList atEclate = eclate[i].split("@");
    if ((eclate[i])==""){}
              else {
        int num = atEclate[0].toInt();
        QString lvl;
            lvl = atEclate[1];
            chPrepaSelected << num;
            chPrepaSelectedLevels << lvl;}}
            ui->ecranSelect->setStyleSheet("QHeaderView::section          {border-color : green; color : lightgreen; background:transparent;}"
                                           "QHeaderView                   {border-color : green; color : lightgreen; background:transparent;}"
                                           "QTableView                    {gridline-color : green; color : lightgreen}"
                                           "QHeaderView::section:disabled {border-color : transparent; color : grey; background:transparent;}"
                                           "QHeaderView:disabled          {border-color : transparent; color : grey; background:transparent;}"
                                           "QTableView:disabled           {gridline-color : transparent; color:grey; border-color:transparent;}");
            QStringList names;
            names << "Ch" << "Level";
            ui->modele->setHorizontalHeaderLabels(names);
            ui->modele->setRowCount(chPrepaSelected.count());
            majNiveauxSelection();
    }
    if (sel=="SUBMASTER")
    {selection="SUBMASTER";
        subsSelected.clear();
        subsSelectedLevels.clear();

        QStringList eclate = content.split(" ");

        for (int i=0; i<eclate.count(); i++)
        {QStringList atEclate = eclate[i].split("@");
if ((eclate[i])==""){}
          else {
    int num = atEclate[0].toInt();
    QString lvl;
        lvl = atEclate[1];
        subsSelected << num;
        subsSelectedLevels << lvl;}}
        ui->ecranSelect->setStyleSheet("QHeaderView::section          {border-color : blue; color : lightblue; background:transparent;}"
                                       "QHeaderView                   {border-color : blue; color : lightblue; background:transparent;}"
                                       "QTableView                    {gridline-color : blue; color : lightblue}"
                                       "QHeaderView::section:disabled {border-color : transparent; color : grey; background:transparent;}"
                                       "QHeaderView:disabled          {border-color : transparent; color : grey; background:transparent;}"
                                       "QTableView:disabled           {gridline-color : transparent; color:grey; border-color:transparent;}");
        QStringList names;
        names << "Sub" << "Level";
        ui->modele->setHorizontalHeaderLabels(names);
        ui->modele->setRowCount(subsSelected.count());
        majNiveauxSelection();
    }
}



void Worker::majNiveauxSelection()
{
    if (selection=="X1")
    {
    for (int i=0; i<chSceneSelected.count(); i++)
    {QStandardItem *first = new QStandardItem(QString(QString::number(chSceneSelected.at(i))));
        first->setTextAlignment(Qt::AlignCenter);
        ui->modele->setItem( i, 0, first);
    QStandardItem *second= new QStandardItem;
if (chSceneSelectedLevels[i]=="100"){second->setText("FF");second->setForeground(Qt::magenta);}
else {
    second->setText(QString(chSceneSelectedLevels[i]));second->setForeground(Qt::white);}
        second->setTextAlignment(Qt::AlignCenter);
        ui->modele->setItem( i, 1, second);        
    }
}
    if (selection=="X2")
    {
    for (int i=0; i<(chPrepaSelected.count()); i++)
    {QStandardItem *first = new QStandardItem(QString(QString::number(chPrepaSelected.at(i))));
        first->setTextAlignment(Qt::AlignCenter);
        ui->modele->setItem( i, 0, first);
    QStandardItem *second= new QStandardItem;
if (chPrepaSelectedLevels[i]=="100"){second->setText("FF");second->setForeground(Qt::red);}
else {second->setText(QString(chPrepaSelectedLevels[i]));second->setForeground(Qt::white);}
        second->setTextAlignment(Qt::AlignCenter);
        ui->modele->setItem( i, 1, second);
    }
}
    if (selection=="SUBMASTER")
        {
        for (int i=0; i<(subsSelected.count()); i++)
        {QStandardItem *first = new QStandardItem(QString(QString::number(subsSelected.at(i))));
            first->setTextAlignment(Qt::AlignCenter);
            ui->modele->setItem( i, 0, first);
        QStandardItem *second= new QStandardItem;
    if (subsSelectedLevels[i]=="100"){second->setText("FF");second->setForeground(Qt::red);}
    else {second->setText(QString(subsSelectedLevels[i]));second->setForeground(Qt::white);}
            second->setTextAlignment(Qt::AlignCenter);
            ui->modele->setItem( i, 1, second);
        }

}
}


void Worker::scene(bool tg)
    {if (tg == true) {
            Message msg("/pad/scene"); msg.pushInt32(1);
            ui->padScene->setChecked(true);
            ui->padScene->setText("X1");
            sendOSC(msg);
            }


    else {  Message msg("/pad/prepa");msg.pushInt32(1);
            ui->padScene->setChecked(false);
            ui->padScene->setText("X2");
            sendOSC(msg);
            }}

void Worker::yES()
    { Message msg("/pad/errorACKYES"); msg.pushInt32(1); sendOSC(msg); }

void Worker::nO()
    { Message msg("/pad/errorACKNO"); msg.pushInt32(1); sendOSC(msg); }

void Worker::masterSubs(int level)
    {
    if (ui->masterSubs->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/sub/master"); msg.pushInt32(i); sendOSC(msg);}}

    if (ui->masterSubs->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/sub/master"); msg.pushInt32(i); sendOSC(msg);}}

    Message msg("/sub/master"); msg.pushInt32(level); sendOSC(msg); }

void Worker::masterScene(int level)
    {
    if (ui->masterScene->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/seq/master"); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->masterScene->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/seq/master"); msg.pushInt32(i); sendOSC(msg);
        }}

    Message msg("/seq/master"); msg.pushInt32(level); sendOSC(msg); }

void Worker::errorIN(int ok)
  { bool faux = ok;
    bool vrai = !faux;

    if (ok) {ui->tabs->setCurrentIndex(1);}

    ui->padALL->setEnabled(vrai);
    ui->padRec->setEnabled(vrai);
    ui->padUpdate->setEnabled(vrai);
    ui->padGoto->setEnabled(vrai);
    ui->padAT->setEnabled(vrai);
    ui->padCH->setEnabled(vrai);
    ui->padClear->setEnabled(vrai);
    ui->padEnter->setEnabled(vrai);
    ui->padFULL->setEnabled(vrai);
    ui->padMoins->setEnabled(vrai);
    ui->padPrev->setEnabled(vrai);
    ui->padPlus->setEnabled(vrai);
    ui->padNext->setEnabled(vrai);
    ui->padPoint->setEnabled(vrai);
    ui->ecranSelect->setEnabled(vrai);
    ui->padGrp->setEnabled(vrai);
    ui->masterSceneT->setEnabled(vrai);
    ui->masterSubsT->setEnabled(vrai);
    ui->padSub->setEnabled(vrai);
    ui->padTHRU->setEnabled(vrai);
    ui->pad0->setEnabled(vrai);
    ui->pad1->setEnabled(vrai);
    ui->pad2->setEnabled(vrai);
    ui->pad3->setEnabled(vrai);
    ui->pad4->setEnabled(vrai);
    ui->pad5->setEnabled(vrai);
    ui->pad6->setEnabled(vrai);
    ui->pad7->setEnabled(vrai);
    ui->pad8->setEnabled(vrai);
    ui->pad9->setEnabled(vrai);
    ui->padScene->setEnabled(vrai);
   if (vrai) {ui->magicSlider->setStyleSheet("QSlider::groove:disabled   {border: 1px outset indigo; background: indigo; border-radius: 4px;}"
                                             "QSlider::sub-page:disabled {border: 1px outset indigo; background: indigo; border-radius: 4px;}"
                                             "QSlider::handle:disabled   {border: 3px outset fuchsia;border-radius: 10px; "
                                                                         "background: transparent;color : fushia;}");
          ui->textMagicSlider->setStyleSheet("QLabel                     {color:white; font:bold; font-size:16pt;}");
                }

   if (faux) {ui->magicSlider->setStyleSheet("QSlider::groove:disabled   {background: darkgray; border: grey;}"
                                             "QSlider::sub-page:disabled {background: darkgray; border: grey;}"
                                             "QSlider::handle:disabled   {border: 3px outset grey;border-radius: 10px; background: transparent;}");
          ui->textMagicSlider->setStyleSheet("QLabel:disabled            {color:grey;}");
                }

       ui->yes->setEnabled(faux);
       ui->no->setEnabled(faux);
   }

void Worker::padInfo(QString info)
    { ui->ecrantxt->setText(info);
      timer = new QTimer(ui->ecrantxt);
      timer->setInterval(1000);
      connect(timer, SIGNAL(timeout()), this, SLOT(erase()));
      timer->start();
     }

void Worker::erase()
    { ui->ecrantxt->setText("");
      if (timer) timer->stop();
     }

void Worker::freescale()
    { Message msg("/pad/freescale"); msg.pushInt32(freescaleValue); sendOSC(msg); }

void Worker::valueMS(int val)
    {
    ui->magicSlider->setValue(100-val);
    freescaleValue=(50-val)/5;
      if (freescaleValue==0) {timer1->stop();}
          else
            {freescale();
            timer1->start(100);
             }
     }

void Worker::endMSlider()
    {timer1->stop();
     ui->magicSlider->setValue(50);
    }





//SUBS///////////////////////////////////////////////////////////////////////////////

void Worker::subNumPage(int index)
    { Message msg("/sub/page"); msg.pushInt32(index+1); sendOSC(msg); }

void Worker::sub01(int level)
    {
    if (ui->Sub01->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(1); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub01->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(1); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(1); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub02(int level)
    {
    if (ui->Sub02->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(2); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub02->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(2); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(2); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub03(int level)
    {
    if (ui->Sub03->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(3); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub03->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(3); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(3); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub04(int level)
    {
    if (ui->Sub04->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(4); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub04->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(4); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(4); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub05(int level)
    {
    if (ui->Sub05->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(5); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub05->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(5); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(5); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub06(int level)
    {
    if (ui->Sub06->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(6); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub06->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(6); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(6); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub07(int level)
    {
    if (ui->Sub07->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(7); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub07->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(7); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(7); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub08(int level)
    {
    if (ui->Sub08->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(8); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub08->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(8); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(8); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub09(int level)
    {
    if (ui->Sub09->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(9); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub09->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(9); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(9); msg.pushInt32(level); sendOSC(msg); }

void Worker::sub10(int level)
    {
    if (ui->Sub10->value()==0)
        { for (int i=0; i<level;i++)
        {Message msg("/subStick/level");msg.pushInt32(10); msg.pushInt32(i); sendOSC(msg);
        }}
    if (ui->Sub10->value()==255)
        { for (int i=255; i>level;i--)
        {Message msg("/subStick/level");msg.pushInt32(10); msg.pushInt32(254); sendOSC(msg);
        }} Message msg("/subStick/level");msg.pushInt32(10); msg.pushInt32(level); sendOSC(msg); }

void Worker::flashOn1()
    { Message msg("/subStick/flash");msg.pushInt32(1); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn2()
    { Message msg("/subStick/flash");msg.pushInt32(2); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn3()
    { Message msg("/subStick/flash");msg.pushInt32(3); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn4()
    { Message msg("/subStick/flash");msg.pushInt32(4); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn5()
    { Message msg("/subStick/flash");msg.pushInt32(5); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn6()
    { Message msg("/subStick/flash");msg.pushInt32(6); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn7()
    { Message msg("/subStick/flash");msg.pushInt32(7); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn8()
    { Message msg("/subStick/flash");msg.pushInt32(8); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn9()
    { Message msg("/subStick/flash");msg.pushInt32(9); msg.pushInt32(255); sendOSC(msg); }
void Worker::flashOn10()
    { Message msg("/subStick/flash");msg.pushInt32(10); msg.pushInt32(255); sendOSC(msg); }

void Worker::flashOff1()
    { Message msg("/subStick/flash");msg.pushInt32(1); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff2()
    { Message msg("/subStick/flash");msg.pushInt32(2); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff3()
    { Message msg("/subStick/flash");msg.pushInt32(3); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff4()
    { Message msg("/subStick/flash");msg.pushInt32(4); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff5()
    { Message msg("/subStick/flash");msg.pushInt32(5); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff6()
    { Message msg("/subStick/flash");msg.pushInt32(6); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff7()
    { Message msg("/subStick/flash");msg.pushInt32(7); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff8()
    { Message msg("/subStick/flash");msg.pushInt32(8); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff9()
    { Message msg("/subStick/flash");msg.pushInt32(9); msg.pushInt32(0); sendOSC(msg); }
void Worker::flashOff10()
    { Message msg("/subStick/flash");msg.pushInt32(10); msg.pushInt32(0); sendOSC(msg); }

void Worker::mpmoins()
    { Message msg("/sub/decrPage"); msg.pushInt32(1); sendOSC(msg); }

void Worker::mpplus()
    { Message msg("/sub/incrPage"); msg.pushInt32(1); sendOSC(msg); }

void Worker::nameOrContent(bool)
    { for (int i=0; i<10;i++)
          {ui->subsTextList->at(i)->setText("                                        ");}

    Message msg("/sub/page"); msg.pushInt32(ui->subnumpageLabel->text().toInt()); sendOSC(msg);

    if (nameAndNotContent)
       { ui->nameOrContent->setText("Content");
         ui->nameOrContent->setStyleSheet("QPushButton {background-color:black; color:white; border-color:white;}");
         nameAndNotContent=false;
        }
    else
       { ui->nameOrContent->setText(" Name  ");
         ui->nameOrContent->setStyleSheet("QPushButton {background-color:navy; color:white; border-color:navy;}");
         nameAndNotContent=true;
        }
    }
void Worker::setSubFlashOn(int no)
{ ui->subsFlashList->at(no-1)->setChecked(true); }

void Worker::setSubFlashOff(int no)
{ ui->subsFlashList->at(no-1)->setChecked(false);}

void Worker::setSubNum(QString sub, int no)
{ ui->subsNumList->at(no-1)->setText(sub); }

void Worker::chgText(QString flash, int no)
{ ui->subsFlashList->at(no-1)->setText(flash); }

void Worker::setSubName(QString sub, int no)
{ ui->subsTextList->at(no-1)->setText(sub); }

void Worker::setSubValue(int sub, int no)
{ ui->subsList->at(no-1)->setValue(sub);}




//scene///////////////////////////////////////////////////////////////////////////////
void Worker::seqplus()
{ Message msg("/seq/plus"); msg.pushInt32(1); sendOSC(msg); }

void Worker::seqmoins()
{ Message msg("/seq/moins"); msg.pushInt32(1); sendOSC(msg); }

void Worker::seqpause()
{ Message msg("/seq/pause"); msg.pushInt32(1); sendOSC(msg); }

void Worker::seqgoback()
{ Message msg("/seq/goback"); msg.pushInt32(1); sendOSC(msg); }

void Worker::seqgo()
{ Message msg("/seq/go"); msg.pushInt32(1); sendOSC(msg); }

void Worker::slideX1(int value)
{ Message msg("/seq/fadeX1"); msg.pushInt32((value)*2.55); sendOSC(msg); }

void Worker::slideX2(int value)
{ Message msg("/seq/fadeX2"); msg.pushInt32((value)*2.55); sendOSC(msg); }

void Worker::pauseButton(int pause)
{
    if (pause==0)
            { ui->Pause->setStyleSheet("QPushButton{ background-color : black; border : solid; font-size:15pt;"
                                           "border: 1px outset white;height: 35px;"
                                           "border-radius: 5px;}");}
       else { ui->Pause->setStyleSheet("QPushButton{ background-color: red; border-color: red; font-size:15pt;height: 35px; }");
    }
}
void Worker::goBackButton(int goB)
{
    switch (goB)
    {
        case 0 :ui->GoBack->setStyleSheet("QPushButton{ background-color : black; border : solid; font-size:13pt;"
                                                       "border: 1px outset white;height: 35px;"
                                                       "border-radius: 5px;}");
        break;
        case 1 : ui->GoBack->setStyleSheet("QPushButton{ background-color: red; border-color: red; font-size:13pt;height: 35px; }");
        break;
    }
}
void Worker::goButton(int go)
{
    switch (go)
    {
        case 0 :ui->GO->setStyleSheet("QPushButton{ background-color : black; font-size:15pt;"
                                                   "border: 1px outset white;height: 35px;"
                                                   "border-radius: 5px;}");
        break;
        case 1 : ui->GO->setStyleSheet("QPushButton{ background-color: red; border-color: red;font-size:15pt;height: 35px; }");
        break;
    }
}

void Worker::joystick(int value)
{   value = (255-(qRound(value*2.55)));
    Message msg("/joystick"); msg.pushInt32(value); sendOSC(msg);
}





//patch///////////////////////////////////////////////////////////////////////////////


void Worker::padPatch(int id)
{ QString id2 = QString("/patch/%1").arg(-id-2);
    Message msg(id2.toStdString()); msg.pushInt32(1); sendOSC(msg);
}

void Worker::padPointp()
{  Message msg("/patch/dot"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padclearp()
{ Message msg("/patch/clear"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padplusp()
{ Message msg("/patch/plus"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padmoinsp()
{ Message msg("/patch/moins"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padthrup()
{ Message msg("/patch/thru"); msg.pushInt32(1); sendOSC(msg); }

void Worker::padenterp()
{ Message msg("/patch/enter"); msg.pushInt32(1); sendOSC(msg); }

void Worker::patch()
{ Message msg("/patch/patch"); msg.pushInt32(1); sendOSC(msg); }

void Worker::unpatch()
{ Message msg("/patch/unpatch"); msg.pushInt32(1); sendOSC(msg); }

void Worker::patchchannel(bool on)
{
    if (on) {ui->dimmer->setChecked(false);
             ui->nDimmer->setEnabled(false);
             ui->nChannel->setEnabled(true);}

    Message msg("/patch/channel"); msg.pushInt32(1); sendOSC(msg);
}
void Worker::patchdimmer(bool on)
{
        if (on) {ui->padCh_2->setChecked(false);
                 ui->nDimmer->setEnabled(true);
                 ui->nChannel->setEnabled(false);
        }
    Message msg("/patch/dimmer"); msg.pushInt32(1); sendOSC(msg);
}

void Worker::prev()
{ Message msg("/patch/prev"); msg.pushInt32(1); sendOSC(msg); }

void Worker::next()
{ Message msg("/patch/next"); msg.pushInt32(1); sendOSC(msg); }

void Worker::channeltest(bool)
{ Message msg("/patch/testChannel"); msg.pushInt32(1); sendOSC(msg); }

void Worker::dimmertest(bool)
{ Message msg("/patch/testDimmer"); msg.pushInt32(1); sendOSC(msg); }

void Worker::testlevel(int level)
{ level = (255-(qRound(level*2.55)));
  Message msg("/patch/levelRequest"); msg.pushInt32(level); sendOSC(msg);
}

void Worker::affichelevel(int level)
{ level=qRound(level/2.55);
  ui->testLevel->setValue(level);
}

void Worker::errorINPatch(int ok)
{
    bool faux = ok;
    bool vrai = !faux;
    ui->dimmer->setEnabled(vrai);
    ui->padCh_2->setEnabled(vrai);
    ui->padClear_2->setEnabled(vrai);
    ui->padEnter_2->setEnabled(vrai);
    ui->channelTest->setEnabled(vrai);
    ui->padMoins_2->setEnabled(vrai);
    ui->dimmerTest->setEnabled(vrai);
    ui->padPlus_2->setEnabled(vrai);
    ui->padPREV->setEnabled(vrai);
    ui->padPoint_2->setEnabled(vrai);
    ui->padNEXT->setEnabled(vrai);
    ui->padPATCH->setEnabled(vrai);
    ui->padUNPATCH->setEnabled(vrai);
    ui->testLevel->setEnabled(vrai);
    ui->padTHRU_2->setEnabled(vrai);
    ui->p0->setEnabled(vrai);
    ui->p1->setEnabled(vrai);
    ui->p2->setEnabled(vrai);
    ui->p3->setEnabled(vrai);
    ui->p4->setEnabled(vrai);
    ui->p5->setEnabled(vrai);
    ui->p6->setEnabled(vrai);
    ui->p7->setEnabled(vrai);
    ui->p8->setEnabled(vrai);
    ui->p9->setEnabled(vrai);
    ui->nChannel->setEnabled(vrai);
    ui->nDimmer->setEnabled(vrai);
    ui->line->setEnabled(vrai);

    ui->yes_2->setEnabled(faux);
    ui->no_2->setEnabled(faux);
}

void Worker::yESp()
{ Message msg("/patch/errorACKYES"); msg.pushInt32(1); sendOSC(msg); }

void Worker::nOp()
{ Message msg("/patch/errorACKNO"); msg.pushInt32(1); sendOSC(msg); }







//autre///////////////////////////////////////////////////////////////////////////////

void Worker::startOSC()
{
    if ((ui->portTeleco->text().toInt()<1024)or(ui->portTeleco->text().toInt()>49152))
      {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Port"+ui->portTeleco->text()+ "not available");
            msgBox.setText("The D::Luz port MUST be >1024 and <49152");
            msgBox.exec();
            return;
        }
if (!multiCastOn)   {IpSend = QHostAddress(ipDlight);}

if ((isConnectedToNetwork()) & (!(ipAdrr == "no network")) & (!(ipAdrr == "")))
    {
    if (thread->isRunning())
        {
        listener->reStart();
         return;
}

    else
     thread->start();
}
}

void Worker::scan()
{
    if ((ui->portTeleco->text().toInt()<1024)or(ui->portTeleco->text().toInt()>49152))
      {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Port "+ui->portTeleco->text()+ " not available");
            msgBox.setText("The D::Luz port MUST be >1024 and <49152");
            msgBox.exec();
            return;
        }
    if ((ui->portDlight->text().toInt()<1024)or(ui->portDlight->text().toInt()>49152))
      {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Port "+ui->portDlight->text()+ " not available");
            msgBox.setText("The D::Light port MUST be >1024 and <49152");
            msgBox.exec();
            return;
        }

if ((isConnectedToNetwork()) & (!(ipAdrr == "no network")) & (!(ipAdrr == "")))
    {
    QStringList eclate = ipAdrr.split(".");
    QString plageIP = (eclate.at(0) + "." + eclate.at(1) + "." + eclate.at(2)+ "." );

    for (int i=1; i<257; i++)
         {
         QString i2 = QString::number(i);
         QString   ip = (plageIP + i2);
         string idTel = (ipAdrr.toStdString()+":"+PORT_NUMin.toStdString());
         QString appel = ("/request/login");
         Message msg(appel.toStdString());msg.pushStr(idTel);
         PacketWriter pw;
         pw.startBundle().addMessage(msg).endBundle();
         udpSocketSend->writeDatagram(pw.packetData(), pw.packetSize(), QHostAddress(ip), PORT_NUMsend.toInt());
          }
    }
}

void Worker::freshIp()
{
ui->iPTeleco->clear();
if (!isConnectedToNetwork())
    {ui->iPTeleco->addItem("no network");
    ipAdrr = ui->iPTeleco->currentText();}

    else
    {
    addr = QNetworkInterface::allAddresses();
    QStringList addresses;
    foreach (QHostAddress address, QNetworkInterface::allAddresses())
            {
            // Filtre les adresses localhost ...
            if(address != QHostAddress::LocalHostIPv6
            && address != QHostAddress::LocalHost
            // ... APIPA ...
            && !address.isInSubnet(QHostAddress::parseSubnet("169.254.0.0/16"))
            // ... Lien Local IPv6
            && !address.isInSubnet(QHostAddress::parseSubnet("FE80::/64")))
            addresses << address.toString();
            }
  foreach   (QString address, addresses)
            { ui->iPTeleco->addItem(address);}

  ipAdrr = ui->iPTeleco->currentText();}
}


bool Worker::isConnectedToNetwork()
{
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
     bool result = false;

    for (int i = 0; i < ifaces.count(); i++)
    {
        QNetworkInterface iface = ifaces.at(i);
        if ( iface.flags().testFlag(QNetworkInterface::IsUp)
             && !iface.flags().testFlag(QNetworkInterface::IsLoopBack) )
        {

            // this loop is important
            for (int j=0; j<iface.addressEntries().count(); j++)
            {
                if (result == false)
                    result = true;
            }
        }
    }

    return result;
}

void Worker::connexionIsOk(bool ok)
{
    if (ok)
    {
    ui->scanReseau->setStyleSheet("QPushButton {background: purple; color:black; border-color:fushia;}");
    ui->scanReseau->setText("OSC reboot");
    }
    if (!ok)
    {
    ui->scanReseau->setStyleSheet("QPushButton {background: black; color:white;}");
    ui->scanReseau->setText("go OSC");
    }
}

void Worker::castModeSelect()
{
    if (multiCastOn)
    {
        ui->labelIpT->setVisible(true);
        ui->refresh->setVisible(true);
        ui->iPTeleco->setVisible(true);
        ui->cadre1->setVisible(true);
        ui->labelIPDl->setVisible(true);
        ui->iPDLight->setVisible(true);
        ui->cadre2->setVisible(true);
        multiCastOn = false;
        ui->castButton->setText("Mode = UniCast");

        return;
    }
    else if (!multiCastOn)
    {
        ui->labelIpT->setVisible(false);
        ui->refresh->setVisible(false);
        ui->iPTeleco->setVisible(false);
        ui->cadre1->setVisible(false);
        ui->labelIPDl->setVisible(false);
        ui->iPDLight->setVisible(false);
        ui->cadre2->setVisible(false);
        multiCastOn = true;
        ui->castButton->setText("Mode = Multicast");
        IpSend = groupAddress;

    }
}

void Worker::answerDlight(QString answer, int)
{     
    QHostAddress myIP;
       if(!(myIP.setAddress( answer)))
      {
            QMessageBox msgBox;
            msgBox.setText("D::Light sent an invalid  Ip Adress. Try rebooting D::Light OSC server");
            msgBox.exec();
            premiereErreure=false;
            return;
        }

     ipDlight=answer;
     ui->iPDLight->setText(answer);
     startOSC();
     ui->checkConnected->setChecked(true);
     Message msg("/request/init");
     sendOSC(msg);
}
void Worker::supensionEtReveil(Qt::ApplicationState state)
{
    if (state==Qt::ApplicationInactive)
    {
        udpSocketSend->deleteLater();
    }
    if (state==Qt::ApplicationActive)
    {
        udpSocketSend = new QUdpSocket(this);
        freshIp();
        startOSC();
        scan();
        tabindex(ui->tabs->currentIndex());
    }
}

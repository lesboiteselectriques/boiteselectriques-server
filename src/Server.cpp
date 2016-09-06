#include "Server.h"

/**
 * @file Server.cpp
 * @brief Server implementation
 */

#include <QDebug>
#include <exception>

//#define CONFIG_FILE "~/.config/Boites Electriques/config.txt"
#define COMPANY_NAME "Rock & Chanson"
#define APP_NAME "Boites Electriques"

#define DEFAULT_EXTENSION "*.song" /*< Song files extension */
#define DEFAULT_FOLDER "/home/pi/songs/" /*< Files save/load folder*/

#define DEFAULT_IP "192.170.0.17"
#define DEFAULT_SENDER 9989
#define DEFAULT_RECEIVER 9988

#define DEFAULT_THRESHOLD 50 /*< Default threshold value */
#define DEFAULT_MASTER_VOLUME 50 /*< Default master volume value */

#define DEFAULT_VOLUME 50
#define DEFAULT_PAN 0
#define DEFAULT_ACTIVATION false

Server::Server(int& argc, char *argv[]):
    QCoreApplication(argc, argv){

    options = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                            COMPANY_NAME, APP_NAME);
    initConf(options);
    qDebug() << options->fileName();

    player = new PlayThread(options);

    options->beginGroup("osc");
    receiver = new OscReceiver(options->value("receiver").toInt());
    sender = new OscSender(options->value("ip").toString().toStdString(),
                     options->value("sender").toInt());

    options->endGroup();

    connect(player, &PlayThread::muteChanged,
            player, &PlayThread::setMute);

    connect(this, &Server::updateThreshold,
            player, &PlayThread::setThreshold);
    connect(this, &Server::resetThreshold,
            player, &PlayThread::resetThreshold);

    connect(player,	&PlayThread::actualBeatChanged,
            this,		&Server::updateBeat);
    connect(player,	&PlayThread::beatCountChanged,
            this,		&Server::updateBeatCount);

    connect(player,	&PlayThread::songLoaded,
            this,		&Server::onSongLoaded);
    connect(&saveManager,	&SaveManager::updatedTracksList,
            this,		&Server::updateTrackList);

    connect(&serialManager,	&SerialManager::boxActivated,
            this,			&Server::switchBox);
    connect(&serialManager,	&SerialManager::boxActivated,
            this,			&Server::sendBoxActivation);

    serialManager.start();

    //Client events
    receiver->addHandler("/box/update_threshold",
                        std::bind(&Server::handle__box_updateThreshold,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/reset_threshold",
                        std::bind(&Server::handle__box_resetThreshold,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/enable",
                        std::bind(&Server::handle__box_enable,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/volume",
                        std::bind(&Server::handle__box_volume,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/pan",
                        std::bind(&Server::handle__box_pan,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/mute",
                        std::bind(&Server::handle__box_mute,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/solo",
                        std::bind(&Server::handle__box_solo,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/master",
                        std::bind(&Server::handle__box_master,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/play",
                        std::bind(&Server::handle__box_play,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/stop",
                        std::bind(&Server::handle__box_stop,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/reset",
                        std::bind(&Server::handle__box_reset,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/refresh_song",
                        std::bind(&Server::handle__box_refreshSong,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/select_song",
                        std::bind(&Server::handle__box_selectSong,
                                  this, std::placeholders::_1));
    receiver->addHandler("/box/sync",
                        std::bind(&Server::handle__box_sync,
                                  this, std::placeholders::_1));

    receiver->run();
}

Server::~Server() {
    qDebug() << "Quitting server...";
    stop();
    player->stop();
    qDebug() << "PlayThread stopping...";
    if(!player->wait(5000)) {
        qWarning("PlayThread : Potential deadlock detected !! Terminating...");
        player->terminate();
        player->wait(5000);
        player->exit(1);
    }

    qDebug() << "SerialManager stopping...";
    serialManager.stop();
    if(!serialManager.wait(5000)) {
        qWarning("SerialManager : Potential deadlock detected !! Terminating...");
        serialManager.terminate();
        serialManager.wait(5000);
        serialManager.exit(1);
    }

    qDebug() << "Deleting pointers...";
    delete player;
    delete options;

    qDebug() << "Proper quitting OK";

    QCoreApplication::quit();
}

int Server::getTempo() const {
    return m_tempo;
}

unsigned int Server::getThreshold() const {
    return 99 - (player->getThreshold()-100)/4;
}

bool Server::initConf(QSettings *c) {
    QFile f(c->fileName());

    if(!f.exists()){
        c->beginGroup("default");
        c->setValue("threshold", DEFAULT_THRESHOLD);
        c->setValue("master", DEFAULT_MASTER_VOLUME);
        c->setValue("volume", DEFAULT_VOLUME);
        c->setValue("pan", DEFAULT_PAN);
        c->setValue("activation", DEFAULT_ACTIVATION);
        c->endGroup();

        c->beginGroup("files");
        c->setValue("folder", DEFAULT_FOLDER);
        c->setValue("extension", DEFAULT_EXTENSION);
        c->endGroup();

        c->beginGroup("osc");
        c->setValue("ip", DEFAULT_IP);
        c->setValue("sender", DEFAULT_SENDER);
        c->setValue("receiver", DEFAULT_RECEIVER);
        c->endGroup();

        return false;
    } else
        return true;

}

void Server::sendReady(bool isReady) {
    sendMsgReady(isReady);
}

void Server::sendThreshold() {
    sendMsgThreshold(getThreshold());
}

void Server::sendActivatedTracks() {
    sendMsgActivatedTracks(player->getActivatedTracks());
}

void Server::sendPlay()
{
    sendMsgPlay(getTempo());
}

void Server::sendTracksCount() {
    sendMsgTracksCount(player->getTracksCount());
}

void Server::sendBoxActivation(unsigned int i, int val) {
    if(val >= player->getThreshold()){
        sendMsgBoxActivation(i);
    }
}

void Server::sendBeatCount(unsigned int i) {
    sendMsgBeatCount(i);
}

void Server::sendSongsList() {
    options->beginGroup("files");
    QDir exportFolder(options->value("folder").toString());

    exportFolder.setNameFilters(QStringList()<<options->value("extension").toString());
    QStringList fileList = exportFolder.entryList();
    QString str = fileList.join("|");
    QByteArray list = str.toLatin1();
    const char *c_list = list.data();

    options->endGroup();

    sendMsgSongsList(c_list);
}

void Server::sendSongTitle() {
    QString raw_title = QString::fromStdString(song.name);
    QByteArray title = raw_title.toLatin1();
    const char *c_title = title.data();

    sendMsgSongTitle(c_title);
}

void Server::handle__box_updateThreshold(osc::ReceivedMessageArgumentStream args) {
    osc::int32 senseUpdated;
    args >> senseUpdated;
    qDebug() << "received /box/update_threshold" << senseUpdated;

    emit updateThreshold(senseUpdated);
}

void Server::handle__box_resetThreshold(osc::ReceivedMessageArgumentStream args) {
    osc::int32 senseUpdated;
    args >> senseUpdated;
    qDebug() << "received /box/reset_threshold" << senseUpdated;

    emit resetThreshold();
}

void Server::handle__box_enable(osc::ReceivedMessageArgumentStream args) {
    osc::int32 box;
    args >> box;
    qDebug() << "received /box/enable" << box;

    switchBox(box, player->getThreshold());
}

void Server::handle__box_volume(osc::ReceivedMessageArgumentStream args) {
    osc::int32 box;
    osc::int32 vol;
    args >> box >> vol;
    qDebug() << "received /box/volume" << vol;

    player->setVolume(box, vol);
}

void Server::handle__box_pan(osc::ReceivedMessageArgumentStream args) {
    osc::int32 box;
    osc::int32 vol;
    args >> box >> vol;
    qDebug() << "received /box/pan" << box << vol;

    player->setPan(box, vol);
}

void Server::handle__box_mute(osc::ReceivedMessageArgumentStream args) {
    osc::int32 box;
    bool state;
    args >> box >> state;
    qDebug() << "received /box/mute" << box << state;

    player->setMute(box, state);
}

void Server::handle__box_solo(osc::ReceivedMessageArgumentStream args) {
    osc::int32 box;
    bool state;
    args >> box >> state;
    qDebug() << "received /box/solo" << box << state;

    player->solo(box, state);
}

void Server::handle__box_master(osc::ReceivedMessageArgumentStream args) {
    osc::int32 vol;
    args >> vol;
    qDebug() << "received /box/master" << vol;

    player->setMasterVolume(vol);
}

void Server::handle__box_play(osc::ReceivedMessageArgumentStream args) {
    bool state;
    args >> state;
    qDebug() << "received /box/play" << state;

    play();
}

void Server::handle__box_stop(osc::ReceivedMessageArgumentStream args) {
    bool state;
    args >> state;
    qDebug() << "received /box/stop" << state;

    stop();
}

void Server::handle__box_reset(osc::ReceivedMessageArgumentStream args) {
    bool state;
    args >> state;
    qDebug() << "received /box/reset" << state;

    if(state)
        stop();

    player->reset();
}

void Server::handle__box_refreshSong(osc::ReceivedMessageArgumentStream args) {
    bool state;
    args >> state;
    qDebug() << "received /box/refresh_song" << state;

    load();
}

void Server::handle__box_selectSong(osc::ReceivedMessageArgumentStream args) {
    const char *receptSong;
    args >> receptSong;
    qDebug() << "received /box/select_song" << receptSong;

    QString so = QString(QLatin1String(receptSong));
    if(!so.isEmpty())
        selSong = so;

    connect(this, SIGNAL(actionLoad()),
            this, SLOT(load()) );
    emit actionLoad();
}

void Server::handle__box_sync(osc::ReceivedMessageArgumentStream args)
{
    bool state;
    args >> state;
    qDebug() << "received /box/sync" << state;

    sendSongsList();
    sendThreshold();

    sendSongTitle();
    sendTracksCount();
    sendActivatedTracks();
}

void Server::reset() {
    stop();

    player->reset();
}

void Server::switchBox(unsigned int i, int val) {
    if(val >= player->getThreshold()) {
        player->switchBox(i);
    }
}

void Server::play() {
    if(!m_loaded) {
        if(load()) return;
    }

    sendPlay();
    player->start();
    m_playing = true;
}

void Server::stop() {
    if(!m_loaded || !m_playing) return;

    player->stop();
    m_previousBeat = 0;
    m_playing = false;

    updateBeat(0);
}

void Server::updateBeat(double t) { // in seconds

    int time = (int)(t * getTempo() / 60.0f) + 1;

    if(time != m_previousBeat && time <= m_beatCount && !player->isStopped()) {
        m_previousBeat = time;

        sendBeatCount(time);
        if(time == 8 || time == 16 || time == 24){ //sync
            sendActivatedTracks();
            sendBeatCount(time);
        }
    } else if(player->isStopped()) {
        m_previousBeat = 0;
        sendBeatCount(0);
    }
}

void Server::updateBeatCount(double t) { // in seconds
    // Here we calculate the number of beats in the loop
    // Formula : seconds * tempo/60 = nb. beats in loop.

    m_beatCount = t * getTempo() / 60.0;
    m_previousBeat = -1;
}

void Server::onSongLoaded(unsigned int on, unsigned int max) {
    if (on == max)
        sendReady(true);
}

void Server::updateTrackList(const char *list) {
    sendMsgTracksList(list);
}

void Server::setTempo(unsigned int arg) {
    m_tempo = arg;
}

void Server::sendMsgThreshold(int t) {
    sender->send(osc::MessageGenerator()("/box/sensor", t));
    qDebug() << "sent /box/sensor" << t;
}

void Server::sendMsgBoxActivation(int chan) {
    sender->send(osc::MessageGenerator()("/box/enable_out", chan));
    qDebug() << "sent /box/enable_out" << chan;
}

void Server::sendMsgActivatedTracks(int tracks) {
    sender->send(osc::MessageGenerator()("/box/enable_sync", tracks));
    qDebug() << "sent /box/enable_sync" << tracks;
}

void Server::sendMsgBeatCount(int beat) {
    sender->send(osc::MessageGenerator()("/box/beat", beat));
    qDebug() << "sent /box/beat" << beat;
}

void Server::sendMsgPlay(int tempo) {
    sender->send(osc::MessageGenerator()("/box/play", tempo));
    qDebug() << "sent /box/play" << tempo;
}

void Server::sendMsgSongTitle(const char *title) {
    sender->send(osc::MessageGenerator()("/box/title", title));
    qDebug() << "sent /box/title" << title;
}

void Server::sendMsgSongsList(const char *list) {
    sender->send(osc::MessageGenerator()("/box/songs_list", list));
    qDebug() << "sent /box/songs_list" << list;
}

void Server::sendMsgTracksCount(int num) {
    sender->send(osc::MessageGenerator()("/box/tracks_count", num));
    qDebug() << "sent /box/tracks_count" << num;
}

void Server::sendMsgTracksList(const char *list) {
    sender->send(osc::MessageGenerator()("/box/tracks_list", list));
    qDebug() << "sent /box/tracks_list" << list;
}

void Server::sendMsgReady(bool isReady) {
    sender->send(osc::MessageGenerator()("/box/ready", isReady));
    qDebug() << "sent /box/ready" << isReady;
}

int Server::load() {
    sendSongsList();
    sendThreshold();

    if (!selSong.isEmpty()) {
        try {
            stop();

            QString file = EXPORT_FOLDER + selSong;

            if(!file.isEmpty()) {
                currentFile = file;
                song = saveManager.load(file);

                setTempo(song.tempo);
                player->load(song);

                m_loaded = true;
                sendSongTitle();

                sendTracksCount();

                return 0;
            }
            return 1;
        } catch(std::exception& e) {
            qCritical() << tr("LOADING ERROR :") << e.what();
            return 1;
        }
    }//end check selsong
    return 1;
}

void Server::save() {
    try {
        if(m_loaded)
            saveManager.save(currentFile, this);
    } catch(std::exception& e) {
        qCritical() << tr("SAVING ERROR :") << e.what();
    }
}
